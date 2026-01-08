' $Header:   D:/misc/midi/vcs/midiin.bas   1.12   29 Nov 1994 21:43:30   DAVEC  $

' Public domain by David Churcher. No rights reserved. Use at your own risk.

Option Explicit


' Number of headers (MIDIHDR) to allocate
Const DEFSYSEXINHEADERCOUNT% = 4
' Size of each low-level buffer attached to each MIDIHDR
Const DEFSYSEXINHEADERSIZE% = 10000
' Globals used to override the above two values, see sysexInParameters
Global gnSysexInHeaderCount As Integer
Global gnSysexInHeaderSize As Integer

' Set of MIDIHDR structures used for sysex input.
Global stMidiInHeaders() As vbMIDIHDR

' Size of each buffer on the MIDIHDRs (total memory allocated is MIDIINHDRDATASIZE * SYSEXINHEADERCOUNT
Const MIDIINHDRDATASIZE = 10000

' Max size of application's MIDI input buffer
' Not all allocated at start but should be set to prevent buffer exceeding
' maximum size of a VB string
Const MIDIINBUFFERMAXLEN = 32000

' MIDI ans sysex input buffers, initially zero length, extended by midiShortInHandler and midiLongInHandler
Dim stMidiInBuffer As String, stMidiInSysexBuffer As String

' MMSYSTEM input functions
Declare Function midiInOpen Lib "mmsystem.dll" (hMidiIn As Integer, ByVal nDeviceId As Integer, ByVal dwCallback As Long, ByVal dwCallbackInstance As Long, ByVal dwFlags As Long) As Integer
Declare Function midiInReset Lib "mmsystem" (ByVal hMidiIn As Integer) As Integer
Declare Function midiinclose Lib "mmsystem" (ByVal hMidiIn As Integer) As Integer
Declare Function midiInStart Lib "mmsystem" (ByVal hMidiIn As Integer) As Integer
Declare Function midiInStop Lib "mmsystem" (ByVal hMidiIn As Integer) As Integer
Declare Function midiInPrepareHeader Lib "mmsystem" (ByVal hMidiIn As Integer, ByVal lpMidiInHdr As Long, ByVal wSize As Integer) As Integer
Declare Function midiInUnprepareHeader Lib "mmsystem" (ByVal hMidiIn As Integer, ByVal lpMidiInHdr As Long, ByVal wSize As Integer) As Integer
Declare Function midiInAddBuffer Lib "mmsystem" (ByVal hMidiIn As Integer, ByVal lpMidiInHdr As Long, ByVal wSize As Integer) As Integer
Declare Function midiInGetErrorText Lib "mmsystem" (ByVal wError As Integer, ByVal lpText As String, ByVal wSize As Integer) As Integer

