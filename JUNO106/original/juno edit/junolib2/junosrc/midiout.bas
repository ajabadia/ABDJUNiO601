' $Header:   D:/misc/midi/vcs/midiout.bas   1.4   25 Oct 1994 10:18:38   DAVEC  $

' Public domain by David Churcher. No rights reserved. Use at your own risk.

Option Explicit

Declare Function midiOutOpen Lib "mmsystem" (hMidiOut As Integer, ByVal deviceId As Integer, ByVal C As Long, ByVal I As Long, ByVal F As Long) As Integer
Declare Function midiOutShortMsg Lib "mmsystem" (ByVal hMidiOut As Integer, ByVal MidiMessage As Long) As Integer
Declare Function midiOutLongMsg Lib "mmsystem" (ByVal hMidiOut As Integer, ByVal lpMidiOutHdr As Long, ByVal uSize As Integer) As Integer
Declare Function midiOutClose Lib "mmsystem" (ByVal hMidiOut As Integer) As Integer
Declare Function midiOutReset Lib "mmsystem" (ByVal hMidiOut As Integer) As Integer
Declare Function midiOutGetErrorText Lib "mmsystem" (ByVal uError As Integer, ByVal lpText As String, ByVal uSize As Integer) As Integer
Declare Function midiOutPrepareHeader Lib "mmsystem" (ByVal hMidiOut As Integer, ByVal lpMidiOutHdr As Long, ByVal uSize As Integer) As Integer
Declare Function midiOutUnPrepareHeader Lib "mmsystem" (ByVal hMidiOut As Integer, ByVal lpMidiOutHdr As Long, ByVal uSize As Integer) As Integer

Dim midiOutHeader As vbMIDIHDR

'-----------------------------------------------------------------------
' Send a MIDI message
' Returns: 0 if OK or MIDI error code
'  hMidiOut: Output handle
'  sMidiData: Data to send

' Note: This will only send one short MIDI message per call, since it
' doesn't do the decoding necessary to determine the length of each
' MIDI message within the string. It assumes that the length of the string
' passed is the length of the short MIDI message.
' If the MIDI data begins with the MIDI_BEGINSYSEX (F0) code it assumes
' the data is System Exclusive and sends it using vbMidiOutLongMsg.

Function MidiPut (hMidiOut As Integer, sMidiData As String)

	Dim nRetCode As Integer, nLenMsg As Integer, lMidiMessage As Long
	Dim byte1 As Integer, byte2 As Integer, byte3 As Integer

	If Asc(sMidiData) = MIDI_BEGINSYSEX Then
		nRetCode = vbMidiOutLongMsg(hMidiOut, sMidiData)
	Else
		nLenMsg = Len(sMidiData)
		If nLenMsg > 3 Then
			' Just send out the whole thing
			nRetCode = vbMidiOutLongMsg(hMidiOut, sMidiData)
		Else
			byte1 = Asc(sMidiData)
			If nLenMsg > 1 Then
				byte2 = Asc(Mid$(sMidiData, 2, 1))
				If nLenMsg > 2 Then
					byte3 = Asc(Mid$(sMidiData, 3, 1))
				End If
			End If
			lMidiMessage = CLng(byte1) Or (CLng(byte2) * &H100) Or (CLng(byte3) * &H10000)

			'-----------------------------------------------------------
			' This is a copy of vbMidiOutShortMsg(), inlined for speed
			nRetCode = midiOutShortMsg(hMidiOut, lMidiMessage)
			If nRetCode <> 0 Then
				Call vbMidiOutError(nRetCode, "vbMidiOutShortMsg")
			End If
			' End of vbMidiOutShortMsg
			'-----------------------------------------------------------

		End If
	End If

	MidiPut = nRetCode

End Function

'-----------------------------------------------------------------------
' Close the output device
' Warning: waits until everything has finished playing before returning
Function vbMidiOutClose (ByVal hMidiOut As Integer) As Integer

    Dim nRetCode As Integer

    Do While DoEvents()
	nRetCode = midiOutClose(hMidiOut)
	If nRetCode = 0 Or nRetCode <> MIDIERR_STILLPLAYING Then
	    Exit Do
	End If
    Loop
    Call vbMidiOutError(nRetCode, "vbMidiOutClose")

    vbMidiOutClose = nRetCode

End Function

'-----------------------------------------------------------------------
' Display MIDI output error
Private Sub vbMidiOutError (ByVal nError As Integer, sRoutine As String)
	
	Dim sErrText As String * 160
	Dim nRetCode As Integer

	If nError <> 0 Then
		sErrText = "Unknown error"
		nRetCode = midiOutGetErrorText(nError, sErrText, 160)
		nRetCode = MsgBox(sRoutine + ": " + sErrText)
	End If

End Sub

