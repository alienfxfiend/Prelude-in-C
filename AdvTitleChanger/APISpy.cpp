#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <TlHelp32.h>
#include <tchar.h>
#include <Psapi.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <thread>
#include <WinUser.h>
#include <richedit.h>
#include <shellapi.h>
#include "resource.h"

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")

#define WS_EX_NOMAXIMIZEBOX (WS_EX_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX)

bool isCopying = false;
bool isPasting = false;
bool running = true;

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

struct ProcessInfo {
    std::wstring processName;
    std::wstring windowTitle;
    HWND hwnd;
};

std::vector<ProcessInfo> allProcesses;
std::vector<ProcessInfo> targetProcesses;
std::vector<std::wstring> processNames;

HHOOK keyboardHook;
HWND g_hListDialog = NULL; // Global handle for the list dialog
HWND g_mainHwnd; // Global handle for the main window

// Function prototypes
void DeleteWordToLeft(HWND hEdit);
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void AppendTextToRichEdit(HWND hRichText, const std::wstring& text);
BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
bool ModifyWindowTitles(const std::wstring& newTitle);
DWORD WINAPI TitleModifierThread(LPVOID lpParam);
std::vector<std::wstring> Split(const std::wstring& s, wchar_t delimiter);
void ProcessInput(HWND hwnd);
void OpenListDialog(HWND parentHwnd);

HWND hRichTextBox, hTextBox, hButton, hCloseButton, hGroupBox, hLabel;
HWND hListView = NULL;

// Low-level keyboard hook procedure
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        HWND foregroundWindow = GetForegroundWindow();

        if (kbdStruct->vkCode == VK_ESCAPE) {
            if (g_hListDialog && IsWindow(g_hListDialog) &&
                (foregroundWindow == g_hListDialog || IsChild(g_hListDialog, foregroundWindow))) {
                PostMessage(g_hListDialog, WM_KEYDOWN, VK_ESCAPE, 0);
            }
            else if (foregroundWindow == g_mainHwnd || IsChild(g_mainHwnd, foregroundWindow)) {
                PostMessage(g_mainHwnd, WM_USER + 1, VK_ESCAPE, 0);
            }
            return 1;
        }
        else if (kbdStruct->vkCode == VK_F1) {
            if (foregroundWindow == g_mainHwnd || IsChild(g_mainHwnd, foregroundWindow)) {
                PostMessage(g_mainHwnd, WM_USER + 1, VK_F1, 0);
                return 1;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void UninstallHook() {
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }
}

void InstallHook(HWND hWnd) {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!keyboardHook) {
        // Handle error - could not set hook
        DWORD errorCode = GetLastError();
        // Log or display the error
    }
}

void ResetFocusAndHookState() {
    if (g_mainHwnd) {
        SetFocus(g_mainHwnd); // Ensure the main window gets focus back
                // Optionally, bring the window to the foreground
        SetForegroundWindow(g_mainHwnd);
    }

    // Refresh the hook by uninstalling and then reinstalling it
    if (keyboardHook) {
        UninstallHook(); // Assuming this function is defined to uninstall the hook
        InstallHook(g_mainHwnd); // Assuming this function is defined to install the hook
    }
}

void OpenListDialog(HWND parentHwnd) {
    g_hListDialog = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), parentHwnd, DialogProc);
    if (g_hListDialog != NULL) {
        ShowWindow(g_hListDialog, SW_SHOW);
    }
}

void AppendTextToRichEdit(HWND hRichText, const std::wstring& text) {
    int len = GetWindowTextLength(hRichText);
    SendMessage(hRichText, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hRichText, EM_REPLACESEL, 0, (LPARAM)text.c_str());
}

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) {
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
        AppendTextToRichEdit(hRichTextBox, L"LookupPrivilegeValue error: " + std::to_wstring(GetLastError()) + L"\n");
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) {
        AppendTextToRichEdit(hRichTextBox, L"AdjustTokenPrivileges error: " + std::to_wstring(GetLastError()) + L"\n");
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        AppendTextToRichEdit(hRichTextBox, L"The token does not have the specified privilege.\n");
        return FALSE;
    }

    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess != NULL) {
        TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
        if (GetModuleFileNameEx(hProcess, NULL, szProcessName, MAX_PATH)) {
            std::wstring processName(szProcessName);
            size_t pos = processName.find_last_of(L"\\");
            if (pos != std::wstring::npos) {
                processName = processName.substr(pos + 1);
            }

            TCHAR windowTitle[MAX_PATH];
            GetWindowText(hwnd, windowTitle, MAX_PATH);

            if (IsWindowVisible(hwnd) && wcslen(windowTitle) > 0) {
                allProcesses.push_back({ processName, windowTitle, hwnd });
            }

            for (const auto& targetName : *(std::vector<std::wstring>*)lParam) {
                if (processName == targetName) {
                    targetProcesses.push_back({ processName, windowTitle, hwnd });
                }
            }
        }
        CloseHandle(hProcess);
    }
    return TRUE;
}

