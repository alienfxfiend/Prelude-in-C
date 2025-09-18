; ===================================================================
;  THEME-SELECTOR  —  totally working version
; ===================================================================

#SingleInstance  Force
#NoEnv
SetBatchLines   -1
DetectHiddenWindows  On
Process  Priority,, High
SetWorkingDir %A_ScriptDir%

; -------------------------------------------------------------------
;  GLOBAL DATA
; -------------------------------------------------------------------
global filePath := "D:\Download\DL Dir\pendrive\web content\cyberpunk ebooks\backups\Sumatra Proto\SumatraPDF-settings.txt"

themes := {}
themes["IndigoOnPink"] := { "TextColor": "#ee3f86", "BackgroundColor": "#1B0032" }
themes["Sepia"] := { "TextColor": "#5F4B32", "BackgroundColor": "#cda882" }
themes["Orangey Red"] := { "TextColor": "#8a0000", "BackgroundColor": "#e7a212" }
themes["Dracula"] := { "TextColor": "#dcdc30", "BackgroundColor": "#21222c" }
themes["GreenOnWhite"] := { "TextColor": "#efefec", "BackgroundColor": "#223a2c" }
themes["IceReal"] := { "TextColor": "#67707b", "BackgroundColor": "#272727" }
themes["BlueThunder"] := { "TextColor": "#00fdfe", "BackgroundColor": "#003e53" }
themes["Hot Fudge Sundae"] := { "TextColor": "#f39900", "BackgroundColor": "#2B0F01" }
themes["Solarized"] := { "TextColor": "#839496", "BackgroundColor": "#002b36" }
themes["Green Theme Orig"] := { "TextColor": "#A3DE34", "BackgroundColor": "#000000" }
themes["Navajo"] := { "TextColor": "#000000", "BackgroundColor": "#BA9C80" }
themes["Vim Dark"] := { "TextColor": "#ffffff", "BackgroundColor": "#000040" }
themes["Vibrant Ink"] := { "TextColor": "#FFFF80", "BackgroundColor": "#FF8000" }
themes["Khaki"] := { "TextColor": "#5f5f00", "BackgroundColor": "#d7d7af" }
themes["MossyLawn"] := { "TextColor": "#f2c476", "BackgroundColor": "#58693d" }
themes["DansLeRushDark"] := { "TextColor": "#c7c7c7", "BackgroundColor": "#2E2E2E" }
themes["Midnight-Red"] := { "TextColor": "#ffd300", "BackgroundColor": "#240500" }
themes["Slate Blue"] := { "TextColor": "#bac9d0", "BackgroundColor": "#263238" }
themes["Gray"] := { "TextColor": "#7f7e7a", "BackgroundColor": "#191919" }
themes["Green WindowBlindHUDDesktop"] := { "TextColor": "#ffff46", "BackgroundColor": "#7aa618" }
themes["Ice (best)"] := { "TextColor": "#00fdfe", "BackgroundColor": "#003e53" }
themes["IceNonNightLight"] := { "TextColor": "#41FDFE", "BackgroundColor": "#073642" }
themes["Indigo CoralOrange"] := { "TextColor": "#FF7F50", "BackgroundColor": "#3B0B59" }
themes["Liz-MagicPotion"] := { "TextColor": "#e1dafb", "BackgroundColor": "#2c013c" }
themes["Liz-CarbonX"] := { "TextColor": "#ffcd86", "BackgroundColor": "#3a1c13" }
themes["Liz-SunsetRails"] := { "TextColor": "#ff9900", "BackgroundColor": "#240600" }
themes["Liz-BlueLightFilter"] := { "TextColor": "#bc0061", "BackgroundColor": "#fff3a9" }
themes["Maroon-Yellow-Alt2"] := { "TextColor": "#cfba28", "BackgroundColor": "#2b0f01" }
themes["TedNPad-Coral-Pink"] := { "TextColor": "#ff80c0", "BackgroundColor": "#a04644" }
themes["GreenRed-HighContrast"] := { "TextColor": "#C51B1B", "BackgroundColor": "#79FB0E" }
themes["Gold"] := { "TextColor": "#400000", "BackgroundColor": "#B99350" }
themes["3.5Preset DullBlu"] := { "TextColor": "#b9c8cf", "BackgroundColor": "#2c3b42" }
themes["NeonGreenLuminesce"] := { "TextColor": "#262728", "BackgroundColor": "#2CFF05" }
themes["Green-Adhoc-Dull (Zinnwaldite Brown)"] := { "TextColor": "#8bcb4f", "BackgroundColor": "#282c00" }
themes["Purple-Lavendar"] := { "TextColor": "#4b2547", "BackgroundColor": "#aca1b9" }
themes["(DeepAI)[Dark] Terminal Green on Black"] := { "TextColor": "#00FF00", "BackgroundColor": "#000000" }
themes["(DeepAI)[Dark] Soft Blue on Dark Gray"] := { "TextColor": "#00CFFF", "BackgroundColor": "#1C1C1C" }
themes["(DeepAI)[Dark] Light Yellow on Dark Slate"] := { "TextColor": "#FFFF99", "BackgroundColor": "#2A2A2A" }
themes["(DeepAI)[Dark] Cool White on Charcoal"] := { "TextColor": "#E0E0E0", "BackgroundColor": "#3E3E3E" }
themes["(DeepAI)[Dark] Faded Purple on Black"] := { "TextColor": "#B19CD9", "BackgroundColor": "#000000" }
themes["(DeepAI)[Light] Soft Sepia on Cream"] := { "TextColor": "#5B3A29", "BackgroundColor": "#F5EFE6" }
themes["(DeepAI)[Light] Slate Gray on Off-White"] := { "TextColor": "#696969", "BackgroundColor": "#F8F8F8" }
themes["(DeepAI)[Light] Muted Brown on Light Beige"] := { "TextColor": "#4B3C30", "BackgroundColor": "#F5F0E1" }
themes["(DeepAI)[Light] Soft Dark Blue on Pale Yellow"] := { "TextColor": "#003366", "BackgroundColor": "#FFFCE6" }
themes["(DeepAI)[Light] Rosewood on Pale Grey"] := { "TextColor": "#A65E2E", "BackgroundColor": "#EAEAEA" }
themes["(DeepAI)[Extra] White on Black"] := { "TextColor": "#FFFFFF", "BackgroundColor": "#000000" }
themes["(DeepAI)[Extra] Dark Cyan on Dark Purple"] := { "TextColor": "#008B8B", "BackgroundColor": "#cda882" }
themes["(DeepAI)[Extra] Ash Gray on Dark Teal"] := { "TextColor": "#5F4B32", "BackgroundColor": "#004D4D" }
themes["(GPT4.1) Classic Terminal Green on Black"] := { "TextColor": "#39FF14", "BackgroundColor": "#101010" }
themes["(GPT4.1) Modern Dark Mode"] := { "TextColor": "#E0E0E0", "BackgroundColor": "#181A1B" }
themes["(GPT4.1) Solarized Dark"] := { "TextColor": "#93A1A1", "BackgroundColor": "#002B36" }
themes["(GPT4.1) Sepia Mode (Comfortable Light)"] := { "TextColor": "#5B4636", "BackgroundColor": "#F5ECD9" }
themes["(GPT4.1) High Contrast White on Black"] := { "TextColor": "#FFFFFF", "BackgroundColor": "#000000" }
themes["(GPT4.1) Soothing Dark Blue"] := { "TextColor": "#E6EDF3", "BackgroundColor": "#222C36" }
themes["(GPT4.1) Classic Paper Look (Light Mode)"] := { "TextColor": "#333333", "BackgroundColor": "#FFF8E7" }
themes["(GPT4.1) Accessible High Contrast (WCAG AA/AAA)"] := { "TextColor": "#F8F8F2", "BackgroundColor": "#282A36" }
themes["Ultimate Teal Mod"] := { "TextColor": "#008B8B", "BackgroundColor": "#004D4D" }
themes["Ultimate Teal Mod BrightFix"] := { "TextColor": "#00aaaa", "BackgroundColor": "#004D4D" }
themes["Fuschia Pink Explosion Mod"] := { "TextColor": "#FFA1A1", "BackgroundColor": "#FF2B36" }
themes["Luminous ArmyGreen Mod"] := { "TextColor": "#8DF318", "BackgroundColor": "#93901D" }
themes["CyanRed Readability Mod"] := { "TextColor": "#D21016", "BackgroundColor": "#65D8B7" }
themes["PinkLust Mod"] := { "TextColor": "#1E2D35", "BackgroundColor": "#F108F0" }
themes["Edge ReadMode Mod"] := { "TextColor": "#621F56", "BackgroundColor": "#62BB92" }
themes["GreenBlue Mod"] := { "TextColor": "#104421", "BackgroundColor": "#7873E3" }
themes["Yellow Mod"] := { "TextColor": "#0939B0", "BackgroundColor": "#EBCE71" }
themes["LightLime Mod"] := { "TextColor": "#06C127", "BackgroundColor": "#B5E597" }
themes["StarkGreen Mod"] := { "TextColor": "#837F1E", "BackgroundColor": "#A2CC1E" }
themes["PurpleCyan Mod"] := { "TextColor": "#64CCF9", "BackgroundColor": "#662DD6" }
themes["CyanPurple Mod"] := { "TextColor": "#510A52", "BackgroundColor": "#5C97E1" }
themes["RoyalVelvet Mod"] := { "TextColor": "#B242B5", "BackgroundColor": "#220210" }
themes["SkyBlueMod"] := { "TextColor": "#1CD1B5", "BackgroundColor": "#0C3DD6" }
themes["YellowOnIndigoMod"] := { "TextColor": "#1F0954", "BackgroundColor": "#C4BB45" }
themes["RedOnPinkMod"] := { "TextColor": "#ff0000", "BackgroundColor": "#ff00ff" }
themes["Green BrightGlow"] := { "TextColor": "#39DFC4", "BackgroundColor": "#101F18" }
themes["Green BrightGlow2"] := { "TextColor": "#39B378", "BackgroundColor": "#10190F" }
themes["Green BrightGlow3"] := { "TextColor": "#38b479", "BackgroundColor": "#182617" }
themes["Dracula-DansLeRush-PurpleGPT"] := { "TextColor": "#B19CD9", "BackgroundColor": "#21222c" }
themes["Rosewood (Brown) Inverted Dark"] := { "TextColor": "#F5F0E1", "BackgroundColor": "#4B3C30" }
themes["latest-Green"] := { "TextColor": "#C7FBBA", "BackgroundColor": "#4E774F" }
themes["latest-LightBlue"] := { "TextColor": "#210150", "BackgroundColor": "#0951FE" }
themes["latest-LightPurple"] := { "TextColor": "#92D996", "BackgroundColor": "#370C9F" }
themes["latest-LimeyGreen"] := { "TextColor": "#136227", "BackgroundColor": "#98C021" }
themes["latest-PaleBlue"] := { "TextColor": "#393F0D", "BackgroundColor": "#168FB6" }
themes["latest-PaleFaded"] := { "TextColor": "#646F16", "BackgroundColor": "#9CBD93" }
themes["latest-PaleGreenPink"] := { "TextColor": "#E8B7DF", "BackgroundColor": "#317C71" }
themes["latest-BlueResplendence"] := { "TextColor": "#0D7409", "BackgroundColor": "#43C0EF" }
themes["latest-DarkLimeGreen"] := { "TextColor": "#85E555", "BackgroundColor": "#0B1C10" }
themes["latest-BlueVariated"] := { "TextColor": "#15F7CE", "BackgroundColor": "#072366" }
themes["latest-OrangeyReddish"] := { "TextColor": "#d80000", "BackgroundColor": "#e7a212" }
themes["latest-OrangierReddish"] := { "TextColor": "#d80000", "BackgroundColor": "#dc880a" }
themes["latest-DarkTeal"] := { "TextColor": "#a5887d", "BackgroundColor": "#dc880a" }
themes["latest-DarkTealier"] := { "TextColor": "#a5887d", "BackgroundColor": "#0e213d" }
themes["latest-DarkTealFluxNonNight"] := { "TextColor": "#a5887d", "BackgroundColor": "#1f242e" }
themes["latest-DarkTealc00BlueNonNight"] := { "TextColor": "#61748f", "BackgroundColor": "#1f242e" }
themes["latest-TeaRose"] := { "TextColor": "#D6B44A", "BackgroundColor": "#8D5650" }
themes["latest-PurpleIvory"] := { "TextColor": "#CB699B", "BackgroundColor": "#3D0633" }
themes["latest-Other"] := { "TextColor": "#D645D0", "BackgroundColor": "#122a4d" }
themes["latest-Color1"] := { "TextColor": "#04A325", "BackgroundColor": "#A1E809" }
themes["latest-Color2"] := { "TextColor": "#DD39FE", "BackgroundColor": "#891528" }
themes["latest-Color3"] := { "TextColor": "#340004", "BackgroundColor": "#BB1468" }
themes["latest-Color4-RedOnYellow"] := { "TextColor": "#F5D43A", "BackgroundColor": "#EC3233" }
themes["latest-Color5"] := { "TextColor": "#2CFD63", "BackgroundColor": "#417009" }
themes["latest-Color6"] := { "TextColor": "#90FE53", "BackgroundColor": "#76752F" }
themes["latest-Color7-BrownOnOrange"] := { "TextColor": "#F86A23", "BackgroundColor": "#382927" }
themes["latest-Color8-PurpleExpOnOrange"] := { "TextColor": "#B6380E", "BackgroundColor": "#400040" }
themes["Default"] := { "TextColor": "#000000", "BackgroundColor": "#2670BE" }

