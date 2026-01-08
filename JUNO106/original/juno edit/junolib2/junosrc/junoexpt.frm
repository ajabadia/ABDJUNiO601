VERSION 2.00
Begin Form JunoExportSelect 
   Caption         =   "Export Patch Names to Cakewalk"
   ClientHeight    =   4320
   ClientLeft      =   1545
   ClientTop       =   1320
   ClientWidth     =   4680
   Height          =   4725
   Left            =   1485
   LinkTopic       =   "Form1"
   ScaleHeight     =   4320
   ScaleWidth      =   4680
   Top             =   975
   Width           =   4800
   Begin Frame Frame2 
      Caption         =   "Cakewalk Drive/directory"
      Height          =   2475
      Left            =   120
      TabIndex        =   6
      Top             =   1560
      Width           =   3015
      Begin DirListBox dirCakewalk 
         Height          =   1605
         Left            =   120
         TabIndex        =   2
         Top             =   360
         Width           =   2775
      End
      Begin DriveListBox drvCakewalk 
         Height          =   315
         Left            =   120
         TabIndex        =   3
         Top             =   2040
         Width           =   2775
      End
   End
   Begin Frame Frame1 
      Caption         =   "Cakewalk Version"
      Height          =   1275
      Left            =   120
      TabIndex        =   7
      Top             =   120
      Width           =   3015
      Begin OptionButton optCakewalkVersion 
         Caption         =   "v&3.0 MASTER.INS"
         Height          =   315
         Index           =   1
         Left            =   180
         TabIndex        =   1
         Top             =   720
         Width           =   2535
      End
      Begin OptionButton optCakewalkVersion 
         Caption         =   "v&2.0 PATCH.INI"
         Height          =   315
         Index           =   0
         Left            =   180
         TabIndex        =   0
         Top             =   360
         Width           =   2535
      End
   End
   Begin CommandButton OK 
      Caption         =   "&OK"
      Default         =   -1  'True
      Height          =   375
      Left            =   3420
      TabIndex        =   4
      Top             =   240
      Width           =   1095
   End
   Begin CommandButton Cancel 
      Cancel          =   -1  'True
      Caption         =   "&Cancel"
      Height          =   375
      Left            =   3420
      TabIndex        =   5
      Top             =   720
      Width           =   1095
   End
End
Option Explicit

Sub Cancel_Click ()
    Unload Me
End Sub

Sub drvCakewalk_Change ()
    On Error GoTo DriveHandler
    dirCakewalk.Path = drvCakewalk.Drive
    Exit Sub

DriveHandler:
    ' Selected drive not ready, reset to original default
    drvCakewalk.Drive = dirCakewalk.Path
    Exit Sub
    
End Sub

Sub Form_Load ()

    Dim sCakewalkDir As String, nCakewalkVersion As Integer, nRetcode As Integer

    sCakewalkDir = Space$(255)
    nRetcode = GetPrivateProfileString("Defaults", "CakewalkPath", CurDir$, sCakewalkDir, 255, INI_FILENAME)
    nCakewalkVersion = GetPrivateProfileInt("Defaults", "CakewalkVersion", 0, INI_FILENAME)

    dirCakewalk.Path = sCakewalkDir
    drvCakewalk.Drive = sCakewalkDir
    If nCakewalkVersion >= 0 And nCakewalkVersion <= 1 Then
        optCakewalkVersion(nCakewalkVersion) = True
    End If

End Sub

Sub OK_Click ()

    Dim nRetcode As Integer, sVersionNo As String, sPath As String
    
    If optCakewalkVersion(0) Then   ' Version 2.0
        ExportToCakewalkPatch dirCakewalk.Path & "\PATCHES.INI"
        sVersionNo = "0"
    Else
        ExportToCakewalkIns dirCakewalk.Path & "\MASTER.INS"
        sVersionNo = "1"
    End If

    sPath = dirCakewalk.Path
    nRetcode = WritePrivateProfileString("Defaults", "CakewalkPath", sPath, INI_FILENAME)
    nRetcode = WritePrivateProfileString("Defaults", "CakewalkVersion", sVersionNo, INI_FILENAME)

    Unload Me

End Sub