bool ModifyWindowTitles(const std::wstring& newTitle) {
    bool anyModified = false;
    for (const auto& process : targetProcesses) {
        if (SetWindowText(process.hwnd, newTitle.c_str())) {
            anyModified = true;
            AppendTextToRichEdit(hRichTextBox, L"\nModified window title of " + process.processName + L"\n");
        }
        else {
            AppendTextToRichEdit(hRichTextBox, L"\nFailed to modify window title of " + process.processName + L"\n");
        }
    }
    return anyModified;
}

DWORD WINAPI TitleModifierThread(LPVOID lpParam) {
    std::wstring newTitle = *(std::wstring*)lpParam;
    while (running) {
        ModifyWindowTitles(newTitle);
        Sleep(1000);
    }
    return 0;
}

std::vector<std::wstring> Split(const std::wstring& s, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wstring token;
    std::wistringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void ProcessInput(HWND hwnd) {
    static int step = 0;
    static std::wstring newTitle;
    static std::wstring persistence;

    wchar_t buffer[256];
    GetWindowText(hTextBox, buffer, 256);
    std::wstring input(buffer);

    AppendTextToRichEdit(hRichTextBox, L"> " + input + L"\n");

    switch (step) {
    case 0:
        processNames = Split(input, L';');
        AppendTextToRichEdit(hRichTextBox, L"\nEnter the new window title text: \n");
        step++;
        break;

    case 1:
        newTitle = input;
        AppendTextToRichEdit(hRichTextBox, L"\n\nDo you want persistence (Y/N)? \n");
        step++;
        break;

    case 2:
        persistence = input;
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            AppendTextToRichEdit(hRichTextBox, L"OpenProcessToken error: " + std::to_wstring(GetLastError()) + L"\n");
            return;
        }

        if (!SetPrivilege(hToken, SE_DEBUG_NAME, TRUE)) {
            AppendTextToRichEdit(hRichTextBox, L"Failed to enable debug privilege.\n");
            CloseHandle(hToken);
            return;
        }

        EnumWindows(EnumWindowsProc, (LPARAM)&processNames);

        if (targetProcesses.empty()) {
            AppendTextToRichEdit(hRichTextBox, L"No windows found for the specified processes.\n");
        }
        else {
            if (persistence == L"Y" || persistence == L"y") {
                std::thread titleModifierThread(TitleModifierThread, &newTitle);
                titleModifierThread.detach();

                AppendTextToRichEdit(hRichTextBox, L"Window titles are being modified persistently. Press Enter to stop and exit...\n");
            }
            else {
                ModifyWindowTitles(newTitle);
                AppendTextToRichEdit(hRichTextBox, L"Window titles modified once. Press Enter to exit...\n");
            }
        }

        SetPrivilege(hToken, SE_DEBUG_NAME, FALSE);
        CloseHandle(hToken);
        
        // Reset to the beginning
        step = 0;
        AppendTextToRichEdit(hRichTextBox, L"\nEnter the process names separated by semicolons (e.g., notepad.exe;calc.exe): \n");
        break;
    }

    SetWindowText(hTextBox, L"");
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        g_mainHwnd = hwnd; // Set the global main window handle
        InstallHook(hwnd);
        hRichTextBox = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            10, 10, 372, 200, hwnd, (HMENU)1, NULL, NULL);
        hTextBox = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_MULTILINE,
            10, 220, 280, 20, hwnd, (HMENU)2, NULL, NULL);
        hButton = CreateWindowEx(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            300, 220, 80, 20, hwnd, (HMENU)3, NULL, NULL);
        CreateWindowEx(0, L"BUTTON", L"List", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            300, 250, 80, 20, hwnd, (HMENU)5, NULL, NULL);
        hCloseButton = CreateWindowEx(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 250, 290, 20, hwnd, (HMENU)4, NULL, NULL);
        hLabel = CreateWindowEx(0, L"STATIC", L"2024 (c) Entisoft Software. All Rights Reserved. Evans Thorpemorton.", WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 280, 372, 20, hwnd, (HMENU)6, NULL, NULL);

        AppendTextToRichEdit(hRichTextBox, L"Enter the process names separated by semicolons (e.g., notepad.exe;calc.exe): \n");
        SetTimer(hwnd, 1, 10, NULL);

        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = 100;
        si.nPage = 10;
        si.nPos = 0;

        SetScrollInfo(hRichTextBox, SB_VERT, &si, TRUE);

        // Set focus on the input field
        SetFocus(hTextBox);
        //InstallHook(hwnd);
        break;

    case WM_USER + 1:
        switch (wParam) {
        case VK_ESCAPE:
            //if (g_hListDialog && IsWindow(g_hListDialog)) {
            //    EndDialog(g_hListDialog, 0);
            //}
            //else {                
                DestroyWindow(hwnd);
            //}
            return 0;
        case VK_F1:
            OpenListDialog(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        if (g_hListDialog && IsWindow(g_hListDialog)) {
            EndDialog(g_hListDialog, 0);
            g_hListDialog = NULL;
        DestroyWindow(hwnd);
            return 0; // Prevent the main window from closing if we just closed the dialog
        }
        // Allow the window to close normally if there's no dialog open
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 3) {
            ProcessInput(hwnd);
        }
        else if (LOWORD(wParam) == 4) {
            PostQuitMessage(0);
        }
        /*else if (LOWORD(wParam) == 5) {
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hwnd, DialogProc);
        }*/
        if (LOWORD(wParam) == 5) { // ID for opening the list dialog
            OpenListDialog(hwnd);
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == 1) {
            if (GetFocus() == hTextBox) {
                if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
                    static DWORD lastEnterTime = 0;
                    DWORD currentTime = GetTickCount();
                    if (currentTime - lastEnterTime > 500) {
                        ProcessInput(hwnd);
                        lastEnterTime = currentTime;
                    }
                    return 0;
                }
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState('A') & 0x8000)) {
                    SendMessage(hTextBox, EM_SETSEL, 0, -1);
                    return 0;
                }
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_BACK) & 0x8000)) {
                    static DWORD lastCtrlBackspaceTime = 0;
                    DWORD currentTime = GetTickCount();
                    if (currentTime - lastCtrlBackspaceTime > 100) {
                        DeleteWordToLeft(hTextBox);
                        lastCtrlBackspaceTime = currentTime;
                    }
                    return 0;
                }
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState('C') & 0x8000)) {
                    static DWORD lastCtrlCTime = 0;
                    DWORD currentTime = GetTickCount();
                    if (currentTime - lastCtrlCTime > 500) {
                        SendMessage(hTextBox, WM_COPY, 0, 0);
                        lastCtrlCTime = currentTime;
                    }
                    return 0;
                }
            }
        }
        if (wParam == 2) {
            KillTimer(hwnd, 2);
            isPasting = false;
        }
        else if (wParam == 3) {
            KillTimer(hwnd, 3);
            isCopying = false;
        }
        break;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            PostQuitMessage(0); // Exit the application
            return 0;
        case VK_F1:
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hwnd, DialogProc); // Open List window
            return 0;
        }
        break;
        //break;
        if (GetFocus() == hTextBox) {
            if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                if (!isPasting) {
                    isPasting = true;

                    // Store current text
                    int textLength = GetWindowTextLength(hTextBox);
                    std::wstring currentText(textLength + 1, L'\0');
                    GetWindowText(hTextBox, &currentText[0], textLength + 1);

                    // Perform paste
                    SendMessage(hTextBox, WM_PASTE, 0, 0);

                    // Get new text
                    int newTextLength = GetWindowTextLength(hTextBox);
                    std::wstring newText(newTextLength + 1, L'\0');
                    GetWindowText(hTextBox, &newText[0], newTextLength + 1);

                    // If text didn't change, revert
                    if (newText == currentText) {
                        SetWindowText(hTextBox, currentText.c_str());
                    }

                    // Set a timer to reset isPasting
                    SetTimer(hwnd, 2, 100, NULL);
                }
                return 0;
            }
            else if (wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                if (!isCopying) {
                    isCopying = true;
                    SendMessage(hTextBox, WM_COPY, 0, 0);
                    SetTimer(hwnd, 3, 100, NULL);
                }
                return 0;
            }
            break;
        }

    case WM_KEYUP:
        if (wParam == 'V' || wParam == 'C' || wParam == VK_CONTROL) {
            isPasting = false;
            isCopying = false;
        }
        break;

    case WM_GETDLGCODE:
        if (wParam == VK_ESCAPE) {
            return DLGC_WANTALLKEYS;
        }
        if (wParam == VK_F1) {
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
        }
        break;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 400; // Set your desired minimum width
        mmi->ptMinTrackSize.y = 350; // Set your desired minimum height
        mmi->ptMaxTrackSize.x = 400; // Set your desired maximum width
        mmi->ptMaxTrackSize.y = 350; // Set your desired maximum height
        return 0;
    }

    case WM_DESTROY:
        UninstallHook();
        if (g_hListDialog) {
            EndDialog(g_hListDialog, 0); // Ensure the dialog is closed on exit
        }
        running = false;
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        MoveWindow(hLabel, 10, HIWORD(lParam) - 20, 372, 20, TRUE);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