global FG := "",  BG := ""          ; colours currently shown in GUI

; -------------------------------------------------------------------
;  GUI
; -------------------------------------------------------------------
Gui, Font, s10, Segoe UI
Gui, Add, Text, xm ym, % "Select a theme (" themes.Count() " available):"

list := ""
for k in themes
    list .= k "|"
Gui, Add, ListBox, vLB gLB x10 y25 w480 h200, % RTrim(list,"|")

Gui, Add, Edit  , vColorEdit gColorEdit x10 y240 w480 r2 +Multi
Gui, Add, Button, gDoApply      x10  y305 w75 h24, Apply
Gui, Add, Button, gClose        x100 y305 w75 h24 -Default, Close

Gui, Add, Text    , x520 y35 , FG:
Gui, Add, Progress, x545 y30 w60 h25 vFGPrev hwndhFGPrev Range0-100, 100
Gui, Add, Text    , x520 y75 , BG:
Gui, Add, Progress, x545 y70 w60 h25 vBGPrev hwndhBGPrev Range0-100, 100
Gui, Add, Button,  gRandomColors x520 y110 w85 h24, Random

; intercept left-button clicks
OnMessage(0x201, "PreviewClick")   ; WM_LBUTTONDOWN

; -------------------------------------------------------------------
;  INITIALISATION
; -------------------------------------------------------------------
ReadColours(filePath, curFG, curBG)
FG := curFG, BG := curBG
UpdateEditField()
RefreshSquares(FG, BG)

