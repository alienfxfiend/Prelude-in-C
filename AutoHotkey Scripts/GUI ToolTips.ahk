ShowStyledTooltip(msgText, xExpr, yExpr, duration := 15000, wrapWidth := "", wrapHeight := "") {
    global isVisible

    if (duration = 0 && isVisible) {
        Gosub, RemoveGUITip
        return
    }

    Gui, Destroy

    x := Round(xExpr + 0)
    y := Round(yExpr + 0)

    Gui, +AlwaysOnTop -Caption +ToolWindow +E0x80000 +LastFound +Resize
    Gui, Margin, 20, 15
    Gui, Color, 151a28
    Gui, Font, s9, Segoe UI

    ; Build text options with optional width/height and scrollbars
    textOpts := "c800040 BackgroundTrans gStartDrag"
    if (wrapWidth != "")
        textOpts .= " w" . wrapWidth
    if (wrapHeight != "")
        textOpts .= " h" . wrapHeight . " +0x200000"  ; WS_VSCROLL

    Gui, Add, Edit, %textOpts% ReadOnly -E0x200 WantTab Multi, %msgText%

    WinSet, Transparent, 0
    if (wrapWidth != "" || wrapHeight != "")
        Gui, Show, x%x% y%y% NoActivate
    else
        Gui, Show, AutoSize x%x% y%y% NoActivate

    WinGetPos,,, w, h, A
    WinSet, Region, 0-0 W%w% H%h% R20-20

    Loop 20 {
        WinSet, Transparent, % A_Index * 12
        Sleep, 20
    }

    isVisible := true

    if (duration > 0) {
        SetTimer, RemoveGUITip, -%duration%
    }
    return
}

RemoveGUITip:
    Loop 20 {
        WinSet, Transparent, % 255 - (A_Index * 12)
        Sleep, 20
    }
    Gui, Destroy
    isVisible := false
return

StartDrag:
    PostMessage, 0xA1, 2,,, A  ; WM_NCLBUTTONDOWN, HTCAPTION
return

/* Example Usage::
^+F8::ShowStyledTooltip(
    "This is a long message that will wrap properly by word and scroll if needed.",
    A_ScreenWidth/2 - 200,
    A_ScreenHeight/2 - 100,
    0, 300, 100)  ; width=300, height=100
*/
