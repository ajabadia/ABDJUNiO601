VERSION 2.00
Begin Form JunoMover 
   BorderStyle     =   1  'Fixed Single
   Caption         =   "Patch Arranger"
   ClientHeight    =   3435
   ClientLeft      =   165
   ClientTop       =   930
   ClientWidth     =   9315
   Height          =   3840
   Left            =   105
   LinkTopic       =   "Form1"
   ScaleHeight     =   3435
   ScaleWidth      =   9315
   Top             =   585
   Width           =   9435
   Begin PictureBox picNo 
      AutoSize        =   -1  'True
      Height          =   510
      Left            =   6540
      Picture         =   JUNOMOVE.FRX:0000
      ScaleHeight     =   480
      ScaleWidth      =   480
      TabIndex        =   15
      TabStop         =   0   'False
      Top             =   2880
      Visible         =   0   'False
      Width           =   510
   End
   Begin PictureBox picDropPatch 
      AutoSize        =   -1  'True
      Height          =   510
      Left            =   5880
      Picture         =   JUNOMOVE.FRX:0302
      ScaleHeight     =   480
      ScaleWidth      =   480
      TabIndex        =   14
      TabStop         =   0   'False
      Top             =   2880
      Visible         =   0   'False
      Width           =   510
   End
   Begin PictureBox picDragPatch 
      AutoSize        =   -1  'True
      Height          =   510
      Left            =   5280
      Picture         =   JUNOMOVE.FRX:0604
      ScaleHeight     =   480
      ScaleWidth      =   480
      TabIndex        =   13
      TabStop         =   0   'False
      Top             =   2880
      Visible         =   0   'False
      Width           =   510
   End
   Begin CommandButton btnDelete 
      Caption         =   "&Delete"
      Height          =   375
      Left            =   1080
      TabIndex        =   8
      Top             =   2940
      Width           =   855
   End
   Begin CommandButton btnRemoveAll 
      Caption         =   "Remove All"
      Height          =   375
      Left            =   4140
      TabIndex        =   4
      Top             =   2280
      Width           =   1095
   End
   Begin CommandButton btnMoveAll 
      Caption         =   "All ->"
      Height          =   375
      Left            =   4140
      TabIndex        =   2
      Top             =   1020
      Width           =   1095
   End
   Begin ListBox lstTemp 
      Height          =   225
      Left            =   4200
      Sorted          =   -1  'True
      TabIndex        =   12
      TabStop         =   0   'False
      Top             =   2940
      Visible         =   0   'False
      Width           =   1035
   End
   Begin CommandButton btnSort 
      Caption         =   "S&ort"
      Height          =   375
      Left            =   2700
      TabIndex        =   9
      Top             =   2940
      Width           =   855
   End
   Begin CommandButton btnCancel 
      Cancel          =   -1  'True
      Caption         =   "&Cancel"
      Height          =   375
      Left            =   7080
      TabIndex        =   6
      Top             =   2940
      Width           =   855
   End
   Begin CommandButton btnOk 
      Caption         =   "&Ok"
      Height          =   375
      Left            =   8280
      TabIndex        =   7
      Top             =   2940
      Width           =   855
   End
   Begin CommandButton btnMoveToSource 
      Caption         =   "Remove"
      Height          =   375
      Left            =   4140
      TabIndex        =   3
      Top             =   1800
      Width           =   1095
   End
   Begin CommandButton btnMoveToDest 
      Caption         =   "Move ->"
      Height          =   375
      Left            =   4140
      TabIndex        =   1
      Top             =   540
      Width           =   1095
   End
   Begin ListBox lstDest 
      Height          =   2370
      Left            =   5340
      MultiSelect     =   2  'Extended
      TabIndex        =   5
      Top             =   420
      Width           =   3795
   End
   Begin ListBox lstSource 
      Height          =   2370
      Left            =   240
      MultiSelect     =   2  'Extended
      TabIndex        =   0
      Top             =   420
      Width           =   3795
   End
   Begin Label Label2 
      Caption         =   "Rearranged patches"
      Height          =   195
      Left            =   5580
      TabIndex        =   11
      Top             =   180
      Width           =   2835
   End
   Begin Label Label1 
      Caption         =   "Current patches"
      Height          =   195
      Left            =   240
      TabIndex        =   10
      Top             =   180
      Width           =   1815
   End
