; Add at the beginning of your script, right after the first few lines
#SingleInstance Force  ; Replace old instances automatically
SetBatchLines, -1  ; Make script run at maximum speed
Process, Priority, , High  ; Run with higher priority
DetectHiddenWindows, On  ; Detect hidden windows
#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.
#Persistent
;Gui, Destroy  ; Clean up any previous GUI instances before creating a new one
; Initialize with cleanup before creating new GUI
Gui, Destroy
Sleep, 50  ; Small delay to ensure resources are freed

; -------------------------------
; CONFIGURATION
; -------------------------------

; Define the preconfigured themes as an object.
; In each theme the keys "TextColor" and "BackgroundColor" hold the desired values.
themes := {}
themes["IndigoOnPink"] := { "TextColor": "#ee3f86", "BackgroundColor": "#1B0032" }
; Add more themes as needed. Example: hotfudgesundae #f39900 #cfba28 | MP #2c013c CX #3a1c13 BL #000000 Gold #B99350 Violet #400080 + darkred #FB490E reddish #800000 + brown #400000 indigo=#4B0082 orangered=#FF4500 hotpink=#ff084a darkgrey=#4c4e50 lightgray=#5f6264 extradarkgray=#262728
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
themes["NeonGreenLuminesce"] := { "TextColor": "#262728", "BackgroundColor": "#2CFF05" } ;#4B0082-indigo
themes["Default"] := { "TextColor": "#000000", "BackgroundColor": "#ffffff" }

; Path to the settings file that will be updated.
filePath := "D:\Download\DL Dir\pendrive\web content\cyberpunk ebooks\backups\Sumatra Proto\SumatraPDF-settings.txt"


; -------------------------------
; GUI SETUP
; -------------------------------
; Count the number of themes
themeCount := 0
for k, v in themes
    themeCount++

; Display label with theme count
Gui, Add, Text, xm ym, Select a theme (%themeCount% available):
;Gui, Add, Text, xm ym, Select a theme:
; Create a listbox showing the available theme names (separated by | characters)
themeNames := ""
for k, v in themes
    themeNames .= k . "|"
themeNames := SubStr(themeNames, 1, -1)  ; remove trailing bar

;Gui, Add, ListBox, vSelectedTheme xm+5 yp+25 w270 h130 gApply, %themeNames% ;5,25,200,100
Gui, Add, ListBox, vSelectedTheme xm+5 yp+25 w270 h130 gListBoxClick, %themeNames% ; Changed from gApply
;Gui, Add, Button, xm yp+140 w80 gApply, Apply
; Add this line directly after the Apply button line in the GUI SETUP section
;Gui, Add, Button, x+120 w80 gGuiClose, Close ;+10
Gui, Add, Button, xm yp+140 w80 gApply +0x8000, Apply
;Gui, Add, Button, x+120 w80 gGuiClose +0x8000, Close
Gui, Add, Button, x210 yp w80 gGuiClose +0x8000, Close
; Add this before showing your GUI
Gui, +LastFound
WinSet, Redraw  ; Force a redraw of the window
Gui, Show, w300 h200, Theme Selector
Return

; -------------------------------
; EVENT HANDLERS
; -------------------------------

; Add this new handler function:
ListBoxClick:
    if (A_GuiEvent = "DoubleClick") {
        Gosub, Apply
    }
Return

Apply:
    Gui, Submit, NoHide
    if (SelectedTheme = "")
    {
        MsgBox, Please select a theme first.
        Return
    }
   
    ; Retrieve the chosen theme's color values
    themeColors := themes[SelectedTheme]
    newTextColor := themeColors.TextColor
    newBackColor := themeColors.BackgroundColor

    ; Read the entire file contents into a variable.
    FileRead, fileContent, %filePath%
    if (ErrorLevel) {
        MsgBox, 16, Error, Could not read the settings file`n"%filePath%"
        Return
    }

    ; Replace the line that starts with "Textcolor = " (case insensitive)
    ; We assume the line begins with the word and an equals sign, then any text.
    fileContent := RegExReplace(fileContent, "m)^\s*(?i)Textcolor\s*=\s*.*$", "`tTextColor = " newTextColor, 1)
    ; Replace the line that starts with "Backgroundcolor = " (case insensitive)
    fileContent := RegExReplace(fileContent, "m)^\s*(?i)Backgroundcolor\s*=\s*.*$", "`tBackgroundColor = " newBackColor, 1)

    ; (Optional) Create a backup of the current settings file.
    ;backupPath := filePath ".bak"
    ;FileDelete, %backupPath%
    ;FileCopy, %filePath%, %backupPath%
    ;if ErrorLevel
    ;{
    ;    MsgBox, 48, Warning, Could not create a backup file.
    ;}

    ; Write the modified content back to the settings file.
    FileDelete, %filePath%
    FileAppend, %fileContent%, %filePath%
    if (ErrorLevel) {
        MsgBox, 16, Error, Could not write to the settings file!
    } else {
        MsgBox, 64, Success, The theme %SelectedTheme% has been applied.
    }
Return

; Replace your close handlers with this
GuiClose:
GuiEscape:
    SetTimer, ActuallyExit, -100  ; Delay exit slightly to allow GUI to clean up
    Return

ActuallyExit:
    Gui, Destroy
    Sleep, 50
    ExitApp


; Then replace your current exit handlers with these enhanced ones:
;GuiClose:
;GuiEscape:
;    Gui, Destroy  ; Properly destroy the GUI
;    ExitApp
;    Return
    
; Add this to the end of your script
;OnExit, ExitCleanup  ; Register a cleanup handler

;ExitCleanup:
;    Gui, Destroy  ; Extra cleanup to be sure
;    ExitApp

;GuiClose:
;    ExitApp
    
;GuiEscape:
;    ExitApp