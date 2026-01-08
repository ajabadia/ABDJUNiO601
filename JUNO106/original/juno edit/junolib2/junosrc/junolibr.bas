' $Header:   D:/misc/midi/vcs/junolibr.bas   1.18   15 Jan 1995 20:58:58   DAVEC  $

' Public domain by David Churcher. No rights reserved. Use at your own risk.

Option Explicit

'----------------- Data types ----------------------------
Type midiDev
	DeviceNumber As Integer
	DeviceName   As String
	handle As Integer
End Type

Type patch
	Sysex As String
	Description As String
End Type

'----------------- Constants ----------------------------
Global Const INI_FILENAME = "junolibr.INI"
Global Const APP_NAME = "Juno 106 Librarian"
Global Const UNTITLED_NAME = "[untitled]"

' Maximum number of patches supported
Global Const MAX_PATCHES = 128

' Roland ID number used in System Exclusive messages
Global Const ROLAND_ID = &H41
' Sysex message number for entire patches
Global Const SYSEX_PATCH = &H30
' Sysex message number for slider/switch change messages
Global Const SYSEX_CONTROL = &H32
' Length of a patch without sysex header and trailer
Global Const PATCH_LENGTH = 18
' Length of a patch with sysex header and trailer
Global Const SYSEX_PATCH_LENGTH = 24
' Position of MIDI channel in sysex patch
Global Const MIDI_CHANNEL_IN_SYSEX_PATCH = 4
' Position of patch number in sysex patch
Global Const PATCH_NO_IN_SYSEX_PATCH = 5


' Length of sysex patch header, including the patch number and MIDI channel number
Global Const SYSEX_PATCH_HEADER_LENGTH = 5
' Length of panel control sysex message
Global Const CONTROL_MSG_LENGTH = 7
' Offset from start of control message to control number and control value
Global Const CONTROL_NO_IN_MSG = 5
Global Const CONTROL_VAL_IN_MSG = 6
' Position in patch of switch control bytes 1 and 2
Global Const CONTROLBYTE_1 = 17
Global Const CONTROLBYTE_2 = 18

' MsgBox parameters
Global Const MB_OK = 0                 ' OK button only
Global Const MB_OKCANCEL = 1           ' OK and Cancel buttons
Global Const MB_ABORTRETRYIGNORE = 2   ' Abort, Retry, and Ignore buttons
Global Const MB_YESNOCANCEL = 3        ' Yes, No, and Cancel buttons
Global Const MB_YESNO = 4              ' Yes and No buttons
Global Const MB_RETRYCANCEL = 5        ' Retry and Cancel buttons

Global Const MB_ICONSTOP = 16          ' Critical message
Global Const MB_ICONQUESTION = 32      ' Warning query
Global Const MB_ICONEXCLAMATION = 48   ' Warning message
Global Const MB_ICONINFORMATION = 64   ' Information message

Global Const MB_APPLMODAL = 0          ' Application Modal Message Box
Global Const MB_DEFBUTTON1 = 0         ' First button is default
Global Const MB_DEFBUTTON2 = 256       ' Second button is default
Global Const MB_DEFBUTTON3 = 512       ' Third button is default
Global Const MB_SYSTEMMODAL = 4096      'System Modal

' MsgBox return values
Global Const IDOK = 1                  ' OK button pressed
Global Const IDCANCEL = 2              ' Cancel button pressed
Global Const IDABORT = 3               ' Abort button pressed
Global Const IDRETRY = 4               ' Retry button pressed
Global Const IDIGNORE = 5              ' Ignore button pressed
Global Const IDYES = 6                 ' Yes button pressed
Global Const IDNO = 7                  ' No button pressed

' Clipboard format
Global Const CF_TEXT = 1

' Help
Global Const HELP_CONTENTS = &H3

' MousePointer constants
Global Const MP_DEFAULT = 0
Global Const MP_WAIT = 11

' ---------------- Global variables ----------------------
' Input and output device numbers
Global gInputdev As midiDev
Global gOutputDev As midiDev

' Number of current patch selected in listbox and displayed on panel
' Zero-based, i.e. patch A11 is patch number 0
Global gnCurrentPatch As Integer

' Contents of current (possibly modified) patch
Global gsCurrentPatch As String

' String at start of each sysex patch message
' (header also includes patch number)
' Should be a global constant but VB can't handle it, initialised in
' JunoLibrSetup
Global SYSEX_PATCH_HEADER As String

' True if current patch file has been modified
Global gbFileModified As Integer