'-----------------------------------------------------------------------
' Handle sysex input messages (MM_MIM_LONGDATA)
' Copies sysex input data (if any) to the global input buffer
' wParam: handle to MIDI device that received the data
' lParam: pointer to the MIDIHDR structure containing the long message data
' Notes: These routines only support one MIDI input device at a time so
' we ignore wParam.
Sub midiLongInHandler (wParam As Integer, lParam As Long)

	Dim iHdr As Integer, nRetcode As Integer, lBytesRecorded As Long
	Dim sNewSysexData As String

	' Find the matching entry in the global stMidiInHeaders array

	'----------------------------------------------
	' Note: this was the findMidiInHeader function (see revision 1.9)
	' It's been inlined to improve the speed
	' Todo: Write an ASCAN() VBX function
	Dim bFound As Integer

	bFound = 0
	For iHdr = 1 To gnSysexInHeaderCount
		If stMidiInHeaders(iHdr).hdrPointer = lParam Then
			bFound = 1
			Exit For
		End If
	Next

	If bFound = 0 Then
		' Didn't find it
		iHdr = 0
	End If
	' End of findMidiInHeader
	'----------------------------------------------

	If iHdr = 0 Then
		' Debug.Print "midiLongInHandler: findMidiInHeader couldn't find " + Hex(lParam)
		Exit Sub
	Else
		' Update VB's copy of the MIDIHDR from the copy in global memory
		Call midiHeaderUpdate(stMidiInHeaders(iHdr))
		' Debug.Print "MM_MIM_LONGDATA: " + Str$(stMidiInHeaders(iHdr).midiheader.dwbytesrecorded) + " bytes, flags=" + Str$(stMidiInHeaders(iHdr).midiheader.dwFlags) + ", MIDIHDR address=" + Hex(lParam)
	End If

	lBytesRecorded = stMidiInHeaders(iHdr).midiheader.dwbytesrecorded
	If lBytesRecorded = 0 Then

		' This is a workaround for a bug in older versions of the
		' SoundBlaster Pro MIDI drivers. Updated drivers are available
		' from ftp.creaf.com.
	    ' The buggy MIDI device driver sends a 0-length LongMsg at the
	    ' start of a sysex input message, and doesn't send the F0 byte.
	    ' Use this message to add the F0 sysex start code to the sysex data buffer
	    sNewSysexData = Chr$(MIDI_BEGINSYSEX)

	Else

	    ' Copy the data from the global buffer in the midiheader to a string
	    sNewSysexData = Space(lBytesRecorded)
	    Call hmemcpy(sNewSysexData, stMidiInHeaders(iHdr).midiheader.lpdata, lBytesRecorded)
	    
	End If

	' Add to input buffer
	'----------------------------------------------------
	' Note: this was the newMidiInput Sub (see rev 1.9)
	' It's been inlined for speed
	' Check for buffer overflow
	If Len(stMidiInSysexBuffer) + Len(sNewSysexData) > MIDIINBUFFERMAXLEN Then
		' Just trash current contents!
		Debug.Print "Midi buffer overflow!"
		stMidiInSysexBuffer = ""
	End If

	stMidiInSysexBuffer = stMidiInSysexBuffer & sNewSysexData
	' End of newMidiInput sub
	'----------------------------------------------------

	' Reset the buffer and put it back on the input handle
	nRetcode = midiInPrepareHeader(gInputDev.Handle, lParam, LENMIDIHDR)
	If nRetcode > 0 Then
		Call vbMidiInError(nRetcode, "midiInPrepareHeader")
	Else
		nRetcode = vbMidiInAddBuffer(gInputDev.Handle, lParam)
	End If

End Sub

' Purpose: Start MIDI input on the specified device
' Returns: 0 if successful or MIDI error code if failed
'   hMidiIn: MIDI input handle
'   ctlBlaster: MessageBlaster control
'   bShortEnabled: TRUE to enable short input, FALSE for long (sysex) input only
' Notes: If you always want short input, just call midiInStart directly.
' Added the option to disable short input to hopefully reduce the load caused by
' the large number of short messages produced by MIDI timing and active sensing
' messages.
Function vbMidiInStart (hMidiIn As Integer, ctlBlaster As Control, bShortEnabled As Integer)

    Dim nRetcode As Integer

    ' Set or clear the trap for the short MIDI input messages
    ' See also MidiMessageTrap
    If bShortEnabled Then
	ctlBlaster.MsgList(3) = MM_MIM_DATA
    Else
	ctlBlaster.MsgList(3) = 0
    End If
    
    nRetcode = midiInStart(hMidiIn)
    If nRetcode <> 0 Then
	Call vbMidiInError(nRetcode, "vbMidiInStart")
    End If

    vbMidiInStart = nRetcode

End Function

'
'-----------------------------------------------------------------------
' Unprepare an input header and deallocate its buffer memory
' Returns 0 if OK, -1 if error
Function vbMidiInUnprepareHeaders (hMidiIn As Integer) As Integer

	Dim nRetcode As Integer
	Dim nHdrCtr As Integer
	Dim thishdr As vbMIDIHDR

	For nHdrCtr = 1 To gnSysexInHeaderCount

		thishdr = stMidiInHeaders(nHdrCtr)

		If thishdr.hdrPointer <> 0 And thishdr.midiheader.lpdata <> 0 Then  ' If a header has been prepared, unprepare it

			nRetcode = midiInUnprepareHeader(hMidiIn, thishdr.hdrPointer, LENMIDIHDR)
			If nRetcode = 0 Then

				Call midiHeaderDeInit(stMidiInHeaders(nHdrCtr))
				vbMidiInUnprepareHeaders = 0

			Else

				Call vbMidiInError(nRetcode, "midiInUnPrepareHeader")
				vbMidiInUnprepareHeaders = -1

			End If
	
		End If

	Next

End Function

'-----------------------------------------------------------------------
' Get the current midi input buffer
' Returns midi input data as string, or "" if buffer empty
Function MidiGet () As String

	MidiGet = stMidiInBuffer
	stMidiInBuffer = ""

