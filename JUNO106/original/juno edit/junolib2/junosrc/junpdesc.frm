VERSION 2.00
Begin Form frmPatchDescription 
   Caption         =   "Patch Description"
   ClientHeight    =   1365
   ClientLeft      =   330
   ClientTop       =   2925
   ClientWidth     =   7365
   ControlBox      =   0   'False
   Height          =   1770
   Left            =   270
   LinkTopic       =   "Form1"
   ScaleHeight     =   1365
   ScaleWidth      =   7365
   Top             =   2580
   Width           =   7485
   Begin CommandButton btnOK 
      Caption         =   "&OK"
      Default         =   -1  'True
      Height          =   375
      Left            =   5760
      TabIndex        =   2
      Top             =   300
      Width           =   1215
   End
   Begin CommandButton btnCancel 
      Cancel          =   -1  'True
      Caption         =   "&Cancel"
      Height          =   375
      Left            =   5760
      TabIndex        =   3
      Top             =   840
      Width           =   1215
   End
   Begin TextBox PatchDescription 
      Height          =   375
      Left            =   300
      TabIndex        =   0
      Text            =   "PatchDescription"
      Top             =   600
      Width           =   5175
   End
   Begin Label Label1 
      Caption         =   "Description"
      Height          =   255
      Left            =   300
      TabIndex        =   1
      Top             =   240
      Width           =   1095
   End
End
Option Explicit

Sub btnCancel_Click ()
    Unload Me
End Sub

Sub btnOK_Click ()

        DescriptionUpdate PatchDescription.Text
        Unload Me
End Sub

Sub PatchDescription_GotFocus ()
    ' Select on entry
    PatchDescription.SelStart = 0
    PatchDescription.SelLength = Len(PatchDescription.Text)
End Sub