' The patches and their descriptions
Global gPatches(MAX_PATCHES) As patch

' Patch cut/copy buffer
Global gCutPatch As patch

' MIDI output active flags
Global gbRunning As Integer
Global gbOutputOn As Integer

' Current patch filename, including path
Global gsCurrentPatchFilename As String

' MIDI channel for sending sample notes. NB This is 1-based and the MIDI messages are 0-based!
Global gnMidiChannel As Integer
' MIDI note for the sample note (0-127)
Global gnMidiNote As Integer

' True when control panel isn't being automatically updated by the program
Global gbPanelLive As Integer

'---------------------------------------------------------
' INI file functions (correct versions from the Knowledgebase tips file)
Declare Function GetPrivateProfileString Lib "Kernel" (ByVal lpApplicationName As String, ByVal lpKeyName As Any, ByVal lpDefault As String, ByVal lpReturnedString As String, ByVal nSize As Integer, ByVal lpFileName As String) As Integer
Declare Function WritePrivateProfileString Lib "Kernel" (ByVal lpApplicationName As String, ByVal lpKeyName As Any, ByVal lpString As Any, ByVal lplFileName As String) As Integer
Declare Function GetPrivateProfileInt Lib "Kernel" (ByVal lpAppName$, ByVal lpKeyName$, ByVal nDefault%, ByVal lpFileName$) As Integer


' Help
' Commands to pass WinHelp()
Global Const HELP_CONTEXT = &H1 '  Display topic in ulTopic
Global Const HELP_QUIT = &H2    '  Terminate help
Global Const HELP_INDEX = &H3   '  Display index
Global Const HELP_HELPONHELP = &H4      '  Display help on using help
Global Const HELP_SETINDEX = &H5        '  Set the current Index for multi index help
Global Const HELP_KEY = &H101           '  Display topic for keyword in offabData
Global Const HELP_MULTIKEY = &H201

Declare Function WinHelp Lib "User" (ByVal hWnd As Integer, ByVal lpHelpFile As String, ByVal wCommand As Integer, dwData As Any) As Integer

'-----------------------------------------------------------------------
' Purpose: Ask a yes/no question
' Returns: True for yes
Function askyesno (sQuestion As String)

	askyesno = (MsgBox(sQuestion, MB_ICONQUESTION Or MB_YESNO) = IDYES)

End Function

'-----------------------------------------------------------------------
' Purpose: Close MIDI input and output devices
Sub CloseDev ()

	If gInputdev.handle >= 0 Then

		gInputdev.handle = vbMidiInClose(gInputdev.handle)
		gInputdev.handle = -1

	End If

	If gOutputDev.handle >= 0 Then
		
		gOutputDev.handle = vbMidiOutClose(gOutputDev.handle)
		gOutputDev.handle = -1

	End If

End Sub

'-----------------------------------------------------------------------
' Purpose: Set description for current patch
' Returns:
'    gPatches().Description entry changed
'    PatchList entry changed
Sub DescriptionUpdate (ByVal sNewDescription As String)

    ' Save it to the description array
    gPatches(gnCurrentPatch).Description = sNewDescription

    ' Eveything except the global array stores a blank description as <empty>
    If sNewDescription = "" Then
		sNewDescription = "<empty>"
    End If

    ' Save it to the list
    JunoLibrarian!PatchList.List(gnCurrentPatch) = PatchNumberText(gnCurrentPatch) + " " + sNewDescription

    ' Show it on the panel
    JunoPanel.Caption = sNewDescription

    gbFileModified = True

End Sub

'-----------------------------------------------------------------------
' Purpose: Convert list of space-delimited hex bytes to binary string
Function HexListToString (ByVal sHexList As String) As String

	Dim sRetString As String, nThisByte As Integer
	Dim nStartpos As Integer, nEndPos As Integer, nSpacePos As Integer

	nStartpos = 1
	sRetString = ""
	sHexList = Trim$(sHexList)

	Do While nStartpos < Len(sHexList)

		' Extract next byte from string
		nSpacePos = InStr(nStartpos, sHexList, " ")
		If nSpacePos = 0 Then
			nEndPos = Len(sHexList)
		Else
			nEndPos = nSpacePos - 1
		End If

		nThisByte = Val("&H" + Mid$(sHexList, nStartpos, nEndPos))

		If nThisByte >= 0 And nThisByte < 256 Then
		    ' Convert to character
		    sRetString = sRetString + Chr$(nThisByte)
		End If

		If nSpacePos = 0 Then
			Exit Do
		Else
			nStartpos = nSpacePos + 1
		End If

	Loop

	HexListToString = sRetString

