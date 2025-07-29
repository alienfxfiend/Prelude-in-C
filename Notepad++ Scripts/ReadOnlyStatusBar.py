# -*- coding: utf-8 -*-
"""
Ln / Col / Sel in the CURPOS field, RO/RW in its own dedicated field.
Works on any PythonScript version (both Python 2 and 3).
"""
from Npp import *

# ------------------------------------------------------------------
# Helper : convert a numeric index (0-5) to a STATUSBARSECTION value
# ------------------------------------------------------------------
def sb_enum(idx):
    """
    Return a proper STATUSBARSECTION enum object for the given index.
    Guaranteed to work even when some symbolic names are missing.
    """
    try:
        return STATUSBARSECTION(idx)       # Boost.Python enum is callable
    except Exception:
        # very old PS builds aren’t callable – fall back to CURPOS + offset
        base = int(STATUSBARSECTION.CURPOS)  # CURPOS is always 1
        return STATUSBARSECTION(int(base + (idx - 1)))

# we want to reuse column 5 (normally empty).  Use 4 if you prefer INS/OVR.
RO_SECTION = sb_enum(5)

# ------------------------------------------------------------------
# Notifications that trigger an update
# ------------------------------------------------------------------
SCINT_EVENTS = [SCINTILLANOTIFICATION.UPDATEUI]
NPP_EVENTS   = [NOTIFICATION.READONLYCHANGED,
                NOTIFICATION.BUFFERACTIVATED,
                NOTIFICATION.FILEOPENED,
                NOTIFICATION.FILECLOSED]

# clear callbacks from earlier runs
for evts, obj in ((SCINT_EVENTS, editor), (NPP_EVENTS, notepad)):
    try:
        obj.clearCallbacks(evts)
    except Exception:
        pass

# ------------------------------------------------------------------
# Worker
# ------------------------------------------------------------------
def update_statusbar(args=None):

    # “editor” is None during the very first moments of start-up → bail out
    if editor is None:
        return

    # caret position
    pos = editor.getCurrentPos()
    ln  = editor.lineFromPosition(pos) + 1
    col = editor.getColumn(pos) + 1

    # selection info
    sel_info = 'N/A'
    if editor.getSelections() == 1:
        if editor.getSelectionEmpty():
            sel_info = '0 | 0'
        else:
            ss, se   = editor.getSelectionStart(), editor.getSelectionEnd()
            ln0, ln1 = editor.lineFromPosition(ss), editor.lineFromPosition(se)
            lines = ln1 - ln0 + 1
            at_sol = '^' if ss == editor.positionFromLine(ln0) else ''
            at_eol = ''
            txt = editor.getSelText()
            if se == editor.getLineEndPosition(ln1):
                at_eol = '$'
            if txt.endswith(('\r\n', '\r', '\n')):
                at_eol = '$\\R'
                lines -= 1
            sel_info = '{chars} | {sol}{lines}{eol}'.format(
                           chars=len(txt), sol=at_sol, lines=lines, eol=at_eol)

    # push Ln/Col/Sel to CURPOS
    notepad.setStatusBar(STATUSBARSECTION.CURPOS,
                         'Ln : {ln:<6}Col : {col:<6}Sel : {sel}'.format(
                             ln=ln, col=col, sel=sel_info))

    # push RO/RW to our dedicated column
    notepad.setStatusBar(RO_SECTION,
                         'WP+' if editor.getReadOnly() else 'ED-')

# ------------------------------------------------------------------
# Register callbacks
# ------------------------------------------------------------------
editor.callback(update_statusbar, SCINT_EVENTS)
notepad.callback(update_statusbar, NPP_EVENTS)

# first manual update (will silently do nothing if editor is None)
update_statusbar()