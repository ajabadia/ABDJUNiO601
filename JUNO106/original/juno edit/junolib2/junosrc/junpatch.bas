' $Header:   D:/misc/midi/vcs/junpatch.bas   1.12   30 Oct 1994 12:32:02   DAVEC  $
' Patch manipulation for Juno Librarian
Option Explicit
' Sample note to play when patch is changed
Const SAMPLENOTE = &H3C
Const SAMPLEVELOCITY = &H40

' Purpose: Change some bits in the patch and send a control message to the synth
' nByteNumber: Number of byte in patch to change
' nBitMask: Mask that sets bits to 0 when Anded with the original byte
' nBitValue: New value for bits, premultipled to correct position in byte
Sub ChangeControlBit (nByteNumber As Integer, nBitMask As Integer, nBitValue As Integer)

	Dim nPatchValue As Integer

	If gbPanelLive And gsCurrentPatch <> "" Then

		' Mask out previous value and insert new value
		nPatchValue = (Asc(Mid$(gsCurrentPatch, nByteNumber, 1)) And nBitMask) Or nBitValue

		' Send the message. Note the -1 is because the byte numbers are relative to the
		' start of the patch string (byte 1) while the control numbers start at 0
		Call ControlMsgSend(nByteNumber - 1, nPatchValue)

	End If


End Sub

Sub ControlMsgArrived (sMidiData As String, bDisplayUpdate As Integer)

	If gsCurrentPatch <> "" Then

		' Modify current patch with control message change, and update patch display
		gsCurrentPatch = ControlMsgToPatch(sMidiData, gsCurrentPatch, bDisplayUpdate)

		JunoLibrarian.btnSavePatch.Enabled = True

	End If

End Sub

'------------------------------------------------------------------
' Purpose: Send a panel control message to the synth
Sub ControlMsgSend (nMessageNumber As Integer, nMessageValue As Integer)
	
	Dim scontrolmsg As String, nRetcode As Integer

	If gOutputDev.handle >= 0 Then
		scontrolmsg = Chr$(MIDI_BEGINSYSEX) & Chr$(ROLAND_ID) & Chr$(SYSEX_CONTROL) & Chr$(0) & Chr$(nMessageNumber) & Chr$(nMessageValue) & Chr$(MIDI_ENDSYSEX)
		nRetcode = vbMidiOutLongMsg(gOutputDev.handle, scontrolmsg)

		Debug.Print "Sent " & stringtohexlist(scontrolmsg)

		Call ControlMsgArrived(scontrolmsg, False)
	End If

End Sub

'------------------------------------------------------------------
' Purpose: Merge control change sysex message into current patch
Function ControlMsgToPatch (scontrolmsg As String, ByVal sPatch As String, bDisplayUpdate As Integer) As String

	Dim nControlNo As Integer, nControlValue As Integer
	
	nControlNo = Asc(Mid$(scontrolmsg, CONTROL_NO_IN_MSG, 1))
	nControlValue = Asc(Mid$(scontrolmsg, CONTROL_VAL_IN_MSG, 1))

	If nControlNo < PATCH_LENGTH Then
		' Stuff new byte into patch (+1 because control number is zero-based)
		Mid$(sPatch, nControlNo + 1) = Chr$(nControlValue)
	End If

	If bDisplayUpdate Then
		' If it's a slider message, just update the affected slider
		' Note that the slider Index numbers on the panel must match
		' their positions in the patch
		If nControlNo < 16 Then
			JunoPanel.Slider(nControlNo) = 127 - nControlValue
		Else
			' Update all the switches
			Call PatchToPanelSwitches(sPatch)
		End If
	End If
	
	' Return modified patch
	ControlMsgToPatch = sPatch
			
End Function

'------------------------------------------------------------------
' Purpose: Turn the sample note off
Sub NoteOff ()

	Dim nRetcode As Integer

	' Stop the timer
	JunoLibrarian!NoteTimer.Enabled = False

	' Stop the note by sending a zero-velocity note on, the way the Juno keyboard does
    nRetcode = MidiPut(gOutputDev.handle, Chr$(MIDI_NOTEON + gnMidiChannel - 1) + Chr$(gnMidiNote) + Chr$(0))
	
	' Send an All notes off controller message
    nRetcode = MidiPut(gOutputDev.handle, Chr$(MIDI_CONTROLCHANGE + gnMidiChannel - 1) + Chr$(MIDI_ALLNOTESOFF) + Chr$(0))

End Sub