/*
void OpenListDialog(HWND parentHwnd) {
    g_hListDialog = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), parentHwnd, DialogProc);
    if (g_hListDialog != NULL) {
        ShowWindow(g_hListDialog, SW_SHOW);
    }
}*/

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {    
    switch (message) {
    case WM_INITDIALOG:
    {
        // Center the dialog on the screen
        RECT rc, rcDlg, rcOwner;
        GetWindowRect(GetDesktopWindow(), &rc);
        GetWindowRect(hDlg, &rcDlg);
        int x = (rc.right - rc.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = (rc.bottom - rc.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        hListView = GetDlgItem(hDlg, IDC_LIST1);
        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMN lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        lvc.iSubItem = 0;
        lvc.pszText = (LPWSTR)L"Icon";
        lvc.cx = 50;
        ListView_InsertColumn(hListView, 0, &lvc);

        lvc.iSubItem = 1;
        lvc.pszText = (LPWSTR)L"Window Title";
        lvc.cx = 200;
        ListView_InsertColumn(hListView, 1, &lvc);

        lvc.iSubItem = 2;
        lvc.pszText = (LPWSTR)L"Process Name";
        lvc.cx = 150;
        ListView_InsertColumn(hListView, 2, &lvc);

        allProcesses.clear();
        EnumWindows(EnumWindowsProc, (LPARAM)&processNames);

        for (const auto& process : allProcesses) {
            LVITEM lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_IMAGE;
            lvi.iItem = ListView_GetItemCount(hListView);

            // Get the icon for the process
            HICON hIcon = NULL;
            SHFILEINFO sfi = { 0 };
            if (SHGetFileInfo(process.processName.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON)) {
                hIcon = sfi.hIcon;
            }

            // Create an image list if it doesn't exist
            HIMAGELIST hImageList = ListView_GetImageList(hListView, LVSIL_SMALL);
            if (!hImageList) {
                hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 1, 1);
                ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
            }

            // Add the icon to the image list
            int imageIndex = -1;
            if (hIcon) {
                imageIndex = ImageList_AddIcon(hImageList, hIcon);
                DestroyIcon(hIcon);
            }

            lvi.iImage = imageIndex;
            ListView_InsertItem(hListView, &lvi);

            ListView_SetItemText(hListView, lvi.iItem, 1, (LPWSTR)process.windowTitle.c_str());
            ListView_SetItemText(hListView, lvi.iItem, 2, (LPWSTR)process.processName.c_str());
        }

        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            g_hListDialog = NULL; // Reset when dialog closes
            return (INT_PTR)TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        g_hListDialog = NULL; // Reset when dialog closes
        ResetFocusAndHookState(); // Reset focus or hook state here
        return (INT_PTR)TRUE;
    //}
    //return (INT_PTR)FALSE;
//}

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            EndDialog(hDlg, 0); // Close the dialog when Escape is pressed
            return (INT_PTR)TRUE;
        }
        break;

    case WM_GETDLGCODE:
        if (wParam == VK_ESCAPE) {
            return DLGC_WANTALLKEYS;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void DeleteWordToLeft(HWND hWnd) {
    DWORD start, end;
    SendMessage(hWnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    if (start == 0) return;

    std::wstring text(GetWindowTextLength(hWnd), L'\0');
    GetWindowText(hWnd, &text[0], text.size() + 1);
    text.resize(text.size() - 1);

    size_t pos = start - 1;
    while (pos > 0 && iswspace(text[pos])) {
        --pos;
    }
    while (pos > 0 && !iswspace(text[pos])) {
        --pos;
    }

    text.erase(pos, start - pos);
    SetWindowText(hWnd, text.c_str());
    SendMessage(hWnd, EM_SETSEL, pos, pos);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"WindowTitleModifierClass";
    wcex.hIconSm = (HICON)LoadImage(wcex.hInstance,
        MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON,
        16,    // Small icon width
        16,    // Small icon height
        LR_DEFAULTCOLOR);
    //wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Get the screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Set window dimensions
    int windowWidth = 400;
    int windowHeight = 350;

    // Calculate the position for the centered window
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hWnd = CreateWindowEx(
        //WS_EX_CLIENTEDGE,
        WS_EX_NOMAXIMIZEBOX,
        L"WindowTitleModifierClass",
        L"Window Title Modifier (F1=ProcessList)",
        //WS_OVERLAPPEDWINDOW,
        //WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL);

    if (hWnd == NULL) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}