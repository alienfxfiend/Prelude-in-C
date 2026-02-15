#SingleInstance force
#NoEnv
SetWorkingDir %A_ScriptDir%

;----------------------------------------------------------
;  HOTKEY – pop-up GUI
;----------------------------------------------------------
#^y::
        Gui, New, , URL Converter
        Gui, Add, Text, +0x80, Paste one or more URL(s) separated by spaces (or line-breaks): DB=&raw=1 &dl=1
        Gui, Add, Edit, w500 r5 vURLWant
        Gui, Add, Button, x+5 Default gShort, GO!
        Gui -SysMenu +ToolWindow +AlwaysOnTop
        Gui, Show
return

;----------------------------------------------------------
;  BUTTON "GO!" – mirrors N++ macro step by step
;----------------------------------------------------------
Short:
        Gui, Submit, NoHide

        out := URLWant

        ; Step 1: Strip &pp= and everything non-whitespace after it (regex)
        out := RegExReplace(out, "i)&pp=\S+", "")

        ; Step 2: Convert m.youtube.com/ → www.youtube.com/
        ;         (handles /shorts/, /live/, /playlist, etc.)
        out := RegExReplace(out, "i)m\.youtube\.com/", "www.youtube.com/")

        ; Step 3: Convert www.youtube.com/watch?v= → youtu.be/
        ;         (only /watch?v= URLs become short links)
        out := RegExReplace(out, "i)www\.youtube\.com/watch\?v=", "youtu.be/")

        ; Step 4: Convert www.dropbox → dl.dropboxusercontent
        out := RegExReplace(out, "i)www\.dropbox", "dl.dropboxusercontent")

        ; Step 5: Remove ?dl=0
        out := RegExReplace(out, "i)\?dl=0", "")

        GuiControl,, URLWant, %out%
return

;----------------------------------------------------------
;  CLOSE GUI
;----------------------------------------------------------
GuiEscape:
GuiClose:
        Gui, Destroy
return