'------------------------------------------------------------------
' Play a sample note
Sub NotePlay ()

	Dim startTime As Long
	Dim nRetcode As Integer

	If gOutputDev.handle > 0 Then

		' Send a Note On message
	nRetcode = MidiPut(gOutputDev.handle, Chr$(MIDI_NOTEON + gnMidiChannel - 1) + Chr$(gnMidiNote) + Chr$(SAMPLEVELOCITY))

		' Restart the note timer, note will be turned off when it times out
		JunoLibrarian!NoteTimer.Enabled = False
		JunoLibrarian!NoteTimer.Enabled = True

	End If
	
End Sub

' Purpose: Add sysex header and end byte to patch data
Function PatchAddHeader (ByVal sPatch As String) As String
    ' CHR$(0) is for the patch number (synth ignores it anyway)
    PatchAddHeader = SYSEX_PATCH_HEADER + Chr$(gnMidiChannel - 1) + Chr$(0) + sPatch + Chr$(MIDI_ENDSYSEX)
End Function

'-----------------------------------------------------------------------
' Purpose: Handle incoming patch from synth
Sub PatchArrived (sPatch As String)

	Dim nThisPatch As Integer, nFoundPatch As Integer, nPatchChannel As Integer
	Dim sPatchHex As String, sTempPatchHex As String

	' Quit if it's not a patch
	If Left$(sPatch, Len(SYSEX_PATCH_HEADER)) <> SYSEX_PATCH_HEADER Or Right$(sPatch, 1) <> Chr$(MIDI_ENDSYSEX) Then
	    Exit Sub
	End If

	' Get synth's number for this patch
	nThisPatch = Asc(Mid$(sPatch, PATCH_NO_IN_SYSEX_PATCH, 1))
	' This is a trap for a problem reported in the beta:
	' incoming patches were causing an array out of bounds error. No idea why.
	If nThisPatch > MAX_PATCHES Then
		MsgBox "PatchArrived: Weird patch number " & nThisPatch & " received." & Chr$(13) & Chr$(10) & "Patch data: " & stringtohexlist(sPatch)
		Exit Sub
	End If

	' Get synth MIDI channel
	nPatchChannel = Asc(Mid$(sPatch, MIDI_CHANNEL_IN_SYSEX_PATCH, 1))
	' Fix the configuration
	If nPatchChannel + 1 <> gnMidiChannel And nPatchChannel < 16 Then
	    If AskYesNo("Juno-106 is using MIDI channel " & (nPatchChannel + 1) & ", update the Librarian's MIDI channel configuration?") Then
		gnMidiChannel = nPatchChannel + 1
		SaveIniEntry "MIDI Channel", Str$(gnMidiChannel)
	    End If
	End If

	' Get patch out of sysex message
	sPatch = Mid$(sPatch, SYSEX_PATCH_HEADER_LENGTH + 1, PATCH_LENGTH)

	' Warn if current patch has changed
	If JunoLibrarian.btnSavePatch.Enabled Then

		If AskYesNo("Changes to current patch will be lost, continue?") Then
		    JunoLibrarian.btnSavePatch.Enabled = False
		Else
		    ' Re-send current patch, otherwise synth will be set to its internal patch
		    Call PatchToSynth(gsCurrentPatch)
		    Exit Sub
		End If

	End If
	
	sPatchHex = stringtohexlist(sPatch)

	' Check for duplicate patch in another position
	' Make hex list, search for it, if patch number matches incoming one
	' search all patches after incoming one.
	' Take the incoming patch's data out of the list so we won't find it
	' It'll be replaced by the new data below
	JunoLibrarian!PatchHexList.List(nThisPatch) = ""

	nFoundPatch = ListBoxSearchExact(JunoLibrarian.PatchHexList, sPatchHex)
	If nFoundPatch > 0 Then
		MsgBox "Note: Patch " + PatchNumberText(nThisPatch) + " in the synth is the same as patch " + PatchNumberText(nFoundPatch) + " in the library"
	End If

	' If stored patch is different from incoming patch:
	If gPatches(nThisPatch).Sysex <> "" And sPatch <> gPatches(nThisPatch).Sysex Then

		If Not AskYesNo("Patch " + PatchNumberText(nThisPatch) + " has changed in the synth, update library?") Then

			' Don't proceed with updating, and set synth back to current patch
			Call PatchSetCurrentNo(nThisPatch)
			Exit Sub

		End If

	End If

	' Save patch data
	gPatches(nThisPatch).Sysex = sPatch

	gnCurrentPatch = nThisPatch
	gsCurrentPatch = sPatch
	JunoLibrarian!PatchList.ListIndex = gnCurrentPatch
	Call PatchToPanel(sPatch)

	' Put new hex data into search listbox
	JunoLibrarian!PatchHexList.List(nThisPatch) = sPatchHex
	
	' Set default description
	If gPatches(nThisPatch).Description = "" Then
		Call DescriptionUpdate("Undescribed patch")
	End If

