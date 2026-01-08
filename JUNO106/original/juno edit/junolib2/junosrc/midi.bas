' $Header:   D:/misc/midi/vcs/midi.bas   1.9   07 Nov 1994 00:13:44   DAVEC  $

' Public domain by David Churcher. No rights reserved. Use at your own risk.

Option Explicit

' Control used to notify MIDI input
Global gctlMidiInNotify As Control

' API standard MIDIHDR structure, cf. MMSYSTEM.H
Type MIDIHDR
	lpdata As Long
	dwbufferlength As Long
	dwbytesrecorded As Long
	dwUser As Long
	dwFlags As Long
	lpNext As Long
	reserved As Long
End Type

' Encapsulates each MIDIHDR for Visual Basic; saves the memory pointers and handles for the
' MIDIHDR structure and the sysex data buffer
Type vbMIDIHDR
	midiheader As MIDIHDR
	hdrHandle As Integer
	hdrPointer As Long
	sysexHandle As Integer
	sysexPointer As Long
End Type

Global Const LENMIDIHDR = 28      ' Length of the MIDIHDR structure

' Kernel functions for messing with pointers
' Used to get the address of MIDIHDR variables: addr = lstrcat( var, "" )
' Have to declare the variable as MIDIHDR because Any doesn't work
' with user-defined types
Declare Function lstrcat Lib "kernel.dll" (theVar As MIDIHDR, ByVal blankParam As Any) As Long

' MIDI callback messages
Global Const MM_MIM_OPEN = &H3C1
Global Const MM_MIM_CLOSE = &H3C2
Global Const MM_MIM_DATA = &H3C3
Global Const MM_MIM_LONGDATA = &H3C4
Global Const MM_MIM_ERROR = &H3C5
Global Const MM_MIM_LONGERROR = &H3C6
Global Const MM_MOM_OPEN = &H3C7
Global Const MM_MOM_CLOSE = &H3C8
Global Const MM_MOM_DONE = &H3C9

' flags used with waveOutOpen(), waveInOpen(), midiInOpen(), and
' midiOutOpen() to specify the type of the dwCallback parameter.
Global Const CALLBACK_TYPEMASK& = &H70000
Global Const CALLBACK_NULL& = &H0
Global Const CALLBACK_WINDOW& = &H10000
Global Const CALLBACK_TASK& = &H20000
Global Const CALLBACK_FUNCTION& = &H30000

' MIDI error codes
Global Const MIDIERR_BASE = 64
Global Const MIDIERR_UNPREPARED = (MIDIERR_BASE + 0)     ' header not prepared
Global Const MIDIERR_STILLPLAYING = (MIDIERR_BASE + 1)   ' still something playing
Global Const MIDIERR_NOMAP = (MIDIERR_BASE + 2)          ' no current map
Global Const MIDIERR_NOTREADY = (MIDIERR_BASE + 3)       ' hardware is still busy
Global Const MIDIERR_NODEVICE = (MIDIERR_BASE + 4)       ' port no longer connected
Global Const MIDIERR_INVALIDSETUP = (MIDIERR_BASE + 5)   ' invalid setup

' flags for dwFlags field of MIDIHDR structure
Global Const MHDR_DONE = 1            ' done bit
Global Const MHDR_PREPARED = 2        ' set if header prepared

' MIDI message codes
'       Three-byte messages:
Global Const MIDI_NOTEON = &H90
Global Const MIDI_NOTEOFF = &H80
Global Const MIDI_KEYAFTERTOUCH = &HA0
Global Const MIDI_CONTROLCHANGE = &HB0
Global Const MIDI_PITCHBEND = &HE0
Global Const MIDI_SONGPOSPTR = &HF2

'       Two-byte messages
Global Const MIDI_PROGRAMCHANGE = &HC0
Global Const MIDI_CHANAFTERTOUCH = &HD0
Global Const MIDI_SONGSELECT = &HF3

'       System messages
Global Const MIDI_SYSTEMMESSAGE = &HF0         ' Mask for system messages, see midiInShortHandler
Global Const MIDI_BEGINSYSEX = &HF0
Global Const MIDI_MTCQUARTERFRAME = &HF1
' Sysex start, message continues till MIDI_ENDSYSEX
Global Const MIDI_ENDSYSEX = &HF7
' 1-byte messages
Global Const MIDI_TIMINGCLOCK = &HF8
Global Const MIDI_START = &HFA
Global Const MIDI_CONTINUE = &HFB
Global Const MIDI_STOP = &HFC
Global Const MIDI_ACTIVESENSING = &HFE
Global Const MIDI_SYSTEMRESET = &HFF

Global Const MIDI_ALLNOTESOFF = 123

'-----------------------------------------------------------------------
' Call the appropriate input and output message handlers for MIDI messages
' Called by MsgBlast hook for both input and output messages
' MsgVal: One of the MIDI messages set up in midiMessageTrap
' wParam, lParam: Depends on the message, see handler procs
Sub midiCallbackWndProc (MsgVal As Integer, wParam As Integer, lParam As Long)

	Select Case MsgVal

	Case MM_MIM_DATA
		' Filter out timing clock and active sensing
		' This is purely to improve performance, because some MIDI devices
		' generate an awful lot of these messages, and they're not much use
		' to this library because it's too slow to do anything with them!
		If lParam = MIDI_TIMINGCLOCK Or lParam = MIDI_ACTIVESENSING Then
		    Exit Sub
		Else
		    Call midiShortInHandler(wParam, lParam)
		    ' Debug.Print "MM_MIM_DATA " + Str$(wParam) + Str$(lParam)
		End If

	Case MM_MIM_LONGDATA
		' Debug.Print "MM_MIM_LONGDATA"
		Call midiLongInHandler(wParam, lParam)
   
	Case MM_MIM_ERROR
		' Debug.Print "MM_MIM_ERROR"

	Case MM_MIM_LONGERROR
		' Debug.Print "MM_MIM_LONGERROR"

	Case MM_MOM_DONE
		' Debug.Print "MM_MOM_DONE"

	Case Else
		' Debug.Print "Unrecognized message " + Str$(MsgVal)

	End Select

	' Notify caller of MIDI input by triggering their control's Change event
	gctlMidiInNotify = Str$(Rnd)
	
