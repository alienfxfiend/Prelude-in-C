#MaxThreadsPerHotkey 2
global ClockVisible := false
global gClockX, gClockY, gClockW, gClockH, hClock, hDisk, hRam
global gDiskH
global gOffsetX := 109, gOffsetY := 20, gGap := 8
IsBusy := false

; Hotkey to toggle clock + disk snapshot + RAM
^!c:: ; Ctrl+Alt+C
Critical                          ; grab the thread immediately, don't queue behind timers
if (ClockVisible) {
    ; Stop all timers FIRST before touching GUIs
    SetTimer, ShowClock,   Off
    SetTimer, UpdateRam,   Off
    SetTimer, FollowMouse, Off
    ClockVisible := false
    Critical, Off
    Gui,    Destroy
    Gui, 2: Destroy
    Gui, 3: Destroy
} else {
    ClockVisible := true
    Critical, Off                 ; release before the slower setup work
    CoordMode, Mouse, Screen
    MouseGetPos, mx, my
    gClockX := mx + gOffsetX
    gClockY := my + gOffsetY

    Gosub, BuildClockGui
    Gosub, ShowDiskInfo
    Gosub, BuildRamGui

    SetTimer, ShowClock,   1000
    SetTimer, UpdateRam,   1000
    SetTimer, FollowMouse, 33
    Gosub, FollowMouse
}
return

; -------------------------------------------------------
; Create the clock GUI
; -------------------------------------------------------
BuildClockGui:
    FormatTime, nowTime,, hh:mm:ss tt
    FormatTime, nowDate,, dddd, MMMM d yyyy

    Gui, Destroy
    Gui, +AlwaysOnTop -Caption +ToolWindow +LastFound +E0x20 +HwndhClock
    Gui, Color, 000000
    Gui, Font, c0380DF s14 Bold, Segoe UI ;cFFFFFF
    Gui, Add, Text, vTimeTxt, %nowTime%
    Gui, Font, c55A4E8 s10, Segoe UI ;cAAAAAA
    Gui, Add, Text, vDateTxt, %nowDate%
    Gui, Show, x%gClockX% y%gClockY% NoActivate

    WinGetPos,,, gClockW, gClockH, ahk_id %hClock%
return

; -------------------------------------------------------
; Timer: update clock text
; -------------------------------------------------------
ShowClock:
    FormatTime, nowTime,, hh:mm:ss tt
    FormatTime, nowDate,, dddd, MMMM d yyyy
    GuiControl,, TimeTxt, %nowTime%
    GuiControl,, DateTxt, %nowDate%
return

; -------------------------------------------------------
; One-time disk snapshot GUI
; -------------------------------------------------------
ShowDiskInfo:
    DriveGet, list, List
    text := ""
    Loop, Parse, list
    {
        letter := A_LoopField
        DriveGet, type, Type, %letter%:\
        if (type != "Fixed")
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
    dy := gClockY + gClockH + gGap

    Gui, 2:Destroy
    Gui, 2:+AlwaysOnTop -Caption +ToolWindow +LastFound +E0x20 +HwndhDisk
    Gui, 2:Color, 000000
    Gui, 2:Font, c55A4E8 s10 Bold, Segoe UI ;cAAAAAA
    Gui, 2:Add, Text,, %text%
    Gui, 2:Show, x%dx% y%dy% NoActivate

    ; Measure disk GUI height so RAM box sits below it
    WinGetPos,,, gDiskW, gDiskH, ahk_id %hDisk%
return

; -------------------------------------------------------
; Create the RAM GUI
; -------------------------------------------------------
BuildRamGui:
    ramText := GetRamUsed()

    rx := gClockX
    ry := gClockY + gClockH + gGap + gDiskH + gGap

    Gui, 3:Destroy
    Gui, 3:+AlwaysOnTop -Caption +ToolWindow +LastFound +E0x20 +HwndhRam
    Gui, 3:Color, 000000
    Gui, 3:Font, c55A4E8 s10 Bold, Segoe UI ;cAAAAAA
    Gui, 3:Add, Text, vRamTxt, %ramText%
    Gui, 3:Show, x%rx% y%ry% NoActivate
return

; -------------------------------------------------------
; Timer: update RAM text
; -------------------------------------------------------
UpdateRam:
    ramText := GetRamUsed()
    GuiControl, 3:, RamTxt, %ramText%
return

; -------------------------------------------------------
; Follow the mouse (move all three GUIs)
; -------------------------------------------------------
FollowMouse:
    CoordMode, Mouse, Screen
    MouseGetPos, mx, my
    newX := mx + gOffsetX
    newY := my + gOffsetY

    gClockX := newX
    gClockY := newY
    WinMove, ahk_id %hClock%,, %newX%, %newY%

    if (hDisk) {
        diskY := newY + gClockH + gGap
        WinMove, ahk_id %hDisk%,, %newX%, %diskY%
    }

    if (hRam) {
        ramY := newY + gClockH + gGap + gDiskH + gGap
        WinMove, ahk_id %hRam%,, %newX%, %ramY%
    }
return

; -------------------------------------------------------
; Helper: returns "123.4 GB free" or "N/A"
; -------------------------------------------------------
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

; -------------------------------------------------------
; Helper: returns "Memory Used: xx.x %" via DllCall (non-blocking)
; -------------------------------------------------------
GetRamUsed() {
    VarSetCapacity(ms, 64, 0)
    NumPut(64, ms, 0, "UInt")                 ; dwLength = 64
    DllCall("GlobalMemoryStatusEx", "Ptr", &ms)
    total := NumGet(ms,  8, "UInt64")         ; ullTotalPhys
    avail := NumGet(ms, 16, "UInt64")         ; ullAvailPhys
    if (total = 0)
        return "Memory Used: N/A"
    usedPct := ((total - avail) / total) * 100
    return "Memory Used: " . Round(usedPct, 1) . " %"
}