End Sub

'-----------------------------------------------------------------------
' Set patch to patch selected from list, display patch and play a note
Sub PatchChange (ByVal nNewPatch As Integer)

    Dim bSaveLive As Integer

    ' Disable panel change events
    bSaveLive = gbPanelLive
    gbPanelLive = False

	' Make sure it's quiet
	NoteOff

	' Avoid accidentally losing patch changes
	If JunoLibrarian.btnSavePatch.Enabled Then
	    If Not AskYesNo("Changes to current patch will be lost, continue?") Then
		' Set list back to current (but edited) patch
		JunoLibrarian.PatchList.ListIndex = gnCurrentPatch
		Exit Sub
	    End If
	End If

	' Display patch and send it to the synth
	Call PatchSetCurrentNo(nNewPatch)

	If gPatches(nNewPatch).Sysex <> "" Then
		' Play a note on the synth for this patch
		 NotePlay
	End If

    gbPanelLive = bSaveLive

End Sub

' Purpose: Find a blank patch slot
' Returns: Number of free slot, or -1 if no slots available
Function PatchFindBlank () As Integer

	Dim iCtr As Integer
	PatchFindBlank = -1
	For iCtr = 0 To MAX_PATCHES - 1
	If gPatches(iCtr).Sysex = "" Then
		PatchFindBlank = iCtr
		Exit For
	End If
	Next

End Function

'-----------------------------------------------------------------------
' Purpose: Copy patch descriptions from global array to list
Sub PatchListLoad ()

	Dim iPatchCtr As Integer, sThisDesc As String

    ' Stop list updates from triggering Click events
    gbPanelLive = False

	JunoLibrarian!PatchList.Clear

	For iPatchCtr = 0 To MAX_PATCHES - 1

		sThisDesc = PatchNumberText(iPatchCtr)
		If gPatches(iPatchCtr).Description = "" Then
			sThisDesc = sThisDesc + " <empty>"
		Else
			sThisDesc = sThisDesc + " " + gPatches(iPatchCtr).Description
		End If

		JunoLibrarian!PatchList.AddItem sThisDesc

	Next

    gbPanelLive = True

End Sub

'-----------------------------------------------------------------------
' Purpose: Convert JUNOLIBR internal patch number to Juno patch number
' (e.g. 0 becomes "A11")
Function PatchNumberText (ByVal nPatch As Integer) As String

	Dim retString As String

	If nPatch < 64 Then
		retString = "A"
	Else
		retString = "B"
		nPatch = nPatch - 64
	End If

	' Number is "octal plus 11" version of integer
	retString = retString + Chr(49 + (nPatch \ 8)) + Chr(49 + (nPatch Mod 8))

	PatchNumberText = retString

End Function

'-----------------------------------------------------------------------
' Set panel patch and send patch to synth
Sub PatchSetCurrentNo (ByVal nPatch As Integer)
    Dim bIsPatch As Integer

	gnCurrentPatch = nPatch
	gsCurrentPatch = gPatches(nPatch).Sysex
	JunoLibrarian.btnSavePatch.Enabled = False

    bIsPatch = (gsCurrentPatch <> "")
    If bIsPatch Then
		JunoPanel.Caption = gPatches(nPatch).Description
		Call PatchToPanel(gsCurrentPatch)
		Call PatchToSynth(gsCurrentPatch)
    End If

    ' Disable description button and Cut and Copy menu items if patch is empty
    JunoLibrarian.btnPatchDescription.Enabled = bIsPatch
    JunoLibrarian.mnuCut = bIsPatch
    JunoLibrarian.mnuCopy = bIsPatch

End Sub

'------------------------------------------------------------------
' Copy patch data to panel
Sub PatchToPanel (sPatch As String)

	Dim nPatchByte As Integer
	Dim iCtrlCtr As Integer

	gbPanelLive = False

	If Len(sPatch) = PATCH_LENGTH Then

		' Slider control array Index numbers have been tweaked to match
		' the corresponding byte position in the patches
		For iCtrlCtr = 1 To 16
			JunoPanel.Slider(iCtrlCtr - 1) = 127 - Asc(Mid$(sPatch, iCtrlCtr))
		Next

		Call PatchToPanelSwitches(sPatch)

	End If

	gbPanelLive = True

