' $Header:   D:/misc/midi/vcs/junoexpt.bas   1.3   15 Jan 1995 20:56:38   DAVEC  $

' Export patches
Option Explicit


' Read one CR-LF terminated string from input file
' Returns: string or "" if EOF,
'  Caller must check EOF() to distinguish blank line from EOF()
'Function vbFgets (nVBFileNumber As Integer) As String
'    Dim sRetString As String
'    Dim sCRLF As String, nLineEnd As Integer
'    Dim lStartPosition As Long
'
'    sCRLF = Chr$(13) & Chr$(10)
'
'    If EOF(nVBFileNumber) Then
'        sRetString = ""
'    Else
'        ' Save current file position
 '       lStartPosition = Seek(nVBFileNumber)
 '
'        ' Read until we hit a CRLF or end of file
'        Do
'            sRetString = Input$(255, nVBFileNumber)
'            nLineEnd = InStr(sRetString, sCRLF)
'            If nLineEnd > 0 Or EOF(nVBFileNumber) Then
'                Exit Do
'            End If
'        Loop
'        If nLineEnd > 0 Then
'            sRetString = Left$(sRetString, nLineEnd - 1)
'            ' Move file to start of next line
'            Seek nVBFileNumber, lStartPosition + nLineEnd + 1
'        End If
'    End If
'    vbFgets = sRetString
'End Function





' Exports Juno patch names to Cakewalk's instrument definition file.
' Assumes the .Patch names section is first in the definition file,
' and .Instrument Definitions is last
Sub ExportToCakewalkIns (sExportFile As String)

    Dim nInh As Integer, nOuth As Integer
    Dim bCopyLines As Integer, bDonePatchNames As Integer, bDoneInstrument As Integer
    Dim sLine As String, sTestLine As String
    Dim nSection As Integer

    ' For inlined vbFgets()
    Dim nLineEnd As Integer, lStartPosition As Long

    Dim sCRLF As String
    sCRLF = Chr$(13) & Chr$(10)

    On Error GoTo ExportErr

    Const TEMPFILENAME = "TEMP.INS"
    Const INSTRUMENT_NAME = "Juno-106"

    Const INSTRUMENT_SECTION = 1
    Const PATCHNAME_SECTION = 2
    Const OTHER_SECTION = 3
    
    Screen.MousePointer = MP_WAIT

    ' Process MASTER.INS:
    '  Put Juno-106 in .Instrument definitions section
    '  Put patch names under [Juno-106] in the .Patch Names section
    nInh = FreeFile
    Open sExportFile For Binary Access Read Shared As nInh
    
    ' Write to a temporary copy of the instrument file
    If FileExists(TEMPFILENAME) Then
	Kill TEMPFILENAME
    End If
    nOuth = FreeFile
    Open TEMPFILENAME For Binary Access Write As nOuth

    ' Scan instrument file and make our patch/instrument changes
    bCopyLines = True
    bDonePatchNames = False
    bDoneInstrument = False
    Do
	' Read one line
'-------------------------------- inlined vbFgets() -----------------
	If EOF(nInh) Then
	    sLine = ""
	Else
	    ' Save current file position
	    lStartPosition = Seek(nInh)
    
	    ' Read until we hit a CRLF or end of file
	    Do
		sLine = Input$(255, nInh)
		nLineEnd = InStr(sLine, sCRLF)
		If nLineEnd > 0 Or EOF(nInh) Then
		    Exit Do
		End If
	    Loop
	    If nLineEnd > 0 Then
		sLine = Left$(sLine, nLineEnd - 1)
		' Move file to start of next line
		Seek nInh, lStartPosition + nLineEnd + 1
	    End If
	End If