'-----------------------------------------------------------------------
' Send a long MIDI message
' Returns: 0 if OK, MIDI error code if error
' DEBUG: Check that you don't need to send the F0 byte as a separate short message
' Warning: Will wait 'till the message has been sent before returning
Function vbMidiOutLongMsg (hMidiOut As Integer, sMsgData As String) As Integer

	Dim nRetCode As Integer

	If sMsgData <> "" Then

		' Set up the vbMIDIHDR and allocate the output buffer
		nRetCode = vbMidiOutPrepareHeader(hMidiOut, Len(sMsgData))
		If nRetCode = 0 Then

			' Copy the midi output data to the output buffer
			Call hmemcpy(midiOutHeader.midiheader.lpdata, sMsgData, Len(sMsgData))
		
			' Send it
			nRetCode = midiOutLongMsg(hMidiOut, midiOutHeader.hdrPointer, LENMIDIHDR)
			If nRetCode = 0 Then
			' Wait for the device driver to finish sending it.
			' Note tried Do While DoEvents() here but if lots of sysex input messages
			' arrive at once when sysex messages are being echoed to output the calls
			' to midiInputArrived stack up and you eventually run out of stack space.
			' Could put a timeout here to make sure it doesn't hang.
			'
			' The proper asynchronous way to do this is to allocate a new midiheader
			' for each output message and have a separate routine triggered by the
			' MM_MOM_DONE message to deallocate the header. Would require a lot more
			' code though.
			Do
				Call midiHeaderUpdate(midiOutHeader)
				If (midiOutHeader.midiheader.dwFlags And MHDR_DONE) = 1 Then
				Exit Do
				End If
			Loop

			End If

			nRetCode = vbMidiOutUnprepareHeader(hMidiOut, midiOutHeader)

		End If
		If nRetCode <> 0 Then

			Call vbMidiOutError(nRetCode, "vbMidiOutLongMsg")
	
		End If

	End If

	vbMidiOutLongMsg = nRetCode

End Function

'-----------------------------------------------------------------------
' Open a MIDI device for output and add a Sysex buffer to it
' Sets sysex buffer globals (see vbMidiOutPrepareHeader)
' Returns MIDI input handle or -1 if error
'
' Note: could pass the blaster control to this routine and trap the
' MIDI output messages, but the only useful one is MM_MOM_DONE and you
' can detect that by monitoring the flags in the MIDIHDR structures.
Function vbMidiOutOpen (ByVal hMsgWnd As Integer, ByVal nDeviceID As Integer) As Integer

	Dim nRetCode As Integer, hMidiOut As Integer

	' Open the MIDI device for output
	nRetCode = midiOutOpen(hMidiOut, nDeviceID, CLng(hMsgWnd), 0&, CALLBACK_WINDOW)
	If nRetCode <> 0 Then
	    hMidiOut = -1
	    Call vbMidiOutError(nRetCode, "vbMidiOutOpen")
	End If

	' Return the device handle, or -1 if error
	If nRetCode = 0 Then
	    vbMidiOutOpen = hMidiOut
	Else
	    vbMidiOutOpen = -1
	End If

End Function

'-----------------------------------------------------------------------
' Allocate sysex buffers and prepare the headers for vbmidiOutAddBuffer
' Returns: 0 if OK, MIDI error code if error, -1 if memory allocation error
' Sets globals:
'  midiOutHeaders() array contents
Private Function vbMidiOutPrepareHeader (hMidiOut As Integer, lBufSize As Long) As Integer

    Dim nRetCode As Integer

    ' Prepare the header
    nRetCode = midiHeaderInit(midiOutHeader, lBufSize)
    If nRetCode = 0 Then

	nRetCode = midiOutPrepareHeader(hMidiOut, midiOutHeader.hdrPointer, LENMIDIHDR)
	Call vbMidiOutError(nRetCode, "vbMidiOutPrepareHeader")

	' Put copy of prepared header back into the VB structure
	Call midiHeaderUpdate(midiOutHeader)
    
    End If

    vbMidiOutPrepareHeader = nRetCode

End Function

'-----------------------------------------------------------------------
' Send a short message to MIDI output.
' Returns: 0 if OK or MIDI error code
' hMidiOut: Output handle
' MidiMessage: Message to send. First byte of message is low byte of Long, etc.
Function vbMidiOutShortMsg (hMidiOut As Integer, MidiMessage As Long) As Integer

    Dim nRetCode As Integer

    nRetCode = midiOutShortMsg(hMidiOut, MidiMessage)
    Call vbMidiOutError(nRetCode, "vbMidiOutShortMsg")

    vbMidiOutShortMsg = nRetCode

End Function

'-----------------------------------------------------------------------
' Unprepare an output header and deallocate its memory
' Returns: 0 if OK or MIDI error code
Private Function vbMidiOutUnprepareHeader (hMidiOut, theheader As vbMIDIHDR)

	Dim nRetCode As Integer

	If theheader.hdrPointer <> 0 And theheader.midiheader.lpdata <> 0 Then

		nRetCode = midiOutUnPrepareHeader(hMidiOut, theheader.hdrPointer, LENMIDIHDR)
		If nRetCode = 0 Then

			Call midiHeaderDeInit(theheader)
			vbMidiOutUnprepareHeader = 0

		Else

			Call vbMidiOutError(nRetCode, "vbMidiOutUnPrepareHeader")
			nRetCode = -1

		End If
	
	End If

	vbMidiOutUnprepareHeader = nRetCode

End Function

