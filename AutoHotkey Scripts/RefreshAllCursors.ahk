; AutoHotkey v1.1
; Press Ctrl+Alt+C to refresh ALL standard system cursors
; (Arrow, Busy, Wait, IBeam, Resize, Hand, etc.)

^!c::
    ; List of standard cursor registry values to refresh
    cursors := "Arrow,Help,AppStarting,Wait,Crosshair,IBeam,NWPen,No"
              . ",SizeNS,SizeWE,SizeNWSE,SizeNESW,SizeAll,UpArrow,Hand"

    ; Choose a temporary cursor (built-in Windows arrow)
    tempCursor := A_WinDir . "\Cursors\aero_arrow.cur"
    if !FileExist(tempCursor)
        tempCursor := A_WinDir . "\Cursors\arrow_m.cur"  ; fallback

    orig := {}  ; object to store original values

    ; 1) Save originals and set all to temp cursor
    Loop, Parse, cursors, `,
    {
        key := A_LoopField

        RegRead, curValue, HKEY_CURRENT_USER, Control Panel\Cursors, %key%
        if (ErrorLevel)
            curValue := ""  ; in case value doesn't exist

        orig[key] := curValue

        ; Set this role to the temporary cursor
        RegWrite, REG_SZ, HKEY_CURRENT_USER, Control Panel\Cursors, %key%, %tempCursor%
    }

    ; Apply temporary scheme
    SPI_SETCURSORS  := 0x0057
    SPIF_SENDCHANGE := 0x0002
    DllCall("SystemParametersInfo", "UInt", SPI_SETCURSORS
        , "UInt", 0, "Ptr", 0, "UInt", SPIF_SENDCHANGE)

    Sleep, 200  ; small delay to let Windows update the cursors

    ; 2) Restore original cursor paths
    for key, val in orig
    {
        RegWrite, REG_SZ, HKEY_CURRENT_USER, Control Panel\Cursors, %key%, %val%
    }

    ; Apply original scheme again
    DllCall("SystemParametersInfo", "UInt", SPI_SETCURSORS
        , "UInt", 0, "Ptr", 0, "UInt", SPIF_SENDCHANGE)
return