'-------------------------------- End of inlined vbFgets() -----------------
'        sLine = vbFgets(nInh)

	If Len(sLine) = 0 And EOF(nInh) Then
	    Exit Do
	End If

	' Line to check standard values against
	sTestLine = UCase$(Trim$(sLine))

	' Decide which section we're in
	If Left$(sLine, 1) = "." Then
	    Select Case sTestLine
	    Case ".PATCH NAMES"
		nSection = PATCHNAME_SECTION
	    Case ".INSTRUMENT DEFINITIONS"
		nSection = INSTRUMENT_SECTION
	    Case Else
		nSection = OTHER_SECTION
	    End Select
	    If nSection <> PATCHNAME_SECTION And Not bDonePatchNames Then
		' Come to end of patch name section without writing them, do it now
		' This assumes the patch name section is the first in the file
		WriteCakewalkPatchNames nOuth, INSTRUMENT_NAME
		bDonePatchNames = True
	    End If
	    bCopyLines = True
	Else
	    If Left$(sTestLine, 1) = "[" Then
		' Start of a new instrument, patch definition, or whatever
		Select Case sTestLine
		Case "[" & UCase$(INSTRUMENT_NAME) & "]"    ' It's the one we're changing
		    Select Case nSection
		    Case INSTRUMENT_SECTION
			WriteCakewalkInstrument nOuth, INSTRUMENT_NAME
			bDoneInstrument = True
			bCopyLines = False
		    Case PATCHNAME_SECTION
			WriteCakewalkPatchNames nOuth, INSTRUMENT_NAME
			bDonePatchNames = True
			bCopyLines = False
		    Case Else
			' Don't change whatever this section is
			bCopyLines = True
		    End Select
		Case Else
		    ' Just copy this line
		    bCopyLines = True
		End Select
	    End If
	End If

	If bCopyLines Then
	    sLine = sLine & sCRLF
	    Put #nOuth, , sLine
	End If

	DoEvents

    Loop

    If bDoneInstrument = False Then ' Instrument wasn't present in definitions section
	' Assumes the Instrument definitions section is last in the file
	WriteCakewalkInstrument nOuth, INSTRUMENT_NAME
	bDoneInstrument = True
    End If

    Close nInh
    Close nOuth

    If bDoneInstrument And bDonePatchNames Then
	FileCopy sExportFile, stripExt(sExportFile) & ".BAK"
	DoEvents
	FileCopy TEMPFILENAME, sExportFile
	Kill TEMPFILENAME
    Else
	MsgBox "Couldn't add patch names and instrument to Cakewalk instrument file"
    End If

    GoTo ExportExit

ExportErr:
    ShowError
    Close   ' Closes all open files
    Resume ExportExit

ExportExit:
    Screen.MousePointer = MP_DEFAULT

End Sub

' Export to Cakewalk 2 PATCH.INI file
Sub ExportToCakewalkPatch (sExportFile As String)

    Dim nRetcode As Integer, iCtr As Integer

    ' Put Juno in Cakewalk list of lists
    nRetcode = WritePrivateProfileString("List of Lists", "Juno-106", " ", sExportFile)
    ' Write patch description entries
    For iCtr = 0 To MAX_PATCHES - 1
	If Len(gPatches(iCtr).Sysex) Then
	    ' Write the description
	    nRetcode = WritePrivateProfileString("Juno-106", Trim$(Str$(iCtr)), gPatches(iCtr).Description, sExportFile)
	Else
	    ' Delete any existing entry
	    nRetcode = WritePrivateProfileString("Juno-106", Trim$(Str$(iCtr)), 0&, sExportFile)
	End If
    Next

End Sub

Sub ShowError ()
    Dim s As String
    Dim crlf As String
    
    crlf = Chr(13) + Chr(10)
    s = "The following Error occurred:" + crlf + crlf
    'add the error string
    s = s + Error$ + crlf
    'add the error number
    s = s + "Number: " + CStr(Err)
    'beep and show the error
    Beep
    MsgBox (s)

End Sub

Sub WriteCakewalkInstrument (nOuth As Integer, sInstrumentName As String)

    Dim sOuts As String
    
    sOuts = "[" & sInstrumentName & "]" & Chr$(13) & Chr$(10) & "Patch[*]=" & sInstrumentName & Chr$(13) & Chr$(10)
    Put #nOuth, , sOuts


End Sub

Sub WriteCakewalkPatchNames (nOuth As Integer, sInstrumentName As String)

    Dim iCtr As Integer
    Dim sCRLF As String, sOuts As String
    sCRLF = Chr$(13) & Chr$(10)
    
    sOuts = "[" & sInstrumentName & "]" & sCRLF
    Put #nOuth, , sOuts
    ' Write patch description entries
    For iCtr = 0 To MAX_PATCHES - 1
	If Len(gPatches(iCtr).Sysex) Then
	    ' Write the description
	    sOuts = Trim$(Str$(iCtr)) & "=" & gPatches(iCtr).Description & sCRLF
	    Put #nOuth, , sOuts
	End If
    Next

    ' Add an extra blank line, 'cause the file parsing swallows them.
    Put #nOuth, , sCRLF

End Sub

