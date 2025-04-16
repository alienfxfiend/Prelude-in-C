#SingleInstance Force  ; Replace old instances automatically
SetBatchLines, -1  ; Make script run at maximum speed
Process, Priority, , High  ; Run with higher priority
DetectHiddenWindows, On  ; Detect hidden windows
#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.
#Persistent

; Cleanup any previous GUI instances.
Gui, Destroy
Sleep, 50  ; Small delay to ensure resources are freed


; -------------------------------
; CONFIGURATION
; -------------------------------

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
themes["Default"] := { "TextColor": "#000000", "BackgroundColor": "#ffffff" }

; Settings file path
filePath := "D:\Download\DL Dir\pendrive\web content\cyberpunk ebooks\backups\Sumatra Proto\SumatraPDF-settings.txt"

; -------------------------------
; GUI SETUP
; -------------------------------

themeCount := 0
for k, v in themes
    themeCount++

Gui, Font, S10, Verdana
Gui, Add, Text, xm ym, Select a theme (%themeCount% available):

; Build listbox item list (separated by pipe characters)
themeNames := ""
for k, v in themes
    themeNames .= k . "|"
themeNames := SubStr(themeNames, 1, -1)  ; Remove trailing "|"

Gui, Add, ListBox, vSelectedTheme x10 y10 w480 h200 gListBoxClick, %themeNames%

; Add multi-line Edit control to show/edit current FG/BG colors.
Gui, Add, Edit, vColorEdit x10 y220 w480 h50,

Gui, Add, Button, x10 y280 w75 h25 gApply, Apply
GuiControl, +0x00000001, Apply ; Explicitly set the BS_PUSHBUTTON style
Gui, Add, Button, x100 y280 w75 h25 gGuiClose -Default, Close

Gui, +LastFound
WinSet, Redraw

; Ensure the window is tall enough to show all controls.
Gui, Show, w500 h350, Theme Selector

; Initialize the textbox with current FG and BG colors from the config file.
FileRead, fileContent, %filePath%
FGColor := ""
BGColor := ""
if (!ErrorLevel) {
    if RegExMatch(fileContent, "m)^\s*(?i)`tTextcolor\s*=\s*(\S+)", m)
        FGColor := m1
    if RegExMatch(fileContent, "m)^\s*(?i)`tBackgroundcolor\s*=\s*(\S+)", m)
        BGColor := m1
}

initText := "`tTextColor = " FGColor "`n`tBackgroundColor = " BGColor
GuiControl,, ColorEdit, %initText%

#If WinActive("Theme Selector")
Enter::GoSub, Apply
#If

Return

; -------------------------------
; EVENT HANDLERS
; -------------------------------

ListBoxClick:
    Gui, Submit, NoHide
    if (SelectedTheme != "") {
        themeColors := themes[SelectedTheme]
        newTextColor := themeColors.TextColor
        newBackColor := themeColors.BackgroundColor
        GuiControl,, ColorEdit, `tTextColor = %newTextColor%`n`tBackgroundColor = %newBackColor%
    }

    if (A_GuiEvent = "DoubleClick") {
        ; Optionally, you can automatically apply the changes on double-click.
        Gosub, Apply
    }
Return

Apply:
    Gui, Submit, NoHide

    ; Get the text from the ColorEdit textbox
    GuiControlGet, MultiEdit, , ColorEdit

    ; Split the text into lines, handling both LF and CRLF
    lines := StrSplit(MultiEdit, "`n", "`r")

    if (lines.Length() < 2)
    {
        MsgBox, Please ensure that TextColor and BackgroundColor are both specified.
        return
    }

    newFG := ""
    newBG := ""

    ; Process each line to extract color values
    for index, line in lines
    {
        line := Trim(line)
        if RegExMatch(line, "i)^\s*TextColor\s*=\s*(\S+)", match)
        {
            newFG := Trim(match1)
        }
        else if RegExMatch(line, "i)^\s*BackgroundColor\s*=\s*(\S+)", match)
        {
            newBG := Trim(match1)
        }
    }

    if (newFG = "" || newBG = "")
    {
        MsgBox, Please ensure that TextColor and BackgroundColor are both specified correctly. The format should be:`n`tTextColor = #RRGGBB`n`tBackgroundColor = #RRGGBB
        return
    }

    ; Update the fileContent with the new colors.
    fileContent := RegExReplace(fileContent, "m)^\s*(?i)Textcolor\s*=\s*.*$", "`tTextColor = " . newFG, 1)
    fileContent := RegExReplace(fileContent, "m)^\s*(?i)Backgroundcolor\s*=\s*.*$", "`tBackgroundColor = " . newBG, 1)

    ; Write the updated content back to file.
;    FileWrite, %fileContent%, %filePath%, Overwrite

;    MsgBox, Colors updated successfully:`nTextColor = %newFG%`nBackgroundColor = %newBG%

    ; Write the modified content back to the settings file.
    FileDelete, %filePath%
    FileAppend, %fileContent%, %filePath%
    if (ErrorLevel) {
        MsgBox, 16, Error, Could not write to the settings file!
    } else {
        MsgBox, 64, Success, The theme %SelectedTheme% has been applied.
    }

Return

GuiClose:
GuiEscape:
    SetTimer, ActuallyExit, -100
    Return

ActuallyExit:
    Gui, Destroy
    Sleep, 50
    ExitApp