End Function

'-----------------------------------------------------------------------
' Handle MIDI short input messages (MM_MIM_DATA)
' Copies input data to the midi input buffer
' wParam: handle to MIDI device that received the message
' lParam: message itself, first byte of message in low-order byte
Sub midiShortInHandler (wParam As Integer, lParam As Long)

	Dim sMessage As String, nStatusByte As Integer

	' MIDI message type is first byte of message
	nStatusByte = lParam And &HFF&
	
	' Remove channel information (lower 4 bits) from
	' the message
	Select Case (nStatusByte And &HF0)

	' 3-byte messages
	Case MIDI_NOTEON, MIDI_NOTEOFF, MIDI_KEYAFTERTOUCH, MIDI_CONTROLCHANGE, MIDI_PITCHBEND
		sMessage = Chr$(nStatusByte) & Chr$((lParam \ &H100&) And &HFF) & Chr$((lParam \ &H10000) And &HFF)

	Case MIDI_SYSTEMMESSAGE

		Select Case nStatusByte

		' 3-byte system messages
		Case MIDI_SONGPOSPTR
			sMessage = Chr$(nStatusByte) & Chr$((lParam \ &H100&) And &HFF&) & Chr$((lParam \ &H10000) And &HFF)

		' 2-byte system messages
		Case MIDI_MTCQUARTERFRAME, MIDI_SONGSELECT
			sMessage = Chr$(nStatusByte) & Chr$((lParam \ &H100) And &HFF&)

		' 1-byte system messages
		Case MIDI_TIMINGCLOCK, MIDI_START, MIDI_CONTINUE, MIDI_STOP, MIDI_ACTIVESENSING, MIDI_SYSTEMRESET
			sMessage = Chr$(nStatusByte)

		Case Else
			' No idea what this MIDI message is so don't process it
			' ' Debug.Print "Got unknown MIDI message " & Hex(lParam And &HFF)
			sMessage = ""

		End Select

	Case Else
		' No idea what this MIDI message is so don't process it
		' Debug.Print "Got unknown MIDI message " & Hex$(nStatusByte)
		sMessage = ""

	End Select


	' Note this was the newMidiInput Sub (see rev 1.9)
	' It's been inlined for speed
	' Check for buffer overflow
	If Len(stMidiInBuffer) + Len(sMessage) > MIDIINBUFFERMAXLEN Then
		' Just trash current contents!
		' Debug.Print "Short Midi buffer overflow!"
		stMidiInBuffer = ""
	End If

	stMidiInBuffer = stMidiInBuffer & sMessage

End Sub

'-----------------------------------------------------------------------
' Get the current midi sysex input buffer
' Returns sysex input data as string, or "" if buffer empty
Function midiSysexGet () As String

	midiSysexGet = stMidiInSysexBuffer
	stMidiInSysexBuffer = ""

End Function

'-----------------------------------------------------------------------
' Set globals for default number and size of sysex input buffers.
' Don't call this while a MIDI input device is open because it clears the
' MIDIHDR array
Sub sysexInParameters (nSysexInHeaderCount As Integer, nSysexInHeaderSize As Integer)

	' Apply defaults if they aren't set already
	If gnSysexInHeaderCount = 0 Then
		gnSysexInHeaderCount = DEFSYSEXINHEADERCOUNT
	End If
	If gnSysexInHeaderSize = 0 Then
		gnSysexInHeaderSize = DEFSYSEXINHEADERSIZE
	End If

	' Now change values, if required
	If nSysexInHeaderCount > 0 Then
		gnSysexInHeaderSize = nSysexInHeaderSize
	End If
	If nSysexInHeaderCount > 0 Then
		gnSysexInHeaderCount = nSysexInHeaderCount
	End If

	ReDim stMidiInHeaders(gnSysexInHeaderCount) As vbMIDIHDR

End Sub

'-----------------------------------------------------------------------
' Add a buffer to a midi input device.
' Returns: 0 if OK, API error code if error
Private Function vbMidiInAddBuffer (hMidiIn As Integer, lpMidiHdr As Long)

	Dim nRetcode As Integer
	
	nRetcode = midiInAddBuffer(hMidiIn, lpMidiHdr, LENMIDIHDR)
	Call vbMidiInError(nRetcode, "vbMidiInAddBuffer")
	vbMidiInAddBuffer = nRetcode

