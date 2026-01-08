VERSION 2.00
Begin Form JunoLibrarian 
   Caption         =   "Juno-106 Librarian"
   ClientHeight    =   2520
   ClientLeft      =   180
   ClientTop       =   810
   ClientWidth     =   7155
   Height          =   3210
   Icon            =   JUNOLIBR.FRX:0000
   Left            =   120
   LinkTopic       =   "Form1"
   ScaleHeight     =   2520
   ScaleWidth      =   7155
   Top             =   180
   Width           =   7275
   Begin CommandButton btnSavePatch 
      Caption         =   "&Save Patch"
      Enabled         =   0   'False
      Height          =   375
      Left            =   5640
      TabIndex        =   5
      Top             =   660
      Width           =   1215
   End
   Begin CommandButton btnPatchDescription 
      Caption         =   "&Description"
      Default         =   -1  'True
      Height          =   375
      Left            =   5640
      TabIndex        =   2
      Top             =   120
      Width           =   1215
   End
   Begin CommandButton Command1 
      Caption         =   "&Random"
      Height          =   375
      Left            =   5640
      TabIndex        =   4
      Top             =   1200
      Width           =   1215
   End
   Begin CommonDialog CMDialog1 
      Left            =   3600
      Top             =   840
   End
   Begin ListBox PatchHexList 
      Height          =   225
      Left            =   3000
      TabIndex        =   3
      TabStop         =   0   'False
      Top             =   300
      Visible         =   0   'False
      Width           =   1275
   End
   Begin Timer NoteTimer 
      Enabled         =   0   'False
      Interval        =   1000
      Left            =   3600
      Top             =   1380
   End
   Begin ListBox PatchList 
      Height          =   2370
      Left            =   120
      TabIndex        =   0
      Top             =   60
      Width           =   5295
   End
   Begin MsgBlaster MsgBlaster1 
      Prop8           =   "Click on ""..."" for the About Box ---->"
   End
   Begin Label MidiNotifyControl 
      Caption         =   "MidiNotifyControl (invisible)"
      Height          =   375
      Left            =   3000
      TabIndex        =   1
      Top             =   540
      Visible         =   0   'False
      Width           =   1575
   End
   Begin Menu mnuFile 
      Caption         =   "&File"
      Begin Menu mnuNew 
         Caption         =   "&New"
      End
      Begin Menu mnuOpen 
         Caption         =   "&Open..."
      End
      Begin Menu mnuMerge 
         Caption         =   "&Merge..."
      End
      Begin Menu mnuRevert 
         Caption         =   "&Revert"
      End
      Begin Menu mnuSep1 
         Caption         =   "-"
      End
      Begin Menu mnuSave 
         Caption         =   "&Save"
      End
      Begin Menu mnuSaveAs 
         Caption         =   "Save &As..."
      End
      Begin Menu mnuSep2 
         Caption         =   "-"
      End
      Begin Menu mnuImport 
         Caption         =   "&Import..."
      End
      Begin Menu mnuexport 
         Caption         =   "&Export to Cakewalk..."
      End
      Begin Menu mnuSep 
         Caption         =   "-"
      End
      Begin Menu mnuExit 
         Caption         =   "E&xit"
      End
   End
   Begin Menu mnuEdit 
      Caption         =   "&Edit"
      Begin Menu mnuCut 
         Caption         =   "Cu&t"
         Shortcut        =   ^X
      End
      Begin Menu mnuCopy 
         Caption         =   "&Copy"
         Shortcut        =   ^C
      End
      Begin Menu mnuPaste 
         Caption         =   "&Paste"
         Enabled         =   0   'False
         Shortcut        =   ^V
      End
      Begin Menu mnuDelete 
         Caption         =   "&Delete"
      End
      Begin Menu mnuSepx 
         Caption         =   "-"
      End
      Begin Menu mnuArrange 
         Caption         =   "&Arrange..."
      End
   End
   Begin Menu Options 
      Caption         =   "&Options"
      Begin Menu MidiDevices 
         Caption         =   "Midi..."
      End
   End
   Begin Menu mnuHelp 
      Caption         =   "&Help"
      Begin Menu mnuContents 
         Caption         =   "&Contents"
      End
      Begin Menu mnuSepxx 
         Caption         =   "-"
      End
      Begin Menu mnuAbout 
         Caption         =   "&About..."
      End
   End
