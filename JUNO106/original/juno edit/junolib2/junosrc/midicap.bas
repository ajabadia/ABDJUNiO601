' $Header:   D:/misc/midi/vcs/midicap.bas   1.5   18 Sep 1994 12:02:48   DAVEC  $

' Public domain by David Churcher. No rights reserved. Use at your own risk.

Option Explicit

Type MIDIINCAPS
	wMid As Integer
	wPid As Integer
	vDriverVersion As Integer
	szPname As String * 32
End Type

Type MIDIOUTCAPS
	wMid As Integer
	wPid As Integer
	vDriverVersion As Integer
	szPname As String * 32
	wTechnology As Integer
	wVoices As Integer
	wNotes As Integer
	wChannelMask As Integer
	dwSupport As Long
End Type

Declare Function midiInGetNumDevs Lib "mmsystem" () As Integer
Declare Function midiInGetDevCaps Lib "mmsystem" (ByVal wDeviceID As Integer, lpCaps As MIDIINCAPS, ByVal wSize As Integer) As Integer
Declare Function midiOutGetNumDevs Lib "mmsystem" () As Integer
Declare Function midiOutGetDevCaps Lib "mmsystem" (ByVal wDeviceID As Integer, lpCaps As MIDIOUTCAPS, ByVal wSize As Integer) As Integer

' Global arrays to hold MIDI input and output capabilities
' Filled by LoadCaps
Global incaps() As MIDIINCAPS
Global outcaps() As MIDIOUTCAPS

'-----------------------------------------------------------------------
' Find a MIDI input or output device's ID number given the name.
' Returns device number or -1 if not found
' Warning: Assumes that input and output ports on the same device have different names!
Function findDeviceByName (sDeviceName As String)

    Dim nretcode As Integer

    nretcode = findInDeviceByName(sDeviceName)
    If nretcode < 0 Then
	nretcode = findOutDeviceByName(sDeviceName)
    End If

    findDeviceByName = nretcode

End Function

'-----------------------------------------------------------------------
' Purpose: Find MIDI input device number given its name
' Returns: Device number if successful, -1 if failed
Function findInDeviceByName (ByVal sDeviceName As String) As Integer

	Dim nCtr As Integer

	findInDeviceByName = -1

	sDeviceName = Trim(sDeviceName)

	For nCtr = 0 To UBound(incaps)
		If sDeviceName = Trim(incaps(nCtr).szPname) Then
			findInDeviceByName = nCtr
			Exit For
		End If
	Next

End Function

'-----------------------------------------------------------------------
' Purpose: Find MIDI output device given its name
' Returns: Device number if successful, -1 if failed
Function findOutDeviceByName (ByVal sDeviceName As String) As Integer

	Dim nCtr As Integer

	findOutDeviceByName = -1

	sDeviceName = Trim(sDeviceName)
	For nCtr = 0 To UBound(outcaps)
		If sDeviceName = Trim(outcaps(nCtr).szPname) Then
			findOutDeviceByName = nCtr
			Exit For
		End If
	Next


End Function

'-----------------------------------------------------------------------
' Loads MIDI device capabilities and names to incaps() and outcaps() arrays
' Note incaps() and outcaps() are zero-based, so the index corresponds to the
' MIDI device number on the system
' Returns True if successful, False if failed
Function LoadCaps ()

	Dim nretcode As Integer, mididevice As Integer
	Dim noOfInDevices As Integer, noOfOutDevices As Integer

	noOfInDevices = midiInGetNumDevs()
	noOfOutDevices = midiOutGetNumDevs()
	If noOfInDevices = 0 Or noOfOutDevices = 0 Then
		MsgBox "No MIDI input/output device installed."
		LoadCaps = False
		Exit Function
	End If

	ReDim incaps(0 To noOfInDevices - 1) As MIDIINCAPS
	For mididevice = 0 To noOfInDevices - 1
		nretcode = midiInGetDevCaps(mididevice, incaps(mididevice), Len(incaps(mididevice)))
		incaps(mididevice).szPname = szTrim(incaps(mididevice).szPname)
	Next
	ReDim outcaps(0 To noOfOutDevices - 1) As MIDIOUTCAPS
	For mididevice = 0 To noOfOutDevices - 1
		nretcode = midiOutGetDevCaps(mididevice, outcaps(mididevice), Len(outcaps(mididevice)))
		outcaps(mididevice).szPname = szTrim(outcaps(mididevice).szPname)
	Next
	
	LoadCaps = True    ' Success

End Function

'-----------------------------------------------------------------------
' Removes trailing \0 and any text after it
' Returns: Trimmed string
Function szTrim (szString As String) As String

	Dim pos As Integer, ln As Integer

	pos = InStr(szString, Chr$(0))
	ln = Len(szString)

	Select Case pos
	Case Is > 1
		szTrim = Trim$(Left$(szString, pos - 1))
	Case 1
		szTrim = ""
	Case Else
		szTrim = Trim$(szString)
	End Select

End Function