Gui, Show, w640 h360, % "Theme Selector (" themes.Count() " Themes)"
return



; -------------------------------------------------------------------
;  GUI EVENTS
; -------------------------------------------------------------------
LB:
    Gui, Submit, NoHide
    if (LB = "")
        return
    FG := themes[LB].TextColor
    BG := themes[LB].BackgroundColor
    UpdateEditField()
    RefreshSquares(FG, BG)
    if (A_GuiEvent = "DoubleClick")
        ApplyWork()
return

#If WinActive("Theme Selector")
Enter::ApplyWork()
#If

DoApply:
    ApplyWork()
return

ColorEdit:
    GuiControlGet, txt,, ColorEdit
    ParseEdit(txt, FGtmp, BGtmp)
    if (FGtmp && BGtmp) {
        FG := FGtmp,  BG := BGtmp
        RefreshSquares(FG, BG)
    }
return

RandomColors:
    FG := GetRandomHexColor()
    BG := GetRandomHexColor()
    UpdateEditField()
    RefreshSquares(FG, BG)
    ApplyWork()
return

GuiClose:
GuiEscape:
Close:
ExitApp
return



; -------------------------------------------------------------------
;  WM_LBUTTONDOWN  →  CLICK ON PREVIEW SQUARES
; -------------------------------------------------------------------
PreviewClick(wParam, lParam, msg, hwnd) {
    global hFGPrev, hBGPrev

    if (hwnd = hFGPrev)
        target := "FG"
    else if (hwnd = hBGPrev)
        target := "BG"
    else
        return                        ; click was on some other control

    ; postpone opening the colour dialog until *after* this message
    global __TargetColour := target
    SetTimer, __OpenPicker, -1
    return 0                          ; swallow the click for Progress
}