End Sub

'-----------------------------------------------------------------------
' Deallocate and free the memory associated with a vbMIDIHDR
Sub midiHeaderDeInit (theHeader As vbMIDIHDR)

	Dim nRetcode As Integer

	nRetcode = GlobalPageUnlock(theHeader.hdrHandle)
	nRetcode = GlobalPageUnlock(theHeader.sysexHandle)
	nRetcode = GlobalUnlock(theHeader.hdrHandle)
	nRetcode = GlobalUnlock(theHeader.sysexHandle)
	nRetcode = GlobalFree(theHeader.hdrHandle)
	nRetcode = GlobalFree(theHeader.sysexHandle)

	theHeader.hdrHandle = 0
	theHeader.hdrPointer = 0
	theHeader.sysexHandle = 0
	theHeader.sysexPointer = 0

End Sub

'-----------------------------------------------------------------------
' Initialize a vbMIDIHDR structure, allocating memory as required
' Returns True if successful, False if failed
Function midiHeaderInit (theHeader As vbMIDIHDR, ByVal bufsize As Long)

	Dim nRetcode As Integer
	Dim thisptr As Long
	Dim retptr As Long
	Dim hdrHandle As Integer
	Dim hdrPointer As Long
	Dim sysexHandle As Integer
	Dim sysexPointer As Long

	hdrHandle = GlobalAlloc(GMEM_MOVEABLE + GMEM_SHARE + GMEM_ZEROINIT, LENMIDIHDR)
	sysexHandle = GlobalAlloc(GMEM_MOVEABLE + GMEM_SHARE + GMEM_ZEROINIT, bufsize)

	If hdrHandle = 0 Or sysexHandle = 0 Then

		nRetcode = MsgBox("Can't allocate buffer memory")
		nRetcode = False     ' Failed

	Else

		hdrPointer = GlobalLock(hdrHandle)
		sysexPointer = GlobalLock(sysexHandle)

		' GlobalPageLock the handles so they stay fixed in memory (cf glib.c)
		nRetcode = GlobalPageLock(hdrHandle)
		nRetcode = GlobalPageLock(sysexHandle)

		' Initialize the MIDIHDR structure with the buffer values
		theHeader.midiheader.lpdata = sysexPointer
		theHeader.midiheader.dwbufferlength = bufsize
		theHeader.midiheader.dwUser = 0
		theHeader.midiheader.lpNext = 0
		theHeader.midiheader.reserved = 0

		' Save the memory handles and pointers to the vbMIDIHDR structure
		theHeader.hdrHandle = hdrHandle
		theHeader.sysexHandle = sysexHandle
		theHeader.hdrPointer = hdrPointer
		theHeader.sysexPointer = sysexPointer

		' Copy the header from the VB structure to locked shareable global memory
		thisptr = lstrcat(theHeader.midiheader, "")
		Call hmemcpy(hdrPointer, thisptr, LENMIDIHDR)

		nRetcode = True     ' Success

	End If

End Function

'-----------------------------------------------------------------------
' Update contents of a vbMIDIHDR structure from its global memory copy
' Warning: Will probably GPF if the vbMIDIHDR doesn't contain a valid pointer!
Sub midiHeaderUpdate (theHdr As vbMIDIHDR)

	Dim lVbPtr As Long
	Dim lHdrPointer As Long

	' Get the memory address of the MIDIHDR
	lVbPtr = lstrcat(theHdr.midiheader, "")

	lHdrPointer = theHdr.hdrPointer

	If lVbPtr <> 0 And lHdrPointer <> 0 Then
		Call hmemcpy(lVbPtr, lHdrPointer, LENMIDIHDR)
	End If

End Sub

'-----------------------------------------------------------------------
' Set up MsgBlaster message trapping for MIDI input and output messages
' hMsgWnd: handle of window to subclass (same handle that was sent to midiInOpen or midiOutOpen)
' blasterControl: MsgBlaster control
' Notes: Output messages MM_MOM_OPEN and MM_MOM_CLOSE aren't trapped as they're not very interesting
Sub midiMessageTrap (ByVal hMsgWnd As Integer, blasterControl As Control)
	
	' EATMESSAGE means the messages will not be passed
	' to the window on the form at all; they will be
	' handled by the MsgBlaster _Message procedure

	Const PREPROCESS = -1
	Const EATMESSAGE = 0
	Const POSTPROCESS = 1

	blasterControl.hWndTarget = hMsgWnd
	blasterControl.MsgList(0) = MM_MIM_LONGDATA
	blasterControl.MsgPassage(0) = EATMESSAGE
	blasterControl.MsgList(1) = MM_MIM_ERROR
	blasterControl.MsgPassage(1) = EATMESSAGE
	blasterControl.MsgList(2) = MM_MIM_LONGERROR
	blasterControl.MsgPassage(2) = EATMESSAGE
	' NB: This property array element may be changed by vbMidiInStart
	' to disable short message input.
	blasterControl.MsgList(3) = MM_MIM_DATA
	blasterControl.MsgPassage(3) = EATMESSAGE

	' DONE message not currently used
	' blasterControl.MsgList(4) = MM_MOM_DONE
	' blasterControl.MsgPassage(4) = EATMESSAGE

End Sub