End Function

'-----------------------------------------------------------------------
' Purpose: Clean up
Sub JunoLibrCleanup ()

	Call CloseDev

	Call SaveIni

	Unload JunoPanel

End Sub

'-----------------------------------------------------------------------
' Purpose: Set everything up
Sub JunoLibrSetup ()
	
	' Init MIDI handles to invalid number
	gInputdev.handle = -1
	gOutputDev.handle = -1
	
	gbRunning = False
	gbOutputOn = False
	JunoLibrarian.btnSavePatch.Enabled = False

	gnMidiChannel = 1   ' Default, configured channel loaded by loadsetup()

	' Definition for the bytes at the start of each patch.
	' NB there's also the MIDI channel number and the patch number between
	' this sequence and the patch data
	SYSEX_PATCH_HEADER = Chr$(MIDI_BEGINSYSEX) + Chr$(ROLAND_ID) + Chr$(SYSEX_PATCH)

	' Load config and patches
	If LoadSetup() = False Then
		Unload JunoLibrarian
		End
	End If

	' Copy patch descriptions to listboxes
	PatchListLoad

	' Start MIDI I/O
	startAndStop

	' Show and set first patch
	JunoLibrarian!PatchList.ListIndex = 0
	Call PatchChange(0)

    ' Get the synth control panel up
    JunoPanel.Show 0

    gbPanelLive = True

End Sub

'-----------------------------------------------------------------------
' Purpose: Load configuration from INI file and set globals
' gInputDev.deviceName, gOutputdev.deviceName, gMidiInDevNo, gMidiOutDevNo
Function LoadIni ()

	LoadIni = LoadIniDev("Input", gInputdev, 0, incaps(0).szPname) And LoadIniDev("Output", gOutputDev, 0, outcaps(0).szPname) And LoadIniMIDI()

End Function

'-----------------------------------------------------------------------
' Load and validate a configured MIDI input or output device from the INI file
' Returns: True if OK
' sIniKey: Name of key to load from INI file e.g. "Input"
' nDefaultDevNumber: Number of default MIDI device if no configured device or invalid device name
' sDefaultDevName: Name of default MIDI device
Function LoadIniDev (sIniKey As String, dev As midiDev, nDefaultDevNumber, sDefaultDevName As String)

	Dim sDevName As String, nRetcode As Integer

	sDevName = Space$(32)

	' Load settings to get names of configured devices
	' (ByVal Appname As String, ByVal KeyName As String, ByVal DEFAULT As String, ByVal ReturnedString As String, ByVal MaxSize, ByVal FileName As String)
	nRetcode = GetPrivateProfileString("Midi Devices", sIniKey, "Not configured", sDevName, 32, INI_FILENAME)

	sDevName = szTrim(sDevName)
	If sDevName = "Not configured" Then
		MsgBox "No " + LCase$(sIniKey) + " device configured, defaulting to " + Trim(sDefaultDevName)
		dev.DeviceName = Trim(sDefaultDevName)
		dev.DeviceNumber = nDefaultDevNumber
	Else
		' Find matching device number
		dev.DeviceNumber = findDeviceByName(sDevName)
		If dev.DeviceNumber >= 0 Then
			' Configured device is OK
			dev.DeviceName = Trim(sDevName)
		Else
			MsgBox "Can't find configured MIDI input device " + sDevName + ", defaulting to " + Trim(sDefaultDevName)
			dev.DeviceNumber = nDefaultDevNumber
			dev.DeviceName = sDefaultDevName
		End If
	End If

	LoadIniDev = True ' No error return just yet

End Function

' Purpose: Load MIDI sample note settings
' Returns: gnMidiChannel and gnMidiNote
Function LoadIniMIDI ()
	
	Dim sBuffer As String, sPatchFilename As String, nRetcode As Integer

	LoadIniMIDI = True
	gnMidiChannel = 1
	gnMidiNote = 64
	sBuffer = Space$(11)

	nRetcode = GetPrivateProfileString("MIDI Devices", "MIDI Channel", "", sBuffer, 10, INI_FILENAME)
	If nRetcode > 0 And Val(sBuffer) > 0 And Val(sBuffer) <= 16 Then
	    gnMidiChannel = Val(sBuffer)
	End If
	nRetcode = GetPrivateProfileString("MIDI Devices", "MIDI Note", "", sBuffer, 10, INI_FILENAME)
	If nRetcode > 0 And Val(sBuffer) >= 0 And Val(sBuffer) <= 127 Then
	    gnMidiNote = Val(sBuffer)
	End If

	LoadIniMIDI = True

