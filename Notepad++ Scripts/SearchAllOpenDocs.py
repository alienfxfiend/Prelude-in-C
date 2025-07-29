# Notepad++ PythonScript: Simulated Regex Toggle via Input Text

search_input = notepad.prompt("Enter search text (add '--re' for regex mode):", "Multi-File Search")
search_flags = 0  # 0 = case-sensitive, 1 = case-insensitive

if search_input:
    # Check for '--re' flag
    use_regex = "--re" in search_input
    search_term = search_input.replace("--re", "").strip()

    open_files = notepad.getFiles()

    for file_info in open_files:
        file_path = file_info[0]
        notepad.activateFile(file_path)
        editor.gotoPos(0)

        if use_regex:
            pos = editor.findTextRE(search_flags, 0, editor.getTextLength(), search_term)
        else:
            pos = editor.findText(search_flags, 0, editor.getTextLength(), search_term)

        if pos:
            editor.gotoPos(pos[0])
            result = notepad.messageBox("Match found in:\n" + file_path + "\n\nContinue to next?", "Jump to Match", 4)
            if result == 7:  # 7 = No
                console.write("Search interrupted by user.\n")
                break
        else:
            console.write("No match in " + file_path + "\n")
else:
    console.write("Search cancelled by user.\n")