End
Option Explicit

Sub btnPatchDescription_Click ()
    Dim nThisItem As Integer

    nThisItem = JunoLibrarian.PatchList.ListIndex
    If nThisItem >= 0 Then
	If Len(gPatches(nThisItem).Description) > 0 Then
	    frmPatchDescription.PatchDescription.Text = gPatches(nThisItem).Description
	    frmPatchDescription.Show 1
	End If
    End If
End Sub

Sub btnSavePatch_Click ()
    gPatches(gnCurrentPatch).Sysex = gsCurrentPatch
    JunoLibrarian.btnSavePatch.Enabled = False
End Sub

Sub Command1_Click ()
    If gPatches(gnCurrentPatch).Sysex <> "" Then
	If Not askyesno("Overwrite " + gPatches(gnCurrentPatch).Description + " with a random patch?") Then
	    Exit Sub
	End If
    End If
    RandomPatch
End Sub

Sub Form_Load ()

    JunoLibrSetup

End Sub

Sub Form_QueryUnload (Cancel As Integer, UnloadMode As Integer)
    If gbFileModified Then
	If AskSave() = IDCANCEL Then
	    Cancel = True
	End If
    End If
End Sub

Sub Form_Resize ()
    If Me.WindowState = 1 Then
	' Being minimized, hide the panel
	JunoPanel.Hide
    Else
	' Not being minimized, unhide the panel if necessary
	If Not JunoPanel.Visible Then
	    JunoPanel.Show 0
	End If
    End If
End Sub

Sub Form_Unload (Cancel As Integer)

    JunoLibrCleanup

    End

End Sub

Sub MidiDevices_Click ()
	JunoDevConfig.Show
End Sub

Sub MidiNotifyControl_Change ()

    midiInputArrived
    
End Sub

Sub mnuAbout_Click ()
    JunoAbout.Show 1
End Sub

Sub mnuArrange_Click ()

    junomover.Show 1
    
End Sub

Sub mnuContents_Click ()
    Dim nRetcode As Integer
    nRetcode = WinHelp(JunoLibrarian.hWnd, App.Path + "\" + App.HelpFile, HELP_CONTENTS, 0&)
End Sub

Sub mnuCopy_Click ()
    PatchCopy
End Sub

Sub mnuCut_Click ()
    PatchCut
End Sub

Sub mnuDelete_Click ()
    PatchDelete
End Sub

Sub mnuEdit_Click ()

    Dim bPatchSelected As Integer

    mnuPaste.Enabled = Clipboard.GetFormat(CF_TEXT)

    bPatchSelected = (gPatches(gnCurrentPatch).Sysex <> "")
    mnuCut.Enabled = bPatchSelected
    mnuCopy.Enabled = bPatchSelected
    mnuDelete.Enabled = bPatchSelected

End Sub

Sub mnuExit_Click ()
    Unload Me
End Sub

Sub mnuexport_Click ()
    MenuExport
End Sub

Sub mnuImport_Click ()
    MenuImport
End Sub

Sub mnuMerge_Click ()
    MenuMerge
End Sub

Sub mnuNew_Click ()
    MenuNew
End Sub

Sub mnuOpen_Click ()

    MenuOpen

End Sub

Sub mnuPaste_Click ()
    PatchPaste
End Sub

Sub mnuRevert_Click ()

    MenuRevert

End Sub

Sub mnuSave_Click ()
    
    Dim nRetcode As Integer

    nRetcode = MenuSave()

End Sub

Sub mnuSaveAs_Click ()
    Dim nRetcode As Integer
    nRetcode = MenuSaveAs()
End Sub

Sub MsgBlaster1_Message (MsgVal As Integer, wParam As Integer, lParam As Long, ReturnVal As Long)
	Call midiCallbackWndProc(MsgVal, wParam, lParam)
End Sub

Sub NoteTimer_Timer ()

	NoteOff

End Sub

Sub PatchList_Click ()

    If gbPanelLive Then
	' Stop changes caused by switching to new patch from triggering individual controls
	gbPanelLive = False
	Call PatchChange(PatchList.ListIndex)
	gbPanelLive = True
    End If
    
End Sub

Sub PatchList_DblClick ()
    btnPatchDescription_Click
End Sub

Sub startstop_Click ()

    startAndStop

End Sub

Sub UpdateDescription_Click ()
End Sub

