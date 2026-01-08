VERSION 2.00
Begin Form JunoAbout 
   BackColor       =   &H00808080&
   BorderStyle     =   0  'None
   ClientHeight    =   2370
   ClientLeft      =   1095
   ClientTop       =   1485
   ClientWidth     =   3180
   ControlBox      =   0   'False
   ForeColor       =   &H00000000&
   Height          =   2775
   Left            =   1035
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   2370
   ScaleWidth      =   3180
   Top             =   1140
   Width           =   3300
   Begin Timer tmrSplashScreen 
      Enabled         =   0   'False
      Interval        =   1000
      Left            =   2580
      Top             =   1860
   End
   Begin CommandButton btnOK 
      Cancel          =   -1  'True
      Caption         =   "&OK"
      Default         =   -1  'True
      Height          =   375
      Left            =   1080
      TabIndex        =   4
      Top             =   1860
      Visible         =   0   'False
      Width           =   1035
   End
   Begin Line Line2 
      BorderColor     =   &H000000FF&
      BorderWidth     =   3
      X1              =   0
      X2              =   3180
      Y1              =   1740
      Y2              =   1740
   End
   Begin Line Line1 
      BorderColor     =   &H000000FF&
      BorderWidth     =   3
      X1              =   0
      X2              =   3180
      Y1              =   960
      Y2              =   960
   End
   Begin Label Label4 
      Alignment       =   2  'Center
      BackColor       =   &H00808080&
      Caption         =   "Version 2.0"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   9.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   780
      TabIndex        =   3
      Top             =   600
      Width           =   1635
   End
   Begin Label Label3 
      BackColor       =   &H00808080&
      Caption         =   "davec@wsti.demon.co.uk"
      ForeColor       =   &H00FFFFFF&
      Height          =   315
      Left            =   480
      TabIndex        =   2
      Top             =   1380
      Width           =   2235
   End
   Begin Label Label2 
      BackColor       =   &H00808080&
      Caption         =   "Public Domain by David Churcher"
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   120
      TabIndex        =   1
      Top             =   1140
      Width           =   3015
   End
   Begin Label Label1 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "Juno-106 Librarian"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   435
      Left            =   0
      TabIndex        =   0
      Top             =   0
      Width           =   3195
   End
End
Option Explicit

Sub btnOK_Click ()
    Unload Me
End Sub

Sub Form_Load ()
    ' I copied this whole Aboutbox/splash screen thing from
    ' Arthur Edstrom's VBPIANO example. Thanks, Arthur. :-)
                                                           
    Move (Screen.Width - Width) \ 2, (Screen.Height - Height) \ 2
    ' Start the timer running, its timer proc will start up the main app
    tmrSplashScreen.Enabled = True
End Sub

' Purpose: Start up system proper after splash screen display
Sub tmrSplashScreen_Timer ()

    tmrSplashScreen.Enabled = False

    Screen.MousePointer = MP_WAIT

    ' Show the main form
    JunoLibrarian.Show 0

    ' Hide this aboutbox
    Me.Hide

    ' Show OK button since next use of this form will be as an about box
    Me.btnOK.Visible = True
    
    Screen.MousePointer = MP_DEFAULT

End Sub

