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
;  BUTTON "GO!"
;----------------------------------------------------------
Short:
        Gui, Submit, NoHide                    ; retrieve URLWant
        ; Normalise white-space (turn CR/LF or TAB into a single SPACE)
        All := RegExReplace(URLWant, "\s+", " ")

        out := ""                               ; final string that we’ll push back
        Loop, Parse, All, %A_Space%             ; handle EVERY single token
        {
                u := A_LoopField
                if (u = "")                       ; extra safety – ignore empty tokens
                        continue

                ;-----------------------------  YOUTUBE
                if RegExMatch(u, "i)^https?://(?:www|m)\.youtube\.com/")
                {
                        u := RegExReplace(u, "i)m\.youtube\.com", "www.youtube.com")          ; 1) m. → www.
                        u := RegExReplace(u, "&pp=.*")                                        ; 2) drop &pp=… and everything after
                        u := RegExReplace(u, "i)https?://www\.youtube\.com/watch\?v=", "https://youtu.be/")  ; 3) compact link
                }
                ;-----------------------------  DROPBOX
                else if RegExMatch(u, "i)^https?://www\.dropbox\.com/")
                {
                        ; Add ?raw=1  (or &raw=1 if there is already a query-string)
                        if !RegExMatch(u, "i)([&?]raw=1)$")
                                u .= (InStr(u, "?") ? "&" : "?") "raw=1"
                }

                out .= u . A_Space                   ; keep the spacing
        }
        StringTrimRight, out, out, 1           ; trailing space
        GuiControl,, URLWant, %out%            ; show converted URLs
return

;----------------------------------------------------------
;  CLOSE GUI
;----------------------------------------------------------
GuiEscape:
GuiClose:
        Gui, Destroy
return