# -*- coding: utf-8 -*-
# The lines up to and including sys.stderr should always come first
# Then any errors that occur later get reported to the console
# If you'd prefer to report errors to a file, you can do that instead here.
import sys
from Npp import *

# Set the stderr to the normal console as early as possible, in case of early errors
sys.stderr = console

# Define a class for writing to the console in red
class ConsoleError:
	def __init__(self):
		global console
		self._console = console;

	def write(self, text):
		self._console.writeError(text);

# Set the stderr to write errors in red
sys.stderr = ConsoleError()

# This imports the "normal" functions, including "help"
import site

sys.stdout = console

# In order to set the stdout to the current active document, uncomment the following line
# sys.stdout = editor
# So print "hello world", will insert "hello world" at the current cursor position

# ----------------------------------------------------------------------
#  LOAD EXTERNAL SCRIPTS (edit only this block)
# ----------------------------------------------------------------------
import os, sys
sys.dont_write_bytecode = True     # <- add BEFORE run_script(...)

def run_script(filepath, modulename=None):
    """
    Execute an external .py file by absolute path.
    Works on both Python 2 (imp) and Python 3 (importlib).
    """
    filepath = os.path.expandvars(os.path.expanduser(filepath))
    if not os.path.isfile(filepath):
        console.writeError('[startup] file not found: {}\n'.format(filepath))
        return

    if modulename is None:
        modulename = os.path.splitext(os.path.basename(filepath))[0]

    try:
        if sys.version_info[0] < 3:           # ---- Python 2 branch ----
            import imp
            imp.load_source(modulename, filepath)
        else:                                 # ---- Python 3 branch ----
            import importlib.util
            spec   = importlib.util.spec_from_file_location(modulename, filepath)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)

    except Exception as e:
        console.writeError('[startup] failed to run {} : {}\n'.format(filepath, e))


# ---- RUN YOUR STATUS-BAR SCRIPT (edit only this path) -----------------
run_script(r'C:\Users\dell\AppData\Roaming\Notepad++\plugins\config\PythonScript\scripts\ReadOnlyStatusBar.py')