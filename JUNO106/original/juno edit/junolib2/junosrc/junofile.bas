' $Header:   D:/misc/midi/vcs/junofile.bas   1.8   15 Jan 1995 20:58:32   DAVEC  $

Option Explicit

' Constants for common open and save dialogs
Const OFN_FILEMUSTEXIST = &H1000&
Const OFN_HIDEREADONLY = &H4&
Const OFN_OVERWRITEPROMPT = &H2&
Const OFN_NOREADONLYRETURN = &H8000&

Function MakeBackup (sTheFile As String, sBackupExtension As String)

    Dim sBackupFile As String


    sBackupFile = stripExt(sTheFile) + "." + sBackupExtension

    On Error GoTo MakeBackupError
    FileCopy sTheFile, sBackupFile

    MakeBackup = True
    Exit Function

MakeBackupError:
    MakeBackup = False
    Exit Function

End Function

' Purpose: Find rightmost occurrence of a character in a string
' Returns: Character position or 0 if not found
Function rat (sChar As String, sString As String)
    Dim nRetPos As Integer, nFoundPos As Integer

    If sString = "" Then
	rat = 0
    Else
	nFoundPos = 1
	Do While nFoundPos > 0
	    nFoundPos = InStr(nFoundPos, sString, sChar, 0)
	    If nFoundPos > 0 Then
		nRetPos = nFoundPos
		nFoundPos = nFoundPos + 1
	    End If
	Loop
	rat = nRetPos
    End If
End Function

'-----------------------------------------------------------------------
' Purpose: Save patches
' Returns: Clears global "modified" flags after saving
' DEBUG check what errors you get here if file is readonly, invalid patch,
' DEBUG on drive not ready etc.
Function SavePatches (sPatchFilename As String)

	Dim iCtr As Integer, nRetcode As Integer
	Dim nOuth As Integer
	Dim sPatchIniEntry As String

	If FileExists(sPatchFilename) Then
	    nRetcode = MakeBackup(sPatchFilename, "BAK")
	End If

	nOuth = FreeFile
	Open sPatchFilename For Binary Access Write As nOuth
	' Todo: error trapping for file write errors

	For iCtr = 0 To MAX_PATCHES - 1
		If gPatches(iCtr).Sysex = "" Then
		    sPatchIniEntry = Chr$(13) + Chr$(10)
		Else
			' Write entry as patches, comma, description
			sPatchIniEntry = StringToHexList(gPatches(iCtr).Sysex) + "," + gPatches(iCtr).Description + Chr$(13) + Chr$(10)
		End If
		Put #nOuth, , sPatchIniEntry
	Next
	Close #nOuth

	' Clear modified flags
	JunoLibrarian.btnSavePatch.Enabled = False
	gbFileModified = False

	SavePatches = True

End Function

' Purpose: Ask whether to save patches. If user says Yes, save them
' Returns: IDYES, IDNO, IDCANCEL
Function AskSave () As Integer
    Dim nRetcode As Integer
    nRetcode = MsgBox("Save changes to " + gsCurrentPatchFilename + "?", MB_YESNOCANCEL Or MB_DEFBUTTON1 Or MB_ICONQUESTION)

    If nRetcode = IDYES Then
	If MenuSave() = False Then
	    nRetcode = IDCANCEL
	End If
    End If
    AskSave = nRetcode
End Function

'-----------------------------------------------------------------------
' Purpose: Check that a file exists (from the VB programmer's manual)
' Returns: True if the file exists, False if it doesn't or drive isn't ready
Function FileExists (ByVal sFilename As String) As Integer

	On Error GoTo FileExistsError              ' Turn on error trapping so error handler                                                                                                                       ' responds if any error is detected.

	FileExists = (Dir$(sFilename) <> "")

	Exit Function                              ' Avoid executing error handler
							' if no error occurs.

' Branch here if ERR_DISKNOTREADY or ERR_DEVICEUNAVAILABLE
FileExistsError:
	FileExists = False
	Exit Function

End Function

