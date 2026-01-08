VERSION 2.00
Begin Form JunoPanel 
   BackColor       =   &H00000000&
   BorderStyle     =   1  'Fixed Single
   Caption         =   "Panel"
   ClientHeight    =   2835
   ClientLeft      =   150
   ClientTop       =   3765
   ClientWidth     =   9345
   ForeColor       =   &H00FFFFFF&
   Height          =   3240
   Left            =   90
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   189
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   623
   Top             =   3420
   Width           =   9465
   Begin Frame Frame5 
      BackColor       =   &H00000000&
      Caption         =   "Frame5"
      Height          =   1215
      Left            =   8820
      TabIndex        =   57
      Top             =   840
      Width           =   615
      Begin OptionButton Chorus 
         BackColor       =   &H00000000&
         Caption         =   "Off"
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "MS Sans Serif"
         FontSize        =   8.25
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   255
         Index           =   0
         Left            =   15
         TabIndex        =   28
         Tag             =   "3"
         Top             =   30
         Width           =   735
      End
      Begin OptionButton Chorus 
         BackColor       =   &H00000000&
         Caption         =   "1"
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "MS Sans Serif"
         FontSize        =   8.25
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H000080FF&
         Height          =   255
         Index           =   1
         Left            =   0
         TabIndex        =   29
         Top             =   480
         Width           =   735
      End
      Begin OptionButton Chorus 
         BackColor       =   &H00000000&
         Caption         =   "2"
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "MS Sans Serif"
         FontSize        =   8.25
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H000080FF&
         Height          =   255
         Index           =   2
         Left            =   0
         TabIndex        =   30
         Top             =   960
         Width           =   735
      End
   End
   Begin Frame Frame4 
      BackColor       =   &H00000000&
      Caption         =   "Frame4"
      Height          =   495
      Left            =   6360
      TabIndex        =   56
      Top             =   1620
      Width           =   315
      Begin OptionButton VCAControl 
         BackColor       =   &H00000000&
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   270
         Index           =   0
         Left            =   60
         TabIndex        =   21
         Tag             =   "2"
         Top             =   -15
         Width           =   255
      End
      Begin OptionButton VCAControl 
         BackColor       =   &H00000000&
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   255
         Index           =   1
         Left            =   60
         TabIndex        =   22
         Top             =   240
         Width           =   240
      End
   End
   Begin Frame Frame3 
      BackColor       =   &H00000000&
      Caption         =   "Frame3"
      ForeColor       =   &H00000000&
      Height          =   495
      Left            =   4860
      TabIndex        =   55
      Top             =   1620
      Width           =   315
      Begin OptionButton VCFEnvPolarity 
         BackColor       =   &H00000000&
         ForeColor       =   &H00FFFFFF&
         Height          =   195
         Index           =   0
         Left            =   60
         TabIndex        =   16
         Tag             =   "2"
         Top             =   0
         Width           =   255
      End
      Begin OptionButton VCFEnvPolarity 
         BackColor       =   &H00000000&
         ForeColor       =   &H00FFFFFF&
         Height          =   225
         Index           =   1
         Left            =   60
         TabIndex        =   17
         Top             =   240
         Width           =   255
      End
   End
   Begin Frame Frame2 
      BackColor       =   &H00000000&
      ForeColor       =   &H00000000&
      Height          =   540
      Left            =   1980
      TabIndex        =   54
      Top             =   1560
      Width           =   330
      Begin OptionButton DCOPWMCtrl 
         BackColor       =   &H00000000&
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   195
         Index           =   0
         Left            =   60
         TabIndex        =   7
         Tag             =   "2"
         Top             =   60
         Width           =   255
      End
      Begin OptionButton DCOPWMCtrl 
         BackColor       =   &H00000000&
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   195
         Index           =   1
         Left            =   60
         TabIndex        =   8
         Top             =   300
         Width           =   255
      End
   End
   Begin Frame Frame1 
      BackColor       =   &H00000000&
      Caption         =   "Range"
      ClipControls    =   0   'False
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   1335
      Left            =   780
      TabIndex        =   53
      Top             =   840
      Width           =   555
      Begin OptionButton DCORange 
         Alignment       =   1  'Right Justify
         BackColor       =   &H00000000&
         Caption         =   "16"
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   195
         Index           =   0
         Left            =   0
         TabIndex        =   2
         Tag             =   "3"
         Top             =   360
         Width           =   435
      End
      Begin OptionButton DCORange 
         Alignment       =   1  'Right Justify
         BackColor       =   &H00000000&
         Caption         =   "8"
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   255
         Index           =   1
         Left            =   60
         TabIndex        =   3
         Top             =   600
         Width           =   375
      End
      Begin OptionButton DCORange 
         Alignment       =   1  'Right Justify
         BackColor       =   &H00000000&
         Caption         =   "4"
         FontBold        =   0   'False
         FontItalic      =   0   'False
         FontName        =   "Small Fonts"
         FontSize        =   6.75
         FontStrikethru  =   0   'False
         FontUnderline   =   0   'False
         ForeColor       =   &H00FFFFFF&
         Height          =   195
         Index           =   2
         Left            =   60
         TabIndex        =   4
         Top             =   900
         Width           =   375
      End
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   14
      LargeChange     =   20
      Left            =   8460
      Max             =   127
      TabIndex        =   27
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   13
      LargeChange     =   20
      Left            =   8160
      Max             =   127
      TabIndex        =   26
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   12
      LargeChange     =   20
      Left            =   7860
      Max             =   127
      TabIndex        =   25
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   11
      LargeChange     =   20
      Left            =   7560
      Max             =   127
      TabIndex        =   24
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   10
      LargeChange     =   20
      Left            =   7020
      Max             =   127
      TabIndex        =   23
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   9
      LargeChange     =   20
      Left            =   5820
      Max             =   127
      TabIndex        =   20
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   8
      LargeChange     =   20
      Left            =   5520
      Max             =   127
      TabIndex        =   19
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   7
      LargeChange     =   20
      Left            =   5220
      Max             =   127
      TabIndex        =   18
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   6
      LargeChange     =   20
      Left            =   4560
      Max             =   127
      TabIndex        =   15
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   5
      LargeChange     =   20
      Left            =   4260
      Max             =   127
      TabIndex        =   14
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar HPF 
      Height          =   1935
      Left            =   3660
      Max             =   3
      TabIndex        =   13
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   4
      LargeChange     =   20
      Left            =   3000
      Max             =   127
      TabIndex        =   12
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   15
      LargeChange     =   20
      Left            =   2700
      Max             =   127
      TabIndex        =   11
      Top             =   840
      Width           =   255
   End
   Begin CheckBox DCOTriangleWave 
      BackColor       =   &H00000000&
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   2340
      TabIndex        =   10
      Top             =   1080
      Width           =   255
   End
   Begin CheckBox DCOPulseWave 
      BackColor       =   &H00000000&
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   2040
      TabIndex        =   9
      Top             =   1080
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   3
      LargeChange     =   20
      Left            =   1680
      Max             =   127
      TabIndex        =   6
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   2
      LargeChange     =   20
      Left            =   1380
      Max             =   127
      TabIndex        =   5
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   1
      LargeChange     =   20
      Left            =   360
      Max             =   127
      TabIndex        =   1
      Top             =   840
      Width           =   255
   End
   Begin VScrollBar Slider 
      Height          =   1935
      Index           =   0
      LargeChange     =   20
      Left            =   60
      Max             =   127
      TabIndex        =   0
      Top             =   840
      Width           =   255
   End
   Begin Label Label37 
      BackColor       =   &H00000000&
      Caption         =   "0-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   3480
      TabIndex        =   72
      Top             =   2280
      Width           =   195
   End
   Begin Label Label36 
      BackColor       =   &H00000000&
      Caption         =   "1-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   3480
      TabIndex        =   71
      Top             =   1920
      Width           =   195
   End
   Begin Label Label35 
      BackColor       =   &H00000000&
      Caption         =   "2-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   3480
      TabIndex        =   70
      Top             =   1500
      Width           =   195
   End
   Begin Label Label34 
      BackColor       =   &H00000000&
      Caption         =   "3-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   3480
      TabIndex        =   69
      Top             =   1080
      Width           =   195
   End
   Begin Label Label33 
      BackColor       =   &H00000000&
      Caption         =   "Man"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   1980
      TabIndex        =   65
      Top             =   2100
      Width           =   375
   End
   Begin Label Label32 
      BackColor       =   &H00000000&
      Caption         =   "LFO"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   1980
      TabIndex        =   68
      Top             =   1380
      Width           =   375
   End
   Begin Line Line9 
      BorderColor     =   &H00FFFFFF&
      X1              =   148
      X2              =   148
      Y1              =   68
      Y2              =   60
   End
   Begin Line Line10 
      BorderColor     =   &H00FFFFFF&
      X1              =   168
      X2              =   168
      Y1              =   68
      Y2              =   60
   End
   Begin Line Line8 
      BorderColor     =   &H00FFFFFF&
      X1              =   160
      X2              =   168
      Y1              =   68
      Y2              =   60
   End
   Begin Line Line7 
      BorderColor     =   &H00FFFFFF&
      X1              =   140
      X2              =   148
      Y1              =   68
      Y2              =   68
   End
   Begin Line Line6 
      BorderColor     =   &H00FFFFFF&
      X1              =   140
      X2              =   140
      Y1              =   68
      Y2              =   60
   End
   Begin Line Line5 
      BorderColor     =   &H00FFFFFF&
      X1              =   136
      X2              =   140
      Y1              =   60
      Y2              =   60
   End
   Begin Line Line4 
      BorderColor     =   &H00FFFFFF&
      X1              =   136
      X2              =   136
      Y1              =   68
      Y2              =   60
   End
   Begin Label Label31 
      BackColor       =   &H00000000&
      Caption         =   "-5-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   6840
      TabIndex        =   67
      Top             =   2280
      Width           =   195
   End
   Begin Label Label30 
      BackColor       =   &H00000000&
      Caption         =   "+5-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   6840
      TabIndex        =   66
      Top             =   1080
      Width           =   195
   End
   Begin Label Label29 
      BackColor       =   &H00000000&
      Caption         =   "0-"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   6900
      TabIndex        =   64
      Top             =   1680
      Width           =   135
   End
   Begin Label Label28 
      BackColor       =   &H00000000&
      Caption         =   "Gate"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   6360
      TabIndex        =   63
      Top             =   2100
      Width           =   315
   End
   Begin Label Label27 
      BackColor       =   &H00000000&
      Caption         =   "Env"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   6360
      TabIndex        =   62
      Top             =   1380
      Width           =   315
   End
   Begin Label Label26 
      BackColor       =   &H00000000&
      Caption         =   "-"
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   4980
      TabIndex        =   61
      Top             =   2100
      Width           =   135
   End
   Begin Label Label4 
      BackColor       =   &H00000000&
      Caption         =   "+"
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   4980
      TabIndex        =   60
      Top             =   1380
      Width           =   135
   End
   Begin Line Line3 
      X1              =   356
      X2              =   356
      Y1              =   32
      Y2              =   40
   End
   Begin Line Line2 
      X1              =   336
      X2              =   356
      Y1              =   32
      Y2              =   32
   End
   Begin Line Line1 
      X1              =   336
      X2              =   336
      Y1              =   92
      Y2              =   32
   End
   Begin Label Label25 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "CH"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   8820
      TabIndex        =   47
      Top             =   0
      Width           =   540
   End
   Begin Label Label24 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "ENV"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   7440
      TabIndex        =   52
      Top             =   0
      Width           =   1335
   End
   Begin Label Label23 
      BackColor       =   &H00000000&
      Caption         =   "R"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   8520
      TabIndex        =   51
      Top             =   600
      Width           =   135
   End
   Begin Label Label22 
      BackColor       =   &H00000000&
      Caption         =   "S"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   8220
      TabIndex        =   50
      Top             =   600
      Width           =   135
   End
   Begin Label Label21 
      BackColor       =   &H00000000&
      Caption         =   "D"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   7920
      TabIndex        =   49
      Top             =   600
      Width           =   135
   End
   Begin Label Label20 
      BackColor       =   &H00000000&
      Caption         =   "A"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   7620
      TabIndex        =   48
      Top             =   600
      Width           =   135
   End
   Begin Label Label19 
      BackColor       =   &H00000000&
      Caption         =   "Level"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   6960
      TabIndex        =   46
      Top             =   600
      Width           =   375
   End
   Begin Label Label18 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "VCA"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   6240
      TabIndex        =   45
      Top             =   0
      Width           =   1155
   End
   Begin Label Label17 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "VCF"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   4200
      TabIndex        =   44
      Top             =   0
      Width           =   1995
   End
   Begin Label Label16 
      BackColor       =   &H00000000&
      Caption         =   "Kybd"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   5820
      TabIndex        =   43
      Top             =   600
      Width           =   495
   End
   Begin Label Label15 
      BackColor       =   &H00000000&
      Caption         =   "LFO"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   5460
      TabIndex        =   42
      Top             =   600
      Width           =   375
   End
   Begin Label Label14 
      BackColor       =   &H00000000&
      Caption         =   "Env"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   5160
      TabIndex        =   41
      Top             =   600
      Width           =   375
   End
   Begin Label Label13 
      BackColor       =   &H00000000&
      Caption         =   "Res"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   4560
      TabIndex        =   40
      Top             =   600
      Width           =   375
   End
   Begin Label Label12 
      BackColor       =   &H00000000&
      Caption         =   "Freq"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   4200
      TabIndex        =   39
      Top             =   600
      Width           =   375
   End
   Begin Label Label11 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "HPF"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   3480
      TabIndex        =   38
      Top             =   0
      Width           =   675
   End
   Begin Label Label10 
      BackColor       =   &H00000000&
      Caption         =   "HPF"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   3660
      TabIndex        =   37
      Top             =   600
      Width           =   375
   End
   Begin Label Label8 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "DCO"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   780
      TabIndex        =   36
      Top             =   0
      Width           =   2655
   End
   Begin Label Label9 
      BackColor       =   &H00000000&
      Caption         =   "Noise"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   3000
      TabIndex        =   35
      Top             =   600
      Width           =   495
   End
   Begin Label Label7 
      BackColor       =   &H00000000&
      Caption         =   "Sub"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   2640
      TabIndex        =   34
      Top             =   600
      Width           =   375
   End
   Begin Label Label6 
      BackColor       =   &H00000000&
      Caption         =   "PWM"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   1680
      TabIndex        =   33
      Top             =   600
      Width           =   495
   End
   Begin Label Label5 
      BackColor       =   &H00000000&
      Caption         =   "LFO"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   1320
      TabIndex        =   32
      Top             =   600
      Width           =   375
   End
   Begin Label Label3 
      Alignment       =   2  'Center
      BackColor       =   &H000000FF&
      Caption         =   "LFO"
      FontBold        =   -1  'True
      FontItalic      =   0   'False
      FontName        =   "MS Sans Serif"
      FontSize        =   13.5
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   375
      Left            =   0
      TabIndex        =   31
      Top             =   0
      Width           =   750
   End
   Begin Label Label2 
      BackColor       =   &H00000000&
      Caption         =   "Delay"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   360
      TabIndex        =   59
      Top             =   600
      Width           =   495
   End
   Begin Label Label1 
      BackColor       =   &H00000000&
      Caption         =   "Rate"
      FontBold        =   0   'False
      FontItalic      =   0   'False
      FontName        =   "Small Fonts"
      FontSize        =   6.75
      FontStrikethru  =   0   'False
      FontUnderline   =   0   'False
      ForeColor       =   &H00FFFFFF&
      Height          =   255
      Left            =   0
      TabIndex        =   58
      Top             =   600
      Width           =   375
   End