End Sub

'------------------------------------------------------------------
' Purpose: Set non-slider controls to patch values
Sub PatchToPanelSwitches (sPatch As String)

	Dim nPatchByte As Integer

		nPatchByte = Asc(Mid$(sPatch, CONTROLBYTE_1))
		' Range is bits 1-3, 16'=1, 8'=2, 4'=4
		Select Case (nPatchByte And 3)
		Case 1
			JunoPanel.DCORange(0).Value = True
		Case 2
			JunoPanel.DCORange(1).Value = True
		Case Else
			JunoPanel.DCORange(2).Value = True
		End Select

		' Waveform is bits 4 and 5
		JunoPanel.DCOPulseWave.Value = (nPatchByte And 8) / 8
		JunoPanel.DCOTriangleWave.Value = (nPatchByte And 16) / 16

		' Chorus value is bits 5 and 6
		nPatchByte = (nPatchByte \ 32) And 3
		Select Case nPatchByte
		Case 0  ' Level 2
			JunoPanel.Chorus(2).Value = True
		Case 1  ' Off
			JunoPanel.Chorus(0).Value = True
		Case Else  ' Level 1 (patch byte value should be 2)
			JunoPanel.Chorus(1).Value = True
		End Select

		nPatchByte = Asc(Mid$(sPatch, CONTROLBYTE_2))
		If nPatchByte And 1 Then
			JunoPanel.DCOPWMCtrl(1).Value = True ' LFO
		Else
			JunoPanel.DCOPWMCtrl(0).Value = True ' Manual
		End If

		If nPatchByte And 2 Then
			JunoPanel.VCFEnvPolarity(1).Value = True                 ' Negative
		Else
			JunoPanel.VCFEnvPolarity(0).Value = True                 ' Positive
		End If


		If nPatchByte And 4 Then
			JunoPanel.VCAControl(1).Value = True ' Gate
		Else
			JunoPanel.VCAControl(0).Value = True ' Env
		End If

		JunoPanel.HPF.Value = (nPatchByte \ 8) And 3

End Sub

'-----------------------------------------------------------------------
Sub PatchToSynth (sPatch As String)

	Dim nRetcode As Integer, sSendPatch As String

	If gOutputDev.handle > 0 And Len(sPatch) = PATCH_LENGTH Then

		sSendPatch = PatchAddHeader(sPatch)
	nRetcode = vbMidiOutLongMsg(gOutputDev.handle, sSendPatch)

	End If
	
End Sub

' Purpose: Generate a random patch by setting controls to random values
' in their range
Sub RandomPatch ()

	Dim iCtr As Integer, thisControl As Control, nOffset As Integer

	' Iterate through each control on the panel
	For iCtr = 0 To JunoPanel.Controls.Count - 1

		Set thisControl = JunoPanel.Controls(iCtr)
		If TypeOf thisControl Is VScrollBar Then

			' Set each slider to a random value within its range
			thisControl.Value = Int((thisControl.Max - thisControl.Min + 1) * Rnd + thisControl.Min)

		ElseIf TypeOf thisControl Is OptionButton Then

			' Set a random member of the option group to True
			If thisControl.Index = 0 And Val(thisControl.Tag) > 0 Then

				' The Tag of the first element in each option button control array
				' has been manually set to the number of controls in the
				' array because there doesn't seem to be a way to read this
				' at runtime
				nOffset = Int(Val(thisControl.Tag) * Rnd)
				JunoPanel.Controls(iCtr + nOffset).Value = True

			End If

		ElseIf TypeOf thisControl Is CheckBox Then

			' Set to random on/off
			thisControl.Value = Int(Rnd + .5)

		End If
	Next

	Call DescriptionUpdate("Random Patch")
	JunoLibrarian.btnSavePatch.Enabled = True
	
End Sub

' Purpose: Convert control panel slider Movement into Juno sysex control message and send it
Sub SliderChangeToControlMsg (nSlider As Integer, ByVal nValue As Integer)

	Dim scontrolmsg As String, nRetcode As Integer
	
	If nSlider < 16 And nValue >= 0 And nValue <= 127 Then
		Call ControlMsgSend(nSlider, 127 - nValue)
	Else
		Debug.Print "Invalid slider value at SliderChangeToControlMsg"
	End If

End Sub

