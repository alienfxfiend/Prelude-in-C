#SingleInstance Force
#NoEnv
SetBatchLines, -1
Process, Priority, , High  ; Run with higher priority
DetectHiddenWindows, On  ; Detect hidden windows
;#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.
#Persistent

filePath := "D:\Download\DL Dir\pendrive\web content\cyberpunk ebooks\backups\Sumatra Proto\SumatraPDF-settings.txt"

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
themes["Fuschia Pink Explosion"] := { "TextColor": "#FFA1A1", "BackgroundColor": "#FF2B36" }
themes["Luminous ArmyGreen Mod"] := { "TextColor": "#8DF318", "BackgroundColor": "#93901D" }
themes["CyanRed Readability Mod"] := { "TextColor": "#D21016", "BackgroundColor": "#65D8B7" }
themes["Default"] := { "TextColor": "#000000", "BackgroundColor": "#ffffff" }

; ---------------- GUI ------------------------------------------------
Gui, Font, s10, Segoe UI
Gui, Add, Text, xm ym, % "Select a theme (" themes.Count() " available):"

for k in themes
    list .= k "|"
Gui, Add, ListBox, vLB gLB x10 y25 w480 h200, % RTrim(list,"|")

;Gui, Add, Edit, vColorEdit x10 y240 w480 h50 ReadOnly
Gui, Add, Edit, vColorEdit gColorEdit x10 y240 w480 r2 +Multi
Gui, Add, Button, gApply x10  y305 w75 h24, Apply
Gui, Add, Button, gClose x100 y305 w75 h24 -Default, Close

Gui, Add, Text    , x520 y35 , FG:
Gui, Add, Progress, x545 y30 w60 h25 vFGPrev Range0-100, 100
Gui, Add, Text    , x520 y75 , BG:
Gui, Add, Progress, x545 y70 w60 h25 vBGPrev Range0-100, 100
;Gui, Add, Text    , x520 y115, Hardcoded Red Box:
;Gui, Add, Progress, x545 y110 w60 h25 vRedBox cFF0000 Range0-100, 100

ReadColours(filePath, curFG, curBG)
GuiControl,, ColorEdit, % "`tTextColor = " curFG "`n`tBackgroundColor = " curBG
Gui, Show, w640 h360, % "Theme Selector (" themes.Count() " Themes)"
Sleep, 200
RefreshSquares(curFG, curBG)
return

; ------------ list-box ----------------------------------------------
LB:
    Gui, Submit, NoHide
    if (LB = "")
        return
    t := themes[LB]
    FG := t.TextColor, BG := t.BackgroundColor
    GuiControl,, ColorEdit, % "`tTextColor = " FG "`n`tBackgroundColor = " BG
    RefreshSquares(FG,BG)
    if (A_GuiEvent = "DoubleClick")
        gosub Apply
return

#If WinActive("Theme Selector")
Enter::GoSub, Apply
#If

; ------------ APPLY --------------------------------------------------
Apply:
    Gui, Submit, NoHide
    GuiControlGet, txt,, ColorEdit
    ParseEdit(txt, FG, BG)

    if !(FG && BG) {
        MsgBox, 48, Error, Couldn’t parse colours from edit box.
        return
    }

    file := FileOpen(filePath, "r `n")
    all  := file.Read(), file.Close()

    ; —-- replace only the colour value ‑-----------------------------
all := RegExReplace(all
        , "Oim)[ \t]*TextColor[ \t]*=\s*#[0-9A-Fa-f]{6}"          ; <- no ^
        , "`tTextColor = " FG)

all := RegExReplace(all
        , "Oim)[ \t]*BackgroundColor[ \t]*=\s*#[0-9A-Fa-f]{6}"    ; <- no ^
        , "`tBackgroundColor = " BG)

    file := FileOpen(filePath, "w `n"), file.Write(all), file.Close()
    RefreshSquares(FG,BG)
    
    ; -----------------------------------------------------------------
    ; show which theme has been applied
    ; -----------------------------------------------------------------
    SelectedTheme := (LB != "") ? LB : "Custom"
    MsgBox, 64, Success, The theme "%SelectedTheme%" has been applied.`n`nFG: %FG%`nBG: %BG%
return

GuiClose:
GuiEscape:
Close:
ExitApp
return


; =====================================================================
;  FUNCTIONS
; =====================================================================

ReadColours(path, ByRef FG, ByRef BG)
{
    FG := "#000000", BG := "#FFFFFF"
    if !(file := FileOpen(path, "r `n"))
        return
    txt := file.Read(), file.Close()

    ; no leading ^  vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    pattern := "Oim)\s*(TextColor|BackgroundColor)\s*=\s*(#[0-9A-Fa-f]{6})"

    pos := 1
    while pos := RegExMatch(txt, pattern, m, pos)
    {
        (m[1]="TextColor" ? FG : BG) := m[2]
        pos += StrLen(m[0])      ; continue after this match
    }
}

ParseEdit(src, ByRef FG, ByRef BG)
{
    FG := BG := ""
    ; no leading ^  vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    pattern := "Oim)\s*(TextColor|BackgroundColor)\s*=\s*(#[0-9A-Fa-f]{6})"

    pos := 1
    while pos := RegExMatch(src, pattern, m, pos)
    {
        (m[1]="TextColor" ? FG : BG) := m[2]
        pos += StrLen(m[0])
    }
}

; --------------------------------------------------------------------
RefreshSquares(FG, BG)
{
    if (!RegExMatch(FG, "^#[0-9A-Fa-f]{6}$") || !RegExMatch(BG, "^#[0-9A-Fa-f]{6}$"))
        FG := "#000000", BG := "#FFFFFF"

    StringTrimLeft, FGhex, FG, 1
    StringTrimLeft, BGhex, BG, 1
    GuiControl, +c%FGhex%, FGPrev
    GuiControl, +c%BGhex%, BGPrev
}

ColorEdit:                 ; fires whenever the user edits the box
    GuiControlGet, txt,, ColorEdit
    ParseEdit(txt, FGtmp, BGtmp)
    if (FGtmp && BGtmp)
        RefreshSquares(FGtmp, BGtmp)
return