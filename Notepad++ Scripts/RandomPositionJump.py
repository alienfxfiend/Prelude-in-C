import random

# Get total number of characters in the document
doc_length = editor.getTextLength()

if doc_length == 0:
    console.write("Document is empty.\n")
else:
    # Get first and last visible lines in viewport
    first_visible_line = editor.getFirstVisibleLine()
    lines_on_screen = editor.linesOnScreen()
    last_visible_line = first_visible_line + lines_on_screen - 1

    # Convert visible lines to character position range
    try:
        start_visible_pos = editor.positionFromLine(first_visible_line)
    except:
        start_visible_pos = 0

    try:
        end_visible_pos = editor.getLineEndPosition(last_visible_line)
    except:
        end_visible_pos = doc_length - 1

    # Define ranges outside the visible viewport
    valid_ranges = []
    if start_visible_pos > 0:
        valid_ranges.append((0, start_visible_pos - 1))
    if end_visible_pos < doc_length - 1:
        valid_ranges.append((end_visible_pos + 1, doc_length - 1))

    # If there are no valid ranges, jump anywhere as fallback
    if not valid_ranges:
        random_pos = random.randint(0, doc_length - 1)
    else:
        # Choose a random range, then a random position within that range
        selected_range = random.choice(valid_ranges)
        random_pos = random.randint(*selected_range)

    # Jump to the position
    editor.gotoPos(random_pos)
    editor.scrollCaret()