#Persistent
#SingleInstance Force

IsBusy := false

; ===== Choose your hotkey here =====
^!F12::
    IsBusy := !IsBusy
    if (IsBusy)
        SetCursorAppStarting()
    else
        RestoreCursors()
    ToolTip, % IsBusy ? "Working In Background Cursor ON" : "Working In Background Cursor OFF"
    SetTimer, ClearToolTip, -1500
return

ClearToolTip:
    ToolTip
return

; --- Replace all cursor types with the Working In Background cursor ---
SetCursorAppStarting() {
    ; All standard OCR_ cursor IDs
    CursorIDs := [32512   ; NORMAL (Arrow)
                , 32513   ; IBEAM  (Text select)
                , 32514   ; WAIT   (Busy)
                , 32515   ; CROSS
                , 32516   ; UP
                , 32642   ; SIZENWSE
                , 32643   ; SIZENESW
                , 32644   ; SIZEWE
                , 32645   ; SIZENS
                , 32646   ; SIZEALL
                , 32648   ; NO
                , 32649   ; HAND
                , 32650]  ; APPSTARTING

    ; Read the user's AppStarting cursor path from the registry
    RegRead, CursorPath, HKEY_CURRENT_USER, Control Panel\Cursors, AppStarting    ; <-- CHANGE THIS

    ; Fallback to default Windows animated cursor if registry is empty
    if (CursorPath = "")
        CursorPath := "%SystemRoot%\Cursors\aero_working.ani"    ; <-- CHANGE THIS

    ; Expand environment variables like %SystemRoot%
    VarSetCapacity(Expanded, 520)
    DllCall("ExpandEnvironmentStrings", "Str", CursorPath, "Str", Expanded, "UInt", 260)
    CursorPath := Expanded

    for i, id in CursorIDs
    {
        ; LoadCursorFromFile preserves animation in .ani files
        ; Must load a fresh handle each time — SetSystemCursor destroys the handle
        hCursor := DllCall("LoadCursorFromFile", "Str", CursorPath, "Ptr")
        if (hCursor)
            DllCall("SetSystemCursor", "Ptr", hCursor, "UInt", id)
    }
}

; --- Reload cursors from the current scheme (no scheme change) ---
RestoreCursors() {
    ; SPI_SETCURSORS (0x57) reloads whatever scheme is already active
    DllCall("SystemParametersInfo", "UInt", 0x57, "UInt", 0, "Ptr", 0, "UInt", 0)
}

; --- Safety: restore cursors if the script exits while busy ---
OnExit("ExitCleanup")
ExitCleanup(ExitReason, ExitCode) {
    global IsBusy
    if (IsBusy)
        RestoreCursors()
}

/*
+------------------------+------------------------+-------------------------------+-----------+
| Cursor Name            | Registry Value (LINE 1)| Default Fallback File (LINE 2)| Animated? |
+------------------------+------------------------+-------------------------------+-----------+
| Normal Select          | Arrow                  | aero_arrow.cur                | No        |
| Help Select            | Help                   | aero_helpsel.cur              | No        |
| Working In Background  | AppStarting            | aero_working.ani              | Yes       |
| Busy                   | Wait                   | aero_busy.ani                 | Yes       |
| Precision Select       | Crosshair              | cross_r.cur                   | No        |
| Text Select            | IBeam                  | beam_r.cur                    | No        |
| Handwriting            | NWPen                  | aero_pen.cur                  | No        |
| Unavailable            | No                     | aero_unavail.cur              | No        |
| Vertical Resize        | SizeNS                 | aero_ns.cur                   | No        |
| Horizontal Resize      | SizeWE                 | aero_ew.cur                   | No        |
| Diagonal Resize 1      | SizeNWSE               | aero_nwse.cur                 | No        |
| Diagonal Resize 2      | SizeNESW               | aero_nesw.cur                 | No        |
| Move                   | SizeAll                | aero_move.cur                 | No        |
| Alternate Select       | UpArrow                | aero_up.cur                   | No        |
| Link Select            | Hand                   | aero_link.cur                 | No        |
+------------------------+------------------------+-------------------------------+-----------+
*/