End
Option Explicit

Const DRAG_ENTER = 0
Const DRAG_LEAVE = 1
Const DRAG_OVER = 2

Sub btnCancel_Click ()
    Unload Me
End Sub

Sub btnDelete_Click ()
    Call ListDelete(lstSource)
End Sub

Sub btnMoveAll_Click ()
    Call ListMoveAll(lstSource, lstDest)
End Sub

Sub btnMoveToDest_Click ()
    Call ListMove(lstSource, lstDest)
End Sub

Sub btnMoveToSource_Click ()
    Call ListMove(lstDest, lstSource)
End Sub

Sub btnOK_Click ()
    MoverSave lstDest
    Unload Me
End Sub

Sub btnRemoveAll_Click ()
    Call ListMoveAll(lstDest, lstSource)
End Sub

Sub btnSort_Click ()
    Call ListSort(lstSource, lstTemp)
End Sub

Sub Form_Load ()
    ' Load all the patch descriptions into the source list with attached patch number in Itemdata
    Dim iCtr As Integer, sThisDesc As String

    For iCtr = 0 To MAX_PATCHES - 1
        If Len(gPatches(iCtr).Sysex) > 0 Then
            If Len(gPatches(iCtr).Description) = 0 Then
                sThisDesc = "No description"
            Else
                sThisDesc = gPatches(iCtr).Description
            End If
            junomover.lstSource.AddItem sThisDesc
            junomover.lstSource.ItemData(junomover.lstSource.NewIndex) = iCtr
        End If
    Next

    ' Set initial drag icons for patch listboxes
    lstSource.DragIcon = picDragPatch.Picture
    lstDest.DragIcon = picDragPatch.Picture

End Sub

Sub lstDest_DblClick ()
    Call ListMove(lstDest, lstSource)
End Sub

Sub lstDest_DragDrop (Source As Control, x As Single, y As Single)
    If TypeOf Source Is ListBox Then
        If Source Is lstSource Then
            Call ListMove(lstSource, lstDest)
        End If
    End If
End Sub

Sub lstDest_DragOver (Source As Control, x As Single, y As Single, State As Integer)
    Select Case State
    Case DRAG_ENTER
        If lstDest Is ActiveControl Then
            Source.DragIcon = picDragPatch.Picture
        Else
            Source.DragIcon = picDropPatch.Picture
        End If
            
    Case DRAG_LEAVE
        Source.DragIcon = picNo.Picture
    End Select

End Sub

Sub lstDest_MouseDown (Button As Integer, Shift As Integer, x As Single, y As Single)
    lstDest.Drag
End Sub

Sub lstSource_DblClick ()
    Call ListMove(lstSource, lstDest)
End Sub

Sub lstSource_DragDrop (Source As Control, x As Single, y As Single)
    If TypeOf Source Is ListBox Then
        If Source Is lstDest Then
            Call ListMove(lstDest, lstSource)
        End If
    End If
End Sub

Sub lstSource_DragOver (Source As Control, x As Single, y As Single, State As Integer)
    Select Case State
    Case DRAG_ENTER
        If lstSource Is ActiveControl Then
            Source.DragIcon = picDragPatch.Picture
        Else
            Source.DragIcon = picDropPatch.Picture
        End If
            
    Case DRAG_LEAVE
        Source.DragIcon = picNo.Picture
    End Select

End Sub

Sub lstSource_MouseDown (Button As Integer, Shift As Integer, x As Single, y As Single)
    lstSource.Drag
End Sub

