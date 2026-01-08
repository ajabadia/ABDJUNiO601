' $Header:   D:/misc/midi/vcs/alloc.bas   1.2   07 Nov 1994 00:12:28   DAVEC  $

' Public domain by David Churcher. No rights reserved. Use at your own risk.

Declare Function GlobalAlloc Lib "kernel.dll" (ByVal fuAlloc As Integer, ByVal cbAlloc As Long) As Integer
Declare Function GlobalLock Lib "kernel.dll" (ByVal hGlobal As Integer) As Long
Declare Function GlobalUnlock Lib "kernel.dll" (ByVal hGlobal As Integer) As Integer
Declare Function GlobalFree Lib "kernel.dll" (ByVal hGlobal As Integer) As Integer
Declare Function GlobalPageLock Lib "kernel.dll" (ByVal hGlobal As Integer) As Integer
Declare Function GlobalPageUnlock Lib "kernel.dll" (ByVal hGlobal As Integer) As Integer
Declare Function GlobalRealloc Lib "Kernel.dll" (ByVal hGlobal As Integer, ByVal cbNewSize As Long, ByVal fuAlloc As Integer) As Integer

Declare Sub hmemcpy Lib "kernel.dll" (ByVal lpdest As Any, ByVal lpsrc As Any, ByVal length As Long)

Global Const GMEM_FIXED = &H0
Global Const GMEM_MOVEABLE = &H2
Global Const GMEM_NOCOMPACT = &H10
Global Const GMEM_NODISCARD = &H20
Global Const GMEM_ZEROINIT = &H40
Global Const GMEM_MODIFY = &H80
Global Const GMEM_DISCARDABLE = &H100
Global Const GMEM_NOT_BANKED = &H1000
Global Const GMEM_SHARE = &H2000
Global Const GMEM_DDESHARE = &H2000
Global Const GMEM_NOTIFY = &H4000
Global Const GMEM_LOWER = GMEM_NOT_BANKED
Global Const GHND = (GMEM_MOVEABLE + GMEM_ZEROINIT)
Global Const GPTR = (GMEM_FIXED + GMEM_ZEROINIT)

