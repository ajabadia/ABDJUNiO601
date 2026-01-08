VERSION 2.00
Begin Form JunoDevConfig 
   Caption         =   "Midi Device Configuration"
   ClientHeight    =   4185
   ClientLeft      =   600
   ClientTop       =   3225
   ClientWidth     =   8835
   Height          =   4590
   Left            =   540
   LinkTopic       =   "Form1"
   ScaleHeight     =   4185
   ScaleWidth      =   8835
   Top             =   2880
   Width           =   8955
   Begin Frame Frame2 
      Caption         =   "Ports"
      Height          =   2595
      Left            =   180
      TabIndex        =   11
      Top             =   180
      Width           =   7155
      Begin ListBox inputDevice 
         Height          =   1785
         Left            =   180
         TabIndex        =   0
         Top             =   600
         Width           =   3195
      End
      Begin ListBox outputDevice 
         Height          =   1785
         Left            =   3780
         TabIndex        =   1
         Top             =   600
         Width           =   3195
      End
      Begin Label Label1 
         Caption         =   "Input"
         Height          =   255
         Left            =   180
         TabIndex        =   5
         Top             =   360
         Width           =   1335
      End
      Begin Label Label2 
         Caption         =   "Output"
         Height          =   255
         Left            =   3780
         TabIndex        =   7
         Top             =   360
         Width           =   1335
      End
   End
   Begin Frame Frame1 
      Caption         =   "MIDI"
      Height          =   1095
      Left            =   3960
      TabIndex        =   8
      Top             =   2940
      Width           =   3375
      Begin ComboBox cmbMidiNote 
         Height          =   300
         Left            =   1740
         TabIndex        =   3
         Text            =   "Combo1"
         Top             =   660
         Width           =   855
      End
      Begin ComboBox cmbMidiChannel 
         Height          =   300
         Left            =   1740
         TabIndex        =   2
         Text            =   "Combo1"
         Top             =   240
         Width           =   855
      End
      Begin Label Label4 
         Caption         =   "Sample note"
         Height          =   315
         Left            =   120
         TabIndex        =   10
         Top             =   660
         Width           =   1275
      End
      Begin Label Label3 
         Caption         =   "MIDI channel"
         Height          =   315
         Left            =   120
         TabIndex        =   9
         Top             =   300
         Width           =   1275
      End
   End
   Begin CommandButton OK 
      Caption         =   "&OK"
      Default         =   -1  'True
      Height          =   375
      Left            =   7620
      TabIndex        =   4
      Top             =   180
      Width           =   1095
   End
   Begin CommandButton Cancel 
      Cancel          =   -1  'True
      Caption         =   "&Cancel"
      Height          =   375
      Left            =   7620
      TabIndex        =   6
      Top             =   660
      Width           =   1095
   End
End
Option Explicit

Sub Cancel_Click ()
    Unload Me
End Sub

Sub Form_Load ()

Dim Ctr As Integer

    ' Fill combo boxes with device lists
    For Ctr = 0 To UBound(incaps)
        inputDevice.AddItem (incaps(Ctr).szPname)
    Next
    For Ctr = 0 To UBound(outcaps)
        outputDevice.AddItem (outcaps(Ctr).szPname)
    Next

    ' Set currently-selected devices
    inputDevice.ListIndex = gInputDev.DeviceNumber
    outputDevice.ListIndex = gOutputDev.DeviceNumber

    ' Put list of MIDI channels in sample note channel combo box
    For Ctr = 1 To 16
        cmbMidiChannel.AddItem Str$(Ctr)
    Next
    If gnMidiChannel < 0 Or gnMidiChannel > 16 Then
        gnMidiChannel = 1
    End If
    cmbMidiChannel.ListIndex = gnMidiChannel - 1

    ' Put list of notes in sample note combo box
    For Ctr = 0 To 127
        cmbMidiNote.AddItem Str$(Ctr)
    Next
    If gnMidiNote < 0 Or gnMidiNote > 127 Then
        gnMidiNote = 64
    End If
    cmbMidiNote.ListIndex = gnMidiNote

End Sub

Sub OK_Click ()

    Dim nRetcode As Integer

    If gbRunning Then
        CloseDev
    End If

    If inputDevice.ListIndex >= 0 Then
        SaveIniEntry "Input", inputDevice.List(inputDevice.ListIndex)
    End If
    If outputDevice.ListIndex >= 0 Then
        SaveIniEntry "Output", outputDevice.List(outputDevice.ListIndex)
    End If

    If cmbMidiNote.ListIndex >= 0 Then
        SaveIniEntry "MIDI Note", cmbMidiNote.ListIndex
    End If
    If cmbMidiChannel.ListIndex >= 0 Then
        SaveIniEntry "MIDI Channel", cmbMidiChannel.ListIndex + 1
    End If

    nRetcode = LoadIni()
    If gbRunning Then
        OpenDev
    End If

    Unload Me

End Sub

