global ClockVisible := false
; Position/handle globals
global gClockX, gClockY, gClockW, gClockH, hClock, hDisk
global gOffsetX := 80, gOffsetY := 20, gGap := 8  ; offsets from cursor and gap between boxes

; Hotkey to toggle clock + disk snapshot
^!c:: ; Ctrl+Alt+C
CoordMode, Mouse, Screen
ClockVisible := !ClockVisible
if (ClockVisible) {
    ; Initial anchor based on current mouse position
    MouseGetPos, mx, my
    gClockX := mx + gOffsetX
    gClockY := my + gOffsetY

    ; Build GUIs
    Gosub, BuildClockGui
    Gosub, ShowDiskInfo

    ; Start timers
    SetTimer, ShowClock, 1000    ; updates time/date text
    SetTimer, FollowMouse, 33    ; moves both GUIs with the cursor (~30 FPS)
    Gosub, FollowMouse           ; immediate first move
} else {
    SetTimer, ShowClock, Off
    SetTimer, FollowMouse, Off
    Gui, Destroy        ; Clock GUI
    Gui, 2:Destroy      ; Disk GUI
}
return

; Create the clock GUI with initial values
BuildClockGui:
    FormatTime, nowTime,, hh:mm:ss tt   ; 12-hour format with AM/PM
    FormatTime, nowDate,, dddd, MMMM d yyyy

    Gui, Destroy
    Gui, +AlwaysOnTop -Caption +ToolWindow +LastFound +E0x20 +HwndhClock
    Gui, Color, 000000
    Gui, Font, cFFFFFF s14 Bold, Segoe UI
    Gui, Add, Text, vTimeTxt, %nowTime%
    Gui, Font, cAAAAAA s10, Segoe UI
    Gui, Add, Text, vDateTxt, %nowDate%
    Gui, Show, x%gClockX% y%gClockY% NoActivate

    ; Measure height so disk box can sit right below it
    WinGetPos,,, gClockW, gClockH, ahk_id %hClock%
return

; Timer: update the clock text only (no recreating GUI)
ShowClock:
    FormatTime, nowTime,, hh:mm:ss tt
    FormatTime, nowDate,, dddd, MMMM d yyyy
    GuiControl,, TimeTxt, %nowTime%
    GuiControl,, DateTxt, %nowDate%
return

; One-time disk snapshot GUI (per activation)
; One-time disk snapshot GUI (auto-detect drives each activation)
ShowDiskInfo:
    ; Build list of fixed drives and their free space
    DriveGet, list, List
    text := ""
    Loop, Parse, list
    {
        letter := A_LoopField
        DriveGet, type, Type, %letter%:\
        if (type != "Fixed")  ; change to (type != "Fixed" && type != "Removable") to include USBs
            continue
        path := letter ":\"
        free := GetFree(path)
        if (free = "N/A")
            continue
        text .= letter ":\ " free "`n"
    }
    if (text = "")
        text := "No fixed drives found"

    dx := gClockX
    dy := gClockY + gClockH + gGap   ; place below clock

    Gui, 2:Destroy
    Gui, 2:+AlwaysOnTop -Caption +ToolWindow +LastFound +E0x20 +HwndhDisk
    Gui, 2:Color, 000000
    Gui, 2:Font, cAAAAAA s10 Bold, Segoe UI
    Gui, 2:Add, Text,, %text%
    Gui, 2:Show, x%dx% y%dy% NoActivate
return

; Follow the mouse (move both GUIs only; no content refresh for disk)
FollowMouse:
    CoordMode, Mouse, Screen
    MouseGetPos, mx, my
    newX := mx + gOffsetX
    newY := my + gOffsetY

    ; Move clock
    gClockX := newX, gClockY := newY
    WinMove, ahk_id %hClock%,, %newX%, %newY%

    ; Move disk below clock
    if (hDisk) {
        diskX := newX
        diskY := newY + gClockH + gGap
        WinMove, ahk_id %hDisk%,, %diskX%, %diskY%
    }
return

; Helper: returns "123.4 GB free" or "N/A"
GetFree(path) {
    if (SubStr(path, 0) != "\")
        path .= "\"
    if !FileExist(path)
        return "N/A"
    DriveSpaceFree, freeMB, %path%
    if (ErrorLevel)
        return "N/A"
    freeGB := freeMB / 1024.0
    return Round(freeGB, 1) . " GB free"
}