End Function

'-----------------------------------------------------------------------
' Purpose: Load last patch file used, if it's still there
' Returns: True if load successful or no previous patch file,
'                       False if previous patch file exists but load fails
Function LoadIniPatches ()

	Dim sPatchFilename As String, nRetcode As Integer

	sPatchFilename = Space$(255)

	LoadIniPatches = True

	nRetcode = GetPrivateProfileString("Defaults", "LastPatchFile", "", sPatchFilename, 255, INI_FILENAME)
	If nRetcode > 0 Then

		' Remove trailing \0
		sPatchFilename = Left$(sPatchFilename, nRetcode)

		nRetcode = LoadPatches(sPatchFilename, False)
		If Not nRetcode Then
			' Couldn't load old patches so init blank set
			MenuNew
		End If

	End If

	LoadIniPatches = True

End Function

'-----------------------------------------------------------------------
' Purpose: Load and validate the setup configuration
' Returns: MIDI input and output device numbers
Function LoadSetup ()

If LoadCaps() And LoadIni() And LoadIniPatches() Then

	LoadSetup = True
	JunoPanel.Enabled = True

Else

	LoadSetup = False

End If


End Function

'-----------------------------------------------------------------------
' Purpose: Deal with new MIDI input data
' Called by Change procedure on control nominated in vbMidiInOpen
' when MIDI input is received
Sub midiInputArrived ()

	Dim sMidiData As String, nRetcode As Integer

    gbPanelLive = False

	sMidiData = midiSysexGet()

	Select Case Len(sMidiData)

	Case SYSEX_PATCH_LENGTH ' Full patch

		PatchArrived (sMidiData)

	Case CONTROL_MSG_LENGTH  ' Control change

		Call ControlMsgArrived(sMidiData, True)

	End Select

    gbPanelLive = True

End Sub

'-----------------------------------------------------------------------
' Purpose: Open MIDI output device
Sub OpenDev ()
	Dim nRetcode As Integer
	If gInputdev.handle < 0 Then

	    ' Get lots of short sysex input buffers
	    ' to try to get all the control change messages
	    Call sysexInParameters(100, 30)
		
	    gInputdev.handle = vbMidiInOpen(JunoLibrarian.hWnd, gInputdev.DeviceNumber, JunoLibrarian.MsgBlaster1, JunoLibrarian.MidiNotifyControl)
	    If (gInputdev.handle > 0) Then
		    nRetcode = midiInStart(gInputdev.handle)
	    End If
	End If
	If gOutputDev.handle < 0 Then
		gOutputDev.handle = vbMidiOutOpen(JunoLibrarian.hWnd, gOutputDev.DeviceNumber)
	End If
End Sub

'-----------------------------------------------------------------------
' Purpose: Save configuration and defaults
Sub SaveIni ()

	Dim nRetcode As Integer

	If gsCurrentPatchFilename <> "" And gsCurrentPatchFilename <> UNTITLED_NAME Then
		nRetcode = WritePrivateProfileString("Defaults", "LastPatchFile", gsCurrentPatchFilename, INI_FILENAME)
	End If

End Sub

'-----------------------------------------------------------------------
' Purpose: save one INI MIDI device entry
Sub SaveIniEntry (ByVal keyName As String, ByVal keyValue As String)

	Dim nRetcode As Integer

	nRetcode = WritePrivateProfileString("Midi Devices", keyName, keyValue, INI_FILENAME)

End Sub

'-----------------------------------------------------------------------
' Purpose: Start and stop MIDI input and output
Sub startAndStop ()
	
	If gbRunning = True Then

		gbRunning = False
		Call CloseDev
		
	Else

		Call OpenDev

		' Only change state if opened successfully
		If gInputdev.handle > 0 And gOutputDev.handle > 0 Then
			gbRunning = True
		End If
	
	End If

End Sub

'-----------------------------------------------------------------------
' Purpose: Convert string to space-delimited list of hex values
Function StringToHexList (sTheString As String) As String

	Dim sRetString As String
	Dim ictr As Integer

	For ictr = 1 To Len(sTheString)

		' Convert to two hex characters (the Right$() adds a leading 0 if necessary)
		sRetString = sRetString & Right$("0" + Hex$(Asc(Mid$(sTheString, ictr, 1))), 2) & " "

	Next

	sRetString = Trim$(sRetString)

	StringToHexList = sRetString

End Function