__OpenPicker:
    global __TargetColour, FG, BG
    if (__TargetColour = "FG") {
        if new := ChooseColour(FG)
            FG := new
    } else if (__TargetColour = "BG") {
        if new := ChooseColour(BG)
            BG := new
    }
    UpdateEditField()
    RefreshSquares(FG, BG)
    ApplyWork()
return



; ===================================================================
;  MAIN “APPLY” ROUTINE
; ===================================================================
ApplyWork() {
    global FG, BG, filePath
    Gui, Submit, NoHide
    GuiControlGet, txt,, ColorEdit
    ParseEdit(txt, FGtmp, BGtmp)
    if !(FGtmp && BGtmp) {
        MsgBox, 48, Error, Couldn’t parse colours from edit box.
        return
    }
    FG := FGtmp,  BG := BGtmp

    if !(file := FileOpen(filePath, "r `n")) {
        MsgBox, 48, Error, Couldn’t open:`n%filePath%
        return
    }
    all := file.Read(), file.Close()

    all := RegExReplace(all
        , "Oim)[ \t]*TextColor[ \t]*=\s*#[0-9A-Fa-f]{6}"
        , "`tTextColor = " FG)
    all := RegExReplace(all
        , "Oim)[ \t]*BackgroundColor[ \t]*=\s*#[0-9A-Fa-f]{6}"
        , "`tBackgroundColor = " BG)

    file := FileOpen(filePath, "w `n"), file.Write(all), file.Close()
    RefreshSquares(FG, BG)
}



; ===================================================================
;  HELPER FUNCTIONS
; ===================================================================
UpdateEditField() {
    global FG, BG
    GuiControl,, ColorEdit, % "`tTextColor = " FG "`n`tBackgroundColor = " BG
}

ReadColours(path, ByRef FG, ByRef BG) {
    FG := "#000000", BG := "#FFFFFF"
    if !(file := FileOpen(path, "r `n"))
        return
    txt := file.Read(), file.Close()
    pattern := "Oim)\s*(TextColor|BackgroundColor)\s*=\s*(#[0-9A-Fa-f]{6})"
    pos := 1
    while pos := RegExMatch(txt, pattern, m, pos) {
        (m[1]="TextColor" ? FG : BG) := m[2]
        pos += StrLen(m[0])
    }
}

ParseEdit(src, ByRef FG, ByRef BG) {
    FG := BG := ""
    pattern := "Oim)\s*(TextColor|BackgroundColor)\s*=\s*(#[0-9A-Fa-f]{6})"
    pos := 1
    while pos := RegExMatch(src, pattern, m, pos) {
        (m[1]="TextColor" ? FG : BG) := m[2]
        pos += StrLen(m[0])
    }
}

RefreshSquares(FG, BG) {
    if (!RegExMatch(FG,"^#[0-9A-Fa-f]{6}$") || !RegExMatch(BG,"^#[0-9A-Fa-f]{6}$"))
        FG := "#000000", BG := "#FFFFFF"

    FGhex := SubStr(FG,2)
    BGhex := SubStr(BG,2)
    GuiControl, +c%FGhex%, FGPrev
    GuiControl, +c%BGhex%, BGPrev
}

GetRandomHexColor() {
    Random, r, 0, 255
    Random, g, 0, 255
    Random, b, 0, 255
    return "#" Format("{:02X}{:02X}{:02X}",r,g,b)
}



; -------------------------------------------------------------------
;  STANDARD WINDOWS COLOUR PICKER  (x86 & x64 safe)
; -------------------------------------------------------------------
ChooseColour(initialHex := "#000000")
{
    initBGR := HexToBGR(initialHex)

    if (A_PtrSize=8) {            ; 64-bit structure offsets
        VarSetCapacity(cc,72,0), VarSetCapacity(cust,64,0)
        NumPut(72           , cc, 0 , "UInt")   ; lStructSize
        NumPut(A_ScriptHwnd , cc, 8 , "Ptr")    ; hwndOwner
        NumPut(initBGR      , cc, 24, "UInt")   ; rgbResult
        NumPut(&cust        , cc, 32, "Ptr")    ; lpCustColors
        NumPut(0x1|0x2      , cc, 40, "UInt")   ; Flags (INIT|FULLOPEN)
        retOff := 24                             ; rgbResult offset
    } else {                     ; 32-bit
        VarSetCapacity(cc,36,0), VarSetCapacity(cust,64,0)
        NumPut(36           , cc, 0 , "UInt")
        NumPut(A_ScriptHwnd , cc, 4 , "UInt")
        NumPut(initBGR      , cc, 12, "UInt")
        NumPut(&cust        , cc, 16, "UInt")
        NumPut(0x1|0x2      , cc, 20, "UInt")
        retOff := 12
    }

    if !DllCall("Comdlg32\ChooseColor" (A_IsUnicode?"W":"A")
               ,"Ptr",&cc,"Int")
        return ""                               ; cancel/error

    return BGRtoHex( NumGet(cc,retOff,"UInt") )
}

; --------------------------------------------------------------------
;  CORRECT colour-conversion helpers  (now use 2-4-6 indices)
; --------------------------------------------------------------------
HexToBGR(hex) {                   ; "#RRGGBB" → DWORD 0xBBGGRR
    r := "0x" SubStr(hex, 2, 2)   ; chars 2-3
    g := "0x" SubStr(hex, 4, 2)   ; chars 4-5
    b := "0x" SubStr(hex, 6, 2)   ; chars 6-7
    return (b << 16) | (g << 8) | r
}

BGRtoHex(bgr) {                   ; DWORD 0xBBGGRR → "#RRGGBB"
    r :=  bgr        & 0xFF
    g := (bgr >> 8)  & 0xFF
    b := (bgr >> 16) & 0xFF
    return "#" Format("{:02X}{:02X}{:02X}", r, g, b)
}

; (The ChooseColour() routine remains unchanged; it already
;  passes `initBGR` and sets the CC_RGBINIT flag.)