'-----------------------------------------------------------------------
Function GetImportFilename () As String

	GetImportFilename = ""

	On Error GoTo cancel_err1
	JunoLibrarian.CMDialog1.CancelError = True
	JunoLibrarian.CMDialog1.DefaultExt = "SYX"
	JunoLibrarian.CMDialog1.DialogTitle = "Import"
	JunoLibrarian.CMDialog1.Filename = "*.SYX"
	JunoLibrarian.CMDialog1.Filter = "Sysex files (*.SYX)|*.SYX|All files|*.*"
	JunoLibrarian.CMDialog1.Flags = OFN_FILEMUSTEXIST Or OFN_HIDEREADONLY Or OFN_NOREADONLYRETURN
	JunoLibrarian.CMDialog1.Action = 1  ' Open

	' Can get a wildcard filespec returned if you enter a directory path, then press Cancel
	If Not InStr(JunoLibrarian.CMDialog1.Filename, "*") Then
		If FileExists(JunoLibrarian.CMDialog1.Filename) Then
			GetImportFilename = JunoLibrarian.CMDialog1.Filename
		End If
	End If

cancel_err1:
    Exit Function
	
End Function

' Purpose: Displays an Open dialog and returns a file name or an empty string
' if the user cancels
Function GetLoadFilename ()
	On Error GoTo Cancel_err2
	JunoLibrarian.CMDialog1.CancelError = True
	JunoLibrarian.CMDialog1.Filename = "*.PAT"
	JunoLibrarian.CMDialog1.DefaultExt = "PAT"
	JunoLibrarian.CMDialog1.Filter = "Patch files (*.PAT)|*.PAT|All files|*.*"
	JunoLibrarian.CMDialog1.Flags = OFN_FILEMUSTEXIST Or OFN_HIDEREADONLY Or OFN_NOREADONLYRETURN
	JunoLibrarian.CMDialog1.Action = 1  ' Open

	GetLoadFilename = JunoLibrarian.CMDialog1.Filename
	Exit Function

Cancel_err2:
    GetLoadFilename = ""
    Exit Function

End Function

Function GetSaveAsFilename (sDefaultName As String)
	'Displays a Save As dialog and returns a file name
	'or an empty string if the user cancels
	If sDefaultName = "" Or sDefaultName = UNTITLED_NAME Then
		JunoLibrarian.CMDialog1.Filename = "*.PAT"
	Else
		JunoLibrarian.CMDialog1.Filename = sDefaultName
	End If

	On Error GoTo Cancel_err3
	JunoLibrarian.CMDialog1.CancelError = True
    JunoLibrarian.CMDialog1.DefaultExt = "PAT"
	JunoLibrarian.CMDialog1.Filter = "Patch files (*.PAT)|*.PAT|All files|*.*"
	JunoLibrarian.CMDialog1.Flags = OFN_OVERWRITEPROMPT Or OFN_HIDEREADONLY
	JunoLibrarian.CMDialog1.Action = 2  ' Save As
	GetSaveAsFilename = JunoLibrarian.CMDialog1.Filename
	Exit Function

' User pressed Cancel, restore filename to caller's value
Cancel_err3:
    GetSaveAsFilename = sDefaultName
    Exit Function
    
End Function