End Function

'-----------------------------------------------------------------------
' Closes specified MIDI input device
' Returns 0 if OK, error code (as below) if error
Function vbMidiInClose (ByVal hMidi As Integer) As Integer

	Dim nRetcode As Integer

	' Stop all input, and mark pending buffers as done
	nRetcode = midiInReset(hMidi)
	Call vbMidiInError(nRetcode, "vbMidiInClose")

	' Unprepare and release the Sysex buffers
	nRetcode = vbMidiInUnprepareHeaders(hMidi)
	Call vbMidiInError(nRetcode, "vbMidiInClose")

	nRetcode = midiinclose(hMidi)
	Call vbMidiInError(nRetcode, "vbMidiInClose")

	vbMidiInClose = nRetcode

End Function

'-----------------------------------------------------------------------
Private Sub vbMidiInError (ByVal wError As Integer, routine As String)

	Dim sErrText As String * 160
	Dim nRetcode As Integer

	If wError <> 0 Then
		sErrText = "Unknown error"
		nRetcode = midiInGetErrorText(wError, sErrText, 160)
		nRetcode = MsgBox(routine + ": " + sErrText)
	End If

End Sub

'-----------------------------------------------------------------------
' Open a MIDI device for input and add sysex buffers to it
' Sets sysex buffer globals (see vbMidiInPrepareHeaders)
' Returns MIDI input handle or -1 if error
' Note: Must call midiInStart to start input
Function vbMidiInOpen (ByVal hMsgWnd As Integer, ByVal nDeviceId As Integer, blasterControl As Control, notifyControl As Control) As Integer

	Dim nRetcode As Integer, hMidiIn As Integer, nHdrCtr As Integer

	' Set default number and size of low-level input buffers
	Call sysexInParameters(0, 0)

	' Catch MIDI input messages for the specified window
	Call midiMessageTrap(hMsgWnd, blasterControl)

	' Open the MIDI device
	nRetcode = midiInOpen(hMidiIn, nDeviceId, CLng(hMsgWnd), 0&, CALLBACK_WINDOW)
	If nRetcode <> 0 Then
		hMidiIn = -1
		Call vbMidiInError(nRetcode, "vbMidiInOpen")
	End If

	If nRetcode = 0 Then
		' Clear sysex input buffer
		stMidiInBuffer = ""

		' Add buffers for sysex messages
		nRetcode = vbMidiInPrepareHeaders(hMidiIn, gnSysexInHeaderSize)
		Call vbMidiInError(nRetcode, "vbMidiInOpen")
		If nRetcode = 0 Then
		    For nHdrCtr = 1 To gnSysexInHeaderCount
				nRetcode = vbMidiInAddBuffer(hMidiIn, stMidiInHeaders(nHdrCtr).hdrPointer)
				Call vbMidiInError(nRetcode, "vbMidiInOpen")
		    Next
		    Set gctlMidiInNotify = notifyControl
		End If
	End If


	' Return the device handle, or -1 if error
	If nRetcode = 0 Then
		vbMidiInOpen = hMidiIn
	Else
		vbMidiInOpen = -1
	End If

End Function

'-----------------------------------------------------------------------
' Allocate sysex buffers and prepare the headers for vbMidiInAddBuffer
' Returns: 0 if OK, MIDI error code if error, -1 if memory allocation error
' Sets globals:
'  stMidiInHeaders() array contents
Static Function vbMidiInPrepareHeaders (hMidiIn As Integer, ByVal bufsize As Long) As Integer

	Dim nHdrCtr As Integer, nRetcode As Integer

	For nHdrCtr = 1 To gnSysexInHeaderCount

		' Initialize the header and allocate buffer memory
		nRetcode = midiHeaderInit(stMidiInHeaders(nHdrCtr), bufsize)

		If nRetcode = 0 Then

			' Call the API function to prepare the header
			nRetcode = midiInPrepareHeader(hMidiIn, stMidiInHeaders(nHdrCtr).hdrPointer, LENMIDIHDR)

			Call vbMidiInError(nRetcode, "vbMidiInPrepareHeaders")

			' Copy the prepared header from global shared memory to the VB copy
			Call midiHeaderUpdate(stMidiInHeaders(nHdrCtr))

		End If

		If nRetcode <> 0 Then
			Exit For
		End If

	Next
	
	vbMidiInPrepareHeaders = nRetcode

End Function

