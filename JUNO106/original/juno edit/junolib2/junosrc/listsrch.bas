' $Header:   D:/misc/midi/vcs/listsrch.bas   1.1   07 Nov 1994 00:11:40   DAVEC  $

' Search functions for listboxes
Option Explicit

Const WM_USER = 1024
Const LB_FINDSTRING = WM_USER + 16
Const LB_FINDSTRINGEXACT = WM_USER + 35
Const LB_SETTOPINDEX = WM_USER + 24

Declare Function SendMessage Lib "User" (ByVal hWnd As Integer, ByVal wMsg As Integer, ByVal wParam As Integer, lParam As Any) As Long
Declare Function SendMessageByNum Lib "User" Alias "SendMessage" (ByVal hWnd As Integer, ByVal wMsg As Integer, ByVal wParam As Integer, ByVal lParam As Long) As Long
Declare Function SendMessageByString Lib "User" Alias "SendMessage" (ByVal hWnd As Integer, ByVal wMsg As Integer, ByVal wParam As Integer, ByVal lParam As String) As Long

' ---------------------------------------------------------
' Purpose: Search list box for string
' Returns: String index (long) if found, < 0 if not found
' Notes:
' Search is not case-sensitive.
' This does an inexact search and will find strings that are
' longer than the search string but match up to the end of the search
' string.
' Searches whole list box from the beginning. Could use a different wparam
' to search from current position, see API ref.
Function ListBoxSearch (ctlListBox As ListBox, sSearchString As String) As Long

ListBoxSearch = SendMessageByString&(ctlListBox.hWnd, LB_FINDSTRING, 0, sSearchString)

End Function

' ---------------------------------------------------------
' Purpose: Search list box for exactly matching string
' Returns: String index if found, < 0 if not found
' Notes: Search is not case-sensitive and searches list box from beginning
Function ListBoxSearchExact (ctlListBox As ListBox, sSearchString As String) As Long

ListBoxSearchExact = SendMessageByString&(ctlListBox.hWnd, LB_FINDSTRINGEXACT, -1, sSearchString)

End Function

' ---------------------------------------------------------
' Purpose: Search list box for exactly matching string, starting at given item
' Returns: String index if found, < 0 if not found
' Notes: Search is not case-sensitive
Function ListBoxSearchExactFrom (ctlListBox As ListBox, sSearchString As String, nStart As Integer) As Long

ListBoxSearchExactFrom = SendMessageByString&(ctlListBox.hWnd, LB_FINDSTRINGEXACT, nStart, sSearchString)

End Function