'-----------------------------------------------------------------------
' Purpose: Load patches
' Returns:
'          gsPatches: array of binary patch data, indexed on patch number
'          gsPatchDescriptions: array of patch descriptions, indexed on patch number
'          JunoLibrarian!PatchHexList: Patch hex bytes with attached patch number,
'                          used to search for duplicate patches.
Function LoadPatches (sPatchFilename As String, bMerge As Integer)

	Dim iPatchCtr As Integer, sPatchBuf As String, sEol As String
	Dim sPatchLine As String, sPatchText As String, sPatchDescription As String
	Dim nRetcode As Integer
	Dim nStartPos As Integer, nFinishPos As Integer, nEndOfLinePos As Integer
	Dim nCommaPos As Integer
	Dim nPatchSlot As Integer
	Dim nFoundPatch As Integer
	Dim nInh As Integer

	If Not FileExists(sPatchFilename) Then
		MsgBox "Can't find patch file " + sPatchFilename
		LoadPatches = False
		Exit Function
	End If

	If Not bMerge Then
	    Erase gPatches
	    JunoLibrarian!PatchHexList.Clear
	    For iPatchCtr = 0 To MAX_PATCHES - 1
		JunoLibrarian!PatchHexList.AddItem ""
	    Next
	End If

	nInh = FreeFile
	Open sPatchFilename For Binary Access Read Shared As nInh
	' Todo: error trapping for file open problems
	
	' Read the whole file
	sPatchBuf = Input$(LOF(nInh), nInh)
	nStartPos = 1
	nFinishPos = Len(sPatchBuf)
	sEol = Chr$(13) + Chr$(10)

	' Todo: error trapping for file read errors

	Close #1

	For iPatchCtr = 0 To MAX_PATCHES - 1

		If nStartPos > nFinishPos Then Exit For

		' Extract current line
		nEndOfLinePos = InStr(nStartPos, sPatchBuf, sEol)
		If nEndOfLinePos = 0 Then nEndOfLinePos = nFinishPos
		sPatchLine = Trim$(Mid$(sPatchBuf, nStartPos, nEndOfLinePos - nStartPos))

		' Point to next line, or past end of file
		nStartPos = nEndOfLinePos + 2

		If sPatchLine <> "" Then
		    ' Description (if any) is after first comma
		    nCommaPos = InStr(1, sPatchLine, ",")
		    If nCommaPos = 0 Then
			    sPatchText = sPatchLine
			    sPatchDescription = "<indescribable>"
		    Else
			    sPatchText = Left$(sPatchLine, nCommaPos - 1)
			    sPatchDescription = Trim$(Mid$(sPatchLine, nCommaPos + 1))
		    End If
    
		    If bMerge Then
			    nPatchSlot = PatchFindBlank()
			    If nPatchSlot < 0 Then
			    MsgBox "No more empty spaces for merged patches"
			    Exit For
			    End If
		    Else
			    nPatchSlot = iPatchCtr
		    End If
    
		    nFoundPatch = 0
		    If bMerge Then
			    ' Don't merge in duplicate patches
			    nFoundPatch = ListBoxSearchExact(JunoLibrarian.PatchHexList, sPatchText)
		    End If
		    If (Not bMerge) Or (nFoundPatch = 0) Then
    
			    gPatches(nPatchSlot).Sysex = HexListToString(sPatchText)
			    gPatches(nPatchSlot).Description = sPatchDescription
    
			    ' Save the hex version to the search listbox
			    If bMerge Then
			    JunoLibrarian.PatchHexList.List(nPatchSlot) = sPatchText
			    Else
			    JunoLibrarian.PatchHexList.AddItem sPatchText
			    End If
    
		    End If

	    End If

	Next

	JunoLibrarian.btnSavePatch.Enabled = False
	If bMerge Then
	    gbFileModified = True
	Else
	    gbFileModified = False
	    Call SetFilename(sPatchFilename)
	End If
	
	PatchListLoad

	Call PatchSetCurrentNo(1)

	LoadPatches = True

End Function

' Purpose: Set global patch filename and main form's window title
Sub SetFilename (sFilename As String)
	Dim sCaption As String
	gsCurrentPatchFilename = sFilename
	sCaption = "Juno 106 Librarian"
	If gsCurrentPatchFilename <> "" Then
		sCaption = sCaption + " - " + gsCurrentPatchFilename
	End If
	JunoLibrarian.Caption = sCaption
End Sub

' Purpose: Strip extension from a filename
Function stripExt (ByVal sFilename As String) As String

    Dim nDotPos As Integer, nSlashPos As Integer, nColonPos As Integer

    nDotPos = rat(".", sFilename)
    nSlashPos = rat("\", sFilename)
    nColonPos = rat(":", sFilename)
    If nDotPos > nSlashPos And nDotPos > nColonPos Then
	sFilename = Left$(sFilename, nDotPos - 1)
    End If

    stripExt = sFilename

End Function

