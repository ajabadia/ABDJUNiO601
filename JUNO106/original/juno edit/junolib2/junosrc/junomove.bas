' $Header:   D:/misc/midi/vcs/junomove.bas   1.1   07 Nov 1994 00:10:34   DAVEC  $

Option Explicit

' Purpose: Delete selected item(s) from a listbox
Sub ListDelete (lstBox As ListBox)
    Dim iCtr As Integer

    If lstBox.ListCount = 0 Then
        Exit Sub
    End If

    iCtr = 0
    Do While iCtr < lstBox.ListCount
        If lstBox.Selected(iCtr) Then
            lstBox.RemoveItem iCtr
        Else
            iCtr = iCtr + 1
        End If
    Loop
    
End Sub

' Purpose: Moves all selected items and their attached Itemdatas from one list to another
Sub ListMove (lstSource As ListBox, lstDest As ListBox)
    Dim iCtr As Integer, nNewItem As Integer

    If lstSource.MultiSelect Then
        ' Find insert position in destination:
        ' Look for last selected item in destination
        nNewItem = 0
        For iCtr = 0 To lstDest.ListCount - 1
            If lstDest.Selected(iCtr) Then
                lstDest.Selected(iCtr) = False
                nNewItem = iCtr
            End If
        Next
        If nNewItem = 0 Then
            nNewItem = -1
        Else
            nNewItem = nNewItem + 1
        End If

        iCtr = 0
        Do While iCtr < lstSource.ListCount
            If lstSource.Selected(iCtr) Then
                If nNewItem > 0 And nNewItem < lstDest.ListCount Then
                    lstDest.AddItem lstSource.List(iCtr), nNewItem
                    lstDest.ListIndex = nNewItem
                Else
                    lstDest.AddItem lstSource.List(iCtr)
                    lstDest.ListIndex = lstDest.ListCount - 1
                    nNewItem = lstDest.NewIndex
                End If
                lstDest.ItemData(lstDest.NewIndex) = lstSource.ItemData(iCtr)
                ' Mark the added item(s) as selected
                lstDest.Selected(nNewItem) = True
                lstSource.RemoveItem (iCtr) ' And don't inc loop counter
                nNewItem = nNewItem + 1
            Else
                iCtr = iCtr + 1
            End If
        Loop
    Else
        If lstSource.ListIndex >= 0 Then
            lstDest.AddItem lstSource.List(lstSource.ListIndex)
            lstDest.ItemData(lstDest.NewIndex) = lstSource.ItemData(lstSource.ListIndex)
            lstSource.RemoveItem (lstSource.ListIndex)
        End If
    End If
        
End Sub

' Purpose: Move all entries from one multiselect listbox into another one
Sub ListMoveAll (lstSource As ListBox, lstDest As ListBox)
    Dim iCtr As Integer
    lstSource.Visible = False
    ' Select everything in the source list, then call the standard move
    For iCtr = 0 To lstSource.ListCount - 1
        lstSource.Selected(iCtr) = True
    Next
    Call ListMove(lstSource, lstDest)
    lstSource.Visible = True
End Sub

' Purpose: Sort a listbox using a dummy sorted listbox
Sub ListSort (lstToSort As ListBox, lstTemp As ListBox)
    Dim iCtr As Integer

    ' Copy the list
    lstTemp.Clear
    For iCtr = 0 To lstToSort.ListCount - 1
        lstTemp.AddItem lstToSort.List(iCtr)
        lstTemp.ItemData(lstTemp.NewIndex) = lstToSort.ItemData(iCtr)
    Next

    ' Copy the list back
    lstToSort.Clear
    For iCtr = 0 To lstTemp.ListCount - 1
        lstToSort.AddItem lstTemp.List(iCtr)
        lstToSort.ItemData(lstToSort.NewIndex) = lstTemp.ItemData(iCtr)
    Next

    lstTemp.Clear

End Sub

' Purpose: Saves rearranged patches
Sub MoverSave (lstPatch As ListBox)

    Dim nNewsize As Integer, iCtr As Integer
    Static tempPatch() As Patch

    nNewsize = lstPatch.ListCount

    If nNewsize > 0 Then
        ' Copy selected patches in list order to temp array
        ReDim tempPatch(nNewsize) As Patch
        For iCtr = 0 To nNewsize - 1
            tempPatch(iCtr) = gPatches(lstPatch.ItemData(iCtr))
        Next
        ' Clear global patches
        Erase gPatches
        For iCtr = 0 To nNewsize - 1
            gPatches(iCtr) = tempPatch(iCtr)
        Next

        ' Reset description list
        PatchListLoad

        ' Show and send patch
        Call PatchSetCurrentNo(0)

        gbFilemodified = True

    End If
    
End Sub

