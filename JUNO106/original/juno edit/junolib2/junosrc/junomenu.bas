' $Header:   D:/misc/midi/vcs/junomenu.bas   1.7   15 Jan 1995 21:00:14   DAVEC  $
Option Explicit

' Purpose: Export to Cakewalk
Sub MenuExport ()

    JunoExportSelect.Show 1

End Sub

Sub MenuImport ()

	Dim sImportFile As String, nFileno As Integer
	Dim sNewpatch As String, nNewPatch As Integer
	Dim sChar As String, sPatch As String, sPatchHex As String

	' Stop patch updates affecting control panel
	gbPanelLive = False

	sImportFile = GetImportFilename()
	If sImportFile <> "" Then
		nFileno = FreeFile
		Open sImportFile For Binary Access Read As nFileno
	
		' Find first sysex start character
		Do While Not EOF(nFileno)
			sChar = Input$(1, nFileno)
			If sChar = "" Then
				Exit Do
			End If

			If Asc(sChar) = MIDI_BEGINSYSEX Then
				' Read rest of patch
				sPatch = sChar + Input(SYSEX_PATCH_LENGTH - 1, nFileno)
				If Len(sPatch) < SYSEX_PATCH_LENGTH Then
					' Got EOF before end of patch
					Exit Do
				End If
				' Check that it's got the right bits on it
				If Left(sPatch, Len(SYSEX_PATCH_HEADER)) = SYSEX_PATCH_HEADER And Right$(sPatch, 1) = Chr$(MIDI_ENDSYSEX) Then
					' Find blank space for patch
					nNewPatch = PatchFindBlank()
					If nNewPatch >= 0 Then
						' Strip off header and trailer
						sPatch = Mid$(sPatch, SYSEX_PATCH_HEADER_LENGTH + 1, PATCH_LENGTH)
						gPatches(nNewPatch).Sysex = sPatch
						gPatches(nNewPatch).Description = "Imported patch"
						' Save the hex version to the search listbox
						sPatchHex = StringToHexList(sPatch)
						JunoLibrarian.PatchHexList.List(nNewPatch) = sPatchHex
					Else
						MsgBox "No more empty spaces for imported patches"
						Exit Do
					End If
				End If
			End If
		Loop
		Close nFileno
		PatchListLoad
		gbFileModified = True
	End If

	gbPanelLive = True

End Sub

Sub MenuMerge ()
    Dim sFilename As String, bRetcode As Integer

    sFilename = GetLoadFilename()
    If sFilename <> "" Then
	bRetcode = LoadPatches(sFilename, True)
    End If

End Sub

Sub MenuNew ()

	Dim iCtr As Integer

    If gbFileModified Then
	If Not AskYesNo("Lose changes to " + gsCurrentPatchFilename + "?") Then
	    Exit Sub
	End If
    End If

	Erase gPatches

	JunoLibrarian!PatchHexList.Clear
	For iCtr = 0 To MAX_PATCHES - 1
		JunoLibrarian!PatchHexList.AddItem ""
	Next

	JunoLibrarian.btnSavePatch.Enabled = False
	gbFileModified = False
	Call SetFilename(UNTITLED_NAME)
	  
	PatchListLoad
	
	Call PatchSetCurrentNo(1)

End Sub

Sub MenuOpen ()
	Dim sFilename As String, bRetcode As Integer

	' Allow user to cancel file open if current file has been modified
	If gbFileModified Then
	    If AskSave() = IDCANCEL Then
		Exit Sub
	    End If
	End If

	sFilename = GetLoadFilename()
	If sFilename <> "" Then
		bRetcode = LoadPatches(sFilename, False)
	End If
End Sub

Sub MenuRevert ()

    Dim nretcode As Integer

    If gbFileModified Then
	If Not AskYesNo("Lose changes to " + gsCurrentPatchFilename + "?") Then
	    Exit Sub
	End If
    End If

    If gsCurrentPatchFilename <> "" Then

	nretcode = LoadPatches(gsCurrentPatchFilename, False)

    End If

End Sub

Function MenuSave ()

	If gsCurrentPatchFilename = UNTITLED_NAME Or gsCurrentPatchFilename = "" Then
		MenuSave = MenuSaveAs()
	Else
		MenuSave = SavePatches(gsCurrentPatchFilename)

	End If

End Function

' Purpose: Get filename to save as, and save
' Returns: True if successful, False if user cancels
Function MenuSaveAs ()

	Dim sNewname As String

	MenuSaveAs = False

	sNewname = GetSaveAsFilename(gsCurrentPatchFilename)
	If Len(sNewname) <> 0 And sNewname <> UNTITLED_NAME Then
		If SavePatches(sNewname) Then
		    Call SetFilename(sNewname)
		    MenuSaveAs = True
		End If
	End If

End Function

Sub PatchCopy ()
	If gnCurrentPatch >= 0 Then
		If gsCurrentPatch <> "" Then
			' Copy patch and description to clipboard
			Clipboard.SetText StringToHexList(PatchAddHeader(gsCurrentPatch)) + "," + gPatches(gnCurrentPatch).Description
			
			JunoLibrarian!mnuPaste.Enabled = True
		End If
	End If

End Sub

' Purpose: Cut current patch
Sub PatchCut ()

	If gnCurrentPatch >= 0 Then
		If gsCurrentPatch <> "" Then
			' Copy to clipboard
			PatchCopy
			' Blank patch
			gPatches(gnCurrentPatch).Sysex = ""
			gsCurrentPatch = ""

			' Blank description
			Call DescriptionUpdate("")

			' Reset menu and enabled flags, etc.
			Call PatchSetCurrentNo(gnCurrentPatch)
			gbFileModified = True
		End If
	End If

End Sub

' Purpose: Delete current patch
Sub PatchDelete ()

	If gnCurrentPatch >= 0 Then
		' Blank patch
		gPatches(gnCurrentPatch).Sysex = ""
		' Blank patch hex
		JunoLibrarian!PatchHexList.List(gnCurrentPatch) = ""
		' Blank description
		Call DescriptionUpdate("")
	End If

End Sub

' Purpose: Paste current patch
Sub PatchPaste ()

    Dim sClipText As String, nCommaPos As Integer, sHeaderText As String, sPatchDescription As String

	If gnCurrentPatch >= 0 Then
		If gPatches(gnCurrentPatch).Sysex <> "" Then
			If Not AskYesNo("Overwrite " + gPatches(gnCurrentPatch).Description + "?") Then
				Exit Sub
			End If
		End If
		' Get patch from clipboard
		' Description (if any) is after first comma
		sClipText = Clipboard.GetText()
		' Check that it's probably a patch
		sHeaderText = StringToHexList(SYSEX_PATCH_HEADER)
		If Left$(sClipText, Len(sHeaderText)) = sHeaderText Then
		    nCommaPos = InStr(1, sClipText, ",")
		    If nCommaPos = 0 Then
			   sPatchDescription = "<indescribable>"
		    Else
			   sPatchDescription = Trim$(Mid$(sClipText, nCommaPos + 1))
			   sClipText = Left$(sClipText, nCommaPos - 1)
		    End If
		    sClipText = HexListToString(sClipText)
		    If Len(sClipText) = SYSEX_PATCH_LENGTH Then
			' Remove sysex header and trailer from the patch
			gPatches(gnCurrentPatch).Sysex = Mid$(sClipText, SYSEX_PATCH_HEADER_LENGTH + 1, PATCH_LENGTH)
			Call DescriptionUpdate(sPatchDescription)
			Call PatchChange(gnCurrentPatch)
		    End If
		End If
	End If

End Sub