End
Option Explicit

Sub Chorus_Click (Index As Integer)
    Dim nPatchValue As Integer

    ' Chorus is bits 5 and 6 of byte 1, where 0=Level 2, 1 = Off, 2 = Level 1
    Select Case Index
    Case 0  ' Off
        nPatchValue = 1
    Case 1  ' Level 1
        nPatchValue = 2
    Case 2  ' Level 2
        nPatchValue = 0
    End Select
    
    Call ChangeControlBit(CONTROLBYTE_1, &H9F, nPatchValue * &H20)

End Sub

Sub DCOPulseWave_Click ()

    ' Bit 4 of CONTROLBYTE_1
    Call ChangeControlBit(CONTROLBYTE_1, &HF7, Abs(DCOPulseWave.Value) * 8)


End Sub

Sub DCOPWMCtrl_Click (Index As Integer)
    ' Bit 1 of control byte 2 is True if LFO control (index=1)
    Call ChangeControlBit(CONTROLBYTE_2, &HFE, Abs(DCOPWMCtrl(1).Value))
End Sub

Sub DCORange_Click (Index As Integer)

    
    Dim nPatchValue As Integer

    ' This switch is bottom 3 bits of control byte 1
    If Index = 0 Then
        nPatchValue = 1
    Else
        nPatchValue = Index * 2
    End If

    Call ChangeControlBit(CONTROLBYTE_1, &HF8, nPatchValue)

End Sub

Sub DCOTriangleWave_Click ()
    
    ' Bit 5 of CONTROLBYTE_1
    Call ChangeControlBit(CONTROLBYTE_1, &HEF, Abs(DCOTriangleWave.Value) * &H10)

End Sub

Sub HPF_Change ()
    ' Bits 3 and 4 of byte 2
    Call ChangeControlBit(CONTROLBYTE_2, &HE7, HPF.Value * 8)
End Sub

Sub Slider_Change (Index As Integer)
    If gbPanelLive Then
        Call SliderChangeToControlMsg(Index, Slider(Index).Value)
    End If
End Sub

Sub VCAControl_Click (Index As Integer)
    ' Bit 3 of byte 2 is set when VCA Control is Gate
    Call ChangeControlBit(CONTROLBYTE_2, &HFB, Abs(VCAControl(1).Value) * 4)
End Sub

Sub VCFEnvPolarity_Click (Index As Integer)
    ' Bit 1 of control byte 2 is True when polarity set to Negative
    Call ChangeControlBit(CONTROLBYTE_2, &HFD, Abs(VCFENVPolarity(1).Value) * 2)
End Sub

