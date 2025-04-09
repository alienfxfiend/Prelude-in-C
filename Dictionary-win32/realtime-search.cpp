#include <Windows.h>
#include <CommCtrl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include "resource2.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")

using namespace std;

// Structure to represent a dictionary entry
struct DictionaryEntry {
    wstring word;
    wstring definition;
};

// Structure to represent a node in the linked list
struct Node {
    DictionaryEntry data;
    Node* next;
};

Node* dictionary = nullptr;
vector<wstring> words;
WNDPROC oldWordEditProc;
WNDPROC oldDefinitionEditProc;
HHOOK g_hHook = NULL;
HWND g_hDlg = NULL;
HWND g_hDefinitionDlg = NULL; // Add this near where you declare g_hDlg

// Function prototypes
//LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ViewDefinitionDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void AddWordToList(HWND hwnd);
void insertNode(Node*& head, const DictionaryEntry& entry);
//void addWord(Node*& head, const wstring& word, const wstring& definition);
bool addWord(Node*& head, const wstring& word, const wstring& definition);
void deleteWord(Node*& head, const wstring& keyword);
wstring InputBox(HWND hwnd, const wstring& prompt, const wstring& title);
void DisplayDefinition(HWND hwnd, const wstring& word, const wstring& definition);
void editWord(Node* head, const wstring& oldWord, const wstring& newWord, const wstring& newDefinition);
void listWords(const Node* head, vector<wstring>& words);
void serializeDictionary(const Node* head, const wstring& filename);
void deserializeDictionary(Node*& head, const wstring& filename);
void searchWordRealtime(const Node* head, const wstring& keyword, vector<wstring>& suggestions);
int pruneDictionary(Node*& head);
void updateDictionaryAfterPrune(Node*& head, vector<wstring>& words, HWND hwnd);
void updateListBox(HWND hwnd, const vector<wstring>& words);

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN) {
            switch (pKbdStruct->vkCode) {
            case VK_RETURN:
                if (GetFocus() == GetDlgItem(g_hDlg, IDC_WORD_INPUT) ||
                    GetFocus() == GetDlgItem(g_hDlg, IDC_DEFINITION_INPUT)) {
                    AddWordToList(g_hDlg);
                    return 1;
                }
                break;
            case VK_ESCAPE:
                if (GetDlgItem(g_hDlg, IDCANCEL)) {
                    SendMessage(g_hDlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
                    return TRUE;
                }
                else if (GetDlgItem(g_hDlg, IDNO)) {
                    SendMessage(g_hDlg, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
                    return TRUE;
                }
                else {
                    SendMessage(g_hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }
                break;
            case VK_F1:
            {
                if (g_hDefinitionDlg && IsWindowVisible(g_hDefinitionDlg)) {
                    // If the definition window is visible, hide it
                    ShowWindow(g_hDefinitionDlg, SW_HIDE);
                }
                else {
                    // If the definition window is not visible or doesn't exist, show/create it
                    HWND hListBox = GetDlgItem(g_hDlg, IDC_LISTBOX);
                    int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                    if (selectedIndex != LB_ERR) {
                        wchar_t selectedWord[256];
                        SendMessage(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)selectedWord);
                        Node* current = dictionary;
                        while (current) {
                            if (current->data.word == selectedWord) {
                                wstring message = L"Definition of '" + current->data.word + L"': \n\n" + current->data.definition;
                                if (g_hDefinitionDlg) {
                                    // Update existing dialog
                                    SetDlgItemText(g_hDefinitionDlg, IDC_DEFINITION_EDIT, message.c_str());
                                    ShowWindow(g_hDefinitionDlg, SW_SHOW);
                                }
                                else {
                                    // Create new dialog
                                    g_hDefinitionDlg = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DEFINITION), g_hDlg, ViewDefinitionDlgProc, (LPARAM)new wstring(message));
                                    ShowWindow(g_hDefinitionDlg, SW_SHOW);
                                }
                                break;
                            }
                            current = current->next;
                        }
                    }
                }
                return 1;
            }
            /* {
                if (g_hDefinitionDlg) {
                    if (IsWindowVisible(g_hDefinitionDlg)) {
                        ShowWindow(g_hDefinitionDlg, SW_HIDE);
                    }
                    else {
                        ShowWindow(g_hDefinitionDlg, SW_SHOW);
                    }
                }
                else {
                    HWND hListBox = GetDlgItem(g_hDlg, IDC_LISTBOX);
                    int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                    if (selectedIndex != LB_ERR) {
                        wchar_t selectedWord[256];
                        SendMessage(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)selectedWord);
                        Node* current = dictionary;
                        while (current) {
                            if (current->data.word == selectedWord) {
                                g_hDefinitionDlg = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DEFINITION), g_hDlg, ViewDefinitionDlgProc, (LPARAM)new wstring(current->data.definition));
                                ShowWindow(g_hDefinitionDlg, SW_SHOW);
                                break;
                            }
                            current = current->next;
                        }
                    }
                }
                return 1;
            }*/
            case VK_F2:
            {
                // F2: Simulate the "Edit Word" command for the currently selected item.
                HWND hListBox = GetDlgItem(g_hDlg, IDC_LISTBOX);
                int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
                if (selectedIndex != LB_ERR) {
                    // Send WM_COMMAND message to simulate clicking the "Edit Word" button.
                    SendMessage(g_hDlg, WM_COMMAND, MAKEWPARAM(IDC_EDIT_WORD, BN_CLICKED), 0);
                }
                return 1;
            }
            case VK_F9:
            {
                MessageBox(g_hDlg, L"Advanced LinkedList Dictionary using SubClassing & LowLevelHookFunction. Made in C++ GUI-based Win32 1148+ Lines of code! Copyright (c) 2025 Evans Thorpemorton", L"About Info", MB_OK | MB_ICONINFORMATION);
                return 1;
            }
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/*INT_PTR CALLBACK ViewDefinitionDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        wstring* definition = (wstring*)lParam;
        SetDlgItemText(hDlg, IDC_DEFINITION_EDIT, definition->c_str());
        delete definition;
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}*/
INT_PTR CALLBACK ViewDefinitionDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        wstring* fullMessage = (wstring*)lParam;
        SetDlgItemText(hDlg, IDC_DEFINITION_EDIT, fullMessage->c_str());
        delete fullMessage;
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDCANCEL) {
            ShowWindow(hDlg, SW_HIDE);
            return (INT_PTR)TRUE;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hDlg, SW_HIDE);
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

// Function implementations
void insertNode(Node*& head, const DictionaryEntry& entry) {
    Node* newNode = new Node{ entry, nullptr };
    if (!head) {
        head = newNode;
    }
    else {
        Node* current = head;
        while (current->next) {
            current = current->next;
        }
        current->next = newNode;
    }
}

bool addWord(Node*& head, const wstring& word, const wstring& definition) {
    // Check if the word already exists
    Node* current = head;
    while (current) {
        if (current->data.word == word) {
            return false; // Word already exists, don't add
        }
        current = current->next;
    }

    // Word doesn't exist, add it
    insertNode(head, { word, definition });
    return true;
}
// w/o duplicates
/* // Modify the addWord function to check for duplicates
bool addWord(Node*& head, const wstring& word, const wstring& definition) {
    // Check if the word already exists
    Node* current = head;
    while (current) {
        if (current->data.word == word) {
            return false; // Word already exists, don't add
        }
        current = current->next;
    }

    // Word doesn't exist, add it
    insertNode(head, { word, definition });
    return true;
}*/
// /w duplicates
/*void addWord(Node*& head, const wstring& word, const wstring& definition) {
    insertNode(head, { word, definition });
}*/

void deleteWord(Node*& head, const wstring& keyword) {
    if (!head) return;

    if (head->data.word == keyword) {
        Node* temp = head;
        head = head->next;
        delete temp;
        return;
    }

    Node* current = head;
    while (current->next && current->next->data.word != keyword) {
        current = current->next;
    }

    if (current->next) {
        Node* temp = current->next;
        current->next = current->next->next;
        delete temp;
    }
}

// Modify the InputBox function to pre-populate the input
wstring InputBox(HWND hwnd, const wstring& prompt, const wstring& title, const wstring& defaultValue = L"") {
    wstring input = defaultValue;
    DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INPUT), hwnd,
        [](HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) -> INT_PTR {
            static wstring* pInput;
            switch (message) {
            case WM_INITDIALOG:
                pInput = (wstring*)lParam;
                SetDlgItemText(hDlg, IDC_INPUT, pInput->c_str());
                SendDlgItemMessage(hDlg, IDC_INPUT, EM_SETSEL, 0, -1);  // Select all text
                return (INT_PTR)TRUE;
            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK) {
                    HWND hEdit = GetDlgItem(hDlg, IDC_INPUT);
                    int len = GetWindowTextLength(hEdit) + 1;
                    wchar_t* buffer = new wchar_t[len];
                    GetWindowText(hEdit, buffer, len);
                    *pInput = buffer;
                    delete[] buffer;
                    EndDialog(hDlg, IDOK);
                    return (INT_PTR)TRUE;
                }
                else if (LOWORD(wParam) == IDCANCEL) {
                    EndDialog(hDlg, IDCANCEL);
                    return (INT_PTR)TRUE;
                }
                break;
            }
            return (INT_PTR)FALSE;
        }, (LPARAM)&input);
    return input;
}
/*wstring InputBox(HWND hwnd, const wstring& prompt, const wstring& title) {
    wchar_t input[156] = L"";
    if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INPUT), hwnd,
        [](HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) -> INT_PTR {
            static wchar_t input[156];
            switch (message) {
            case WM_INITDIALOG: {
                HWND hEdit = GetDlgItem(hDlg, IDC_INPUT);
                SetWindowText(hEdit, (LPCWSTR)lParam);
                SendMessage(hEdit, EM_SETSEL, 0, -1); // Select all text
                break;
            }
            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK) {
                    GetDlgItemText(hDlg, IDC_INPUT, input, 156);
                    EndDialog(hDlg, IDOK);
                    return (INT_PTR)TRUE;
                }
                else if (LOWORD(wParam) == IDCANCEL) {
                    EndDialog(hDlg, IDCANCEL);
                    return (INT_PTR)TRUE;
                }
                break;
            }
            return (INT_PTR)FALSE;
        }, (LPARAM)prompt.c_str()) == IDOK) {
        return (wstring)input;
    }
    return L"";
}*/

void DisplayDefinition(HWND hwnd, const wstring& word, const wstring& definition) {
    wstring message = L"Definition of '" + word + L"': \n\n" + definition;
    HWND hDlg = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DEFINITION), hwnd, [](HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) -> INT_PTR {
        switch (message) {
        case WM_INITDIALOG:
        {
            wstring* definition = (wstring*)lParam;
            SetDlgItemText(hDlg, IDC_DEFINITION_EDIT, definition->c_str());
            delete definition;
        }
        return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(hDlg, 0);
            return (INT_PTR)TRUE;
        }
        return (INT_PTR)FALSE;
        }, (LPARAM)new wstring(message));
    ShowWindow(hDlg, SW_SHOW);
}
/*void DisplayDefinition(HWND hwnd, const wstring& word, const wstring& definition) {
    wstring message = L"Definition of '" + word + L"':\n\n" + definition;
    MessageBox(hwnd, message.c_str(), L"Word Definition", MB_OK | MB_ICONINFORMATION);
}*/

void editWord(Node* head, const wstring& oldWord, const wstring& newWord, const wstring& newDefinition) {
    Node* current = head;
    while (current) {
        if (current->data.word == oldWord) {
            current->data.word = newWord;
            current->data.definition = newDefinition;
            return;
        }
        current = current->next;
    }
}

void listWords(const Node* head, vector<wstring>& words) {
    words.clear();
    const Node* current = head;
    while (current) {
        words.push_back(current->data.word);
        current = current->next;
    }
}

// Delimiter Old Semicolon
void serializeDictionary(const Node* head, const wstring& filename) {
    wofstream file(filename, ios::out | ios::trunc);
    if (!file.is_open()) {
        MessageBox(NULL, L"Failed to open file for writing", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    const Node* current = head;
    while (current) {
        file << current->data.word << L";" << current->data.definition << endl;
        current = current->next;
    }
    file.close();
}

// Delimiter Old Semicolon
void deserializeDictionary(Node*& head, const wstring& filename) {
    wifstream file(filename);
    if (!file.is_open()) {
        MessageBox(NULL, L"Failed to open dictionary file", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    wstring line;
    while (getline(file, line)) {
        size_t pos = line.find(L';');
        if (pos != wstring::npos) {
            wstring word = line.substr(0, pos);
            wstring definition = line.substr(pos + 1);
            addWord(head, word, definition);
        }
    }
    file.close();
}

// Add this function to convert a string to lowercase
wstring toLower(const wstring& str) {
    wstring lower = str;
    transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    return lower;
}

// Modify the searchWordRealtime function to be case-insensitive
void searchWordRealtime(const Node* head, const wstring& keyword, vector<wstring>& suggestions) {
    suggestions.clear();
    const Node* current = head;
    wstring lowercaseKeyword = toLower(keyword);
    while (current) {
        wstring lowercaseWord = toLower(current->data.word);
        if (lowercaseWord.find(lowercaseKeyword) != wstring::npos) {
            suggestions.push_back(current->data.word);
        }
        current = current->next;
    }
}

// Prune Functions
int pruneDictionary(Node*& head) {
    if (!head) return 0;

    int duplicatesRemoved = 0;
    Node* current = head;

    while (current && current->next) {
        Node* runner = current;
        while (runner->next) {
            if (current->data.word == runner->next->data.word) {
                Node* duplicate = runner->next;
                runner->next = runner->next->next;
                delete duplicate;
                duplicatesRemoved++;
            }
            else {
                runner = runner->next;
            }
        }
        current = current->next;
    }

    return duplicatesRemoved;
}

// Prune Functions
void updateDictionaryAfterPrune(Node*& head, vector<wstring>& words, HWND hwnd) {
    listWords(head, words);
    updateListBox(hwnd, words);
    serializeDictionary(head, L"myDictionary99x.txt");
}

void updateListBox(HWND hwnd, const vector<wstring>& words) {
    HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    for (const auto& word : words) {
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)word.c_str());
    }
}

void AddWordToList(HWND hwnd) {
    wchar_t word[256], definition[1024];
    GetDlgItemText(hwnd, IDC_WORD_INPUT, word, 256);
    GetDlgItemText(hwnd, IDC_DEFINITION_INPUT, definition, 1024);
    if (wcslen(word) > 0) {
        if (!addWord(dictionary, word, definition)) {
            wstring message = L"An entry with the name '";
            message += word;
            message += L"' already exists. Do you want to add a duplicate entry?";
            int result = MessageBox(hwnd, message.c_str(), L"Confirm Duplicate Entry", MB_YESNO | MB_ICONQUESTION);

            if (result == IDYES) {
                insertNode(dictionary, { word, definition });
            }
        }
        listWords(dictionary, words);
        updateListBox(hwnd, words);
        serializeDictionary(dictionary, L"myDictionary99x.txt");
        SetDlgItemText(hwnd, IDC_WORD_INPUT, L"");
        SetDlgItemText(hwnd, IDC_DEFINITION_INPUT, L"");
    }
}

/*LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN && pKbdStruct->vkCode == VK_RETURN) {
            HWND hFocus = GetFocus();
            if (hFocus == GetDlgItem(g_hDlg, IDC_WORD_INPUT) ||
                hFocus == GetDlgItem(g_hDlg, IDC_DEFINITION_INPUT)) {
                AddWordToList(g_hDlg);
                return 1;  // Prevent further processing of this key
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}*/


//Return this function
/*LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN) {
            switch (pKbdStruct->vkCode) {
            case VK_RETURN:
                if (GetFocus() == GetDlgItem(g_hDlg, IDC_WORD_INPUT) ||
                    GetFocus() == GetDlgItem(g_hDlg, IDC_DEFINITION_INPUT)) {
                    AddWordToList(g_hDlg);
                    return 1;
                }
                break;
            case VK_ESCAPE:
                /*if (IsWindowVisible(g_hDlg)) { // Assuming g_hDlg is your dialog handle
                    PostMessage(g_hDlg, WM_COMMAND, IDCANCEL, 0);
                    return 1; // Signal that we've handled this key
                }
                // Check for Cancel button                
                if (GetDlgItem(g_hDlg, IDCANCEL)) {
                    SendMessage(g_hDlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
                    return TRUE;
                }
                // Check for No button
                else if (GetDlgItem(g_hDlg, IDNO)) {
                    SendMessage(g_hDlg, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
                    return TRUE;
                }
                // For dialogs with only a close button (like View Definition)
                else {
                    SendMessage(g_hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }
                break;
            case VK_F1:
            {
    if (g_hDefinitionDlg && IsWindowVisible(g_hDefinitionDlg)) {
        // If the dialog is visible, hide it
        ShowWindow(g_hDefinitionDlg, SW_HIDE);
        g_hDefinitionDlg = NULL; // Reset since we're hiding it
    } else {
        // If not visible or doesn't exist, show it or create it
        HWND hListBox = GetDlgItem(g_hDlg, IDC_LISTBOX);
        int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
        if (selectedIndex != LB_ERR) {
            wchar_t selectedWord[256];
            SendMessage(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)selectedWord);
            Node* current = dictionary;
            while (current) {
                if (current->data.word == selectedWord) {
                    // Here, instead of calling DisplayDefinition directly, we'll manage the dialog
                    if (!g_hDefinitionDlg) {
                        g_hDefinitionDlg = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DEFINITION), g_hDlg, ViewDefinitionDlgProc, (LPARAM)new wstring(current->data.definition));
                        ShowWindow(g_hDefinitionDlg, SW_SHOW);
                    }
                    break;
                }
                current = current->next;
            }
        }
    }
    return 1;
}*/

/*LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        HWND hwndParent = (HWND)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if (hwndParent)
        {
            HWND hwndAddButton = GetDlgItem(hwndParent, IDC_ADD_WORD);
            if (hwndAddButton)
            {
                SendMessage(hwndParent, WM_COMMAND, MAKEWPARAM(IDC_ADD_WORD, BN_CLICKED), (LPARAM)hwndAddButton);
                return 0;
            }
        }
    }

    return CallWindowProc((WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA), hWnd, uMsg, wParam, lParam);
}*/

/*LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND hwndParent = (HWND)dwRefData;
        SendMessage(hwndParent, WM_COMMAND, MAKEWPARAM(IDC_ADD_WORD, BN_CLICKED),
            (LPARAM)GetDlgItem(hwndParent, IDC_ADD_WORD));
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}*/
/*LRESULT CALLBACK WordEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDC_ADD_WORD, BN_CLICKED), (LPARAM)GetDlgItem(GetParent(hWnd), IDC_ADD_WORD));
        return 0;
    }
    return CallWindowProc(oldWordEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK DefinitionEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDC_ADD_WORD, BN_CLICKED), (LPARAM)GetDlgItem(GetParent(hWnd), IDC_ADD_WORD));
        return 0;
    }
    return CallWindowProc(oldDefinitionEditProc, hWnd, uMsg, wParam, lParam);
}*/            

// Dialog procedure
// Modify the DialogProc function
INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hFocusedEdit = nullptr; // Define hFocusedEdit here
    static HWND hWordEdit = nullptr;
    static HWND hDefinitionEdit = nullptr;
    switch (uMsg) {
    case WM_INITDIALOG:
    {
        // Load dictionary
        /*wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        wstring exePath(path);
        wstring dirPath = exePath.substr(0, exePath.find_last_of(L"\\/"));
        wstring filePath = dirPath + L"\\myDictionary99x.txt";*/
        // Set both large and small icons
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));

        deserializeDictionary(dictionary, L"myDictionary99x.txt");
        listWords(dictionary, words);
        updateListBox(hwnd, words);
        g_hDlg = hwnd;  // Store the dialog handle globally
        g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
        //hWordEdit = GetDlgItem(hwnd, IDC_WORD_INPUT);
        //hDefinitionEdit = GetDlgItem(hwnd, IDC_DEFINITION_INPUT);

        // Set up subclassing
        /*SetWindowLongPtr(hWordEdit, GWLP_USERDATA, (LONG_PTR)hwnd);
        SetWindowLongPtr(hDefinitionEdit, GWLP_USERDATA, (LONG_PTR)hwnd);
        SetWindowLongPtr(hWordEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        SetWindowLongPtr(hDefinitionEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);*/
        SetFocus(GetDlgItem(hwnd, IDC_SEARCH_INPUT));  // Set focus to search field
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1)));
        return TRUE;
    }

    case WM_COMMAND:
        // Start Double-Click View ListBox Definitions function block
        switch (HIWORD(wParam)) {
        case LBN_DBLCLK:
            if (LOWORD(wParam) == IDC_LISTBOX) {
                int index = SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    wchar_t word[256];
                    SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETTEXT, index, (LPARAM)word);

                    // Find the definition for the selected word
                    Node* current = dictionary;
                    while (current) {
                        if (current->data.word == word) {
                            DisplayDefinition(hwnd, word, current->data.definition);
                            break;
                        }
                        current = current->next;
                    }
                }
                return TRUE;
            }
            //break;
        //}

        case EN_SETFOCUS:
            if (LOWORD(wParam) == IDC_WORD_INPUT || LOWORD(wParam) == IDC_DEFINITION_INPUT) {
                hFocusedEdit = (HWND)lParam;
            }
            break;
        }

        // End Double-Click View ListBox Definitions function block
        // Handle other controls
        switch (LOWORD(wParam)) {
        case IDC_ADD_WORD: {
            if (LOWORD(wParam) == IDC_ADD_WORD) {
                AddWordToList(hwnd);
                return TRUE;
            }
            else if ((LOWORD(wParam) == IDC_WORD_INPUT || LOWORD(wParam) == IDC_DEFINITION_INPUT) &&
                HIWORD(wParam) == EN_SETFOCUS) {
                SetTimer(hwnd, 1, 10, NULL);
                return TRUE;
            }
            else if ((LOWORD(wParam) == IDC_WORD_INPUT || LOWORD(wParam) == IDC_DEFINITION_INPUT) &&
                HIWORD(wParam) == EN_KILLFOCUS) {
                KillTimer(hwnd, 1);
                return TRUE;
            }
            break;
        }
                         /*wchar_t word[256], definition[1024];
                         GetDlgItemText(hwnd, IDC_WORD_INPUT, word, 256);
                         GetDlgItemText(hwnd, IDC_DEFINITION_INPUT, definition, 1024);
                         if (wcslen(word) > 0) {
                             if (!addWord(dictionary, word, definition)) {
                                 // Word already exists, show confirmation box
                                 wstring message = L"An entry with the name '";
                                 message += word;
                                 message += L"' already exists. Do you want to add a duplicate entry?";
                                 int result = MessageBox(hwnd, message.c_str(), L"Confirm Duplicate Entry", MB_YESNO | MB_ICONQUESTION);

                                 if (result == IDYES) {
                                     // User confirmed, add the duplicate entry
                                     insertNode(dictionary, { word, definition });
                                     listWords(dictionary, words);
                                     updateListBox(hwnd, words);
                                     serializeDictionary(dictionary, L"myDictionary99x.txt");
                                     SetDlgItemText(hwnd, IDC_WORD_INPUT, L"");
                                     SetDlgItemText(hwnd, IDC_DEFINITION_INPUT, L"");
                                 }
                                 // If result is IDNO, do nothing (silently ignore)
                             }
                             else {
                                 // Word added successfully
                                 listWords(dictionary, words);
                                 updateListBox(hwnd, words);
                                 serializeDictionary(dictionary, L"myDictionary99x.txt");
                                 SetDlgItemText(hwnd, IDC_WORD_INPUT, L"");
                                 SetDlgItemText(hwnd, IDC_DEFINITION_INPUT, L"");
                             }
                         }
                         return TRUE;
                     }
                 }*/
                 // w/o duplicates
                 /*case IDC_ADD_WORD: {
                     wchar_t word[256], definition[1024];
                     GetDlgItemText(hwnd, IDC_WORD_INPUT, word, 256);
                     GetDlgItemText(hwnd, IDC_DEFINITION_INPUT, definition, 1024);
                     if (wcslen(word) > 0) {
                         if (addWord(dictionary, word, definition)) {
                             listWords(dictionary, words);
                             updateListBox(hwnd, words);
                             serializeDictionary(dictionary, L"myDictionary99x.txt");
                             SetDlgItemText(hwnd, IDC_WORD_INPUT, L"");
                             SetDlgItemText(hwnd, IDC_DEFINITION_INPUT, L"");
                         }
                         // If addWord returns false, it means the word already exists.
                         // We silently ignore it as per the requirement.
                     }
                     return TRUE;
                 }*/
                 // /w duplicates        
                 /*case IDC_ADD_WORD: {
                     wchar_t word[256], definition[1024];
                     GetDlgItemText(hwnd, IDC_WORD_INPUT, word, 256);
                     GetDlgItemText(hwnd, IDC_DEFINITION_INPUT, definition, 1024);
                     if (wcslen(word) > 0) {  // Only check if word is not empty
                         addWord(dictionary, word, definition);
                         listWords(dictionary, words);
                         updateListBox(hwnd, words);
                         serializeDictionary(dictionary, L"myDictionary99x.txt");
                         SetDlgItemText(hwnd, IDC_WORD_INPUT, L"");
                         SetDlgItemText(hwnd, IDC_DEFINITION_INPUT, L"");
                     }
                     return TRUE;
                 }*/

        case IDC_DELETE_WORD: {
            int index = SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
            if (index != LB_ERR) {
                wchar_t word[256];
                SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETTEXT, index, (LPARAM)word);

                // Create the confirmation message
                wstring message = L"Are you sure you want to delete the word '";
                message += word;
                message += L"'?";

                // Show the confirmation box
                int result = MessageBox(hwnd, message.c_str(), L"Confirm Deletion", MB_YESNO | MB_ICONQUESTION);

                // If the user clicks "Yes", proceed with deletion
                if (result == IDYES) {
                    deleteWord(dictionary, word);
                    listWords(dictionary, words);
                    updateListBox(hwnd, words);
                    serializeDictionary(dictionary, L"myDictionary99x.txt");
                }
            }
            return TRUE;
        }

        case IDC_EDIT_WORD: {
            int index = SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
            if (index != LB_ERR) {
                wchar_t wordToEdit[256];
                SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETTEXT, index, (LPARAM)wordToEdit);
                Node* current = dictionary;
                while (current) {
                    if (current->data.word == wordToEdit) {
                        wstring newWord = InputBox(hwnd, L"Edit Word", L"Edit Word", current->data.word);
                        if (!newWord.empty()) {
                            wstring newDefinition = InputBox(hwnd, L"Edit Definition", L"Edit Definition", current->data.definition);
                            // newDefinition can be empty now
                            SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_DELETESTRING, index, 0); // Delete the old word
                            editWord(dictionary, wordToEdit, newWord, newDefinition);
                            listWords(dictionary, words);
                            updateListBox(hwnd, words);
                            // fix for C++ best practices "<" Signed/ Unsigned Mismatch
                            for (size_t i = 0; i < words.size(); i++) {
                                //for (int i = 0; i < words.size(); i++) {
                                if (words[i] == newWord) {
                                    SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_SETCURSEL, static_cast<WPARAM>(i), 0); // Select the edited word
                                    // fix for C++ best practices "<" Signed Unsigned Mismatch
                                    //SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_SETCURSEL, i, 0); // Select the edited word
                                    break;
                                }
                            }
                            serializeDictionary(dictionary, L"myDictionary99x.txt");
                        }
                        break;
                    }
                    current = current->next;
                }
                if (!current) {
                    MessageBox(hwnd, L"Word not found in the dictionary.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            return TRUE;
        }
                          // /w duplicates (does not affect duplicates tho only no check for blank definitions
                          /*case IDC_EDIT_WORD: {
                              int index = SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
                              if (index != LB_ERR) {
                                  wchar_t wordToEdit[256];
                                  SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETTEXT, index, (LPARAM)wordToEdit);
                                  Node* current = dictionary;
                                  while (current) {
                                      if (current->data.word == wordToEdit) {
                                          wstring newWord = InputBox(hwnd, L"Edit Word", L"Edit Word", current->data.word);
                                          if (!newWord.empty()) {
                                              wstring newDefinition = InputBox(hwnd, L"Edit Definition", L"Edit Definition", current->data.definition);
                                              SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_DELETESTRING, index, 0); // Delete the old word
                                              editWord(dictionary, wordToEdit, newWord, newDefinition);
                                              listWords(dictionary, words);
                                              updateListBox(hwnd, words);
                                              for (int i = 0; i < words.size(); i++) {
                                                  if (words[i] == newWord) {
                                                      SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_SETCURSEL, i, 0); // Select the edited word
                                                      break;
                                                  }
                                              }
                                              serializeDictionary(dictionary, L"myDictionary99x.txt");
                                          }
                                          break;
                                      }
                                      current = current->next;
                                  }
                                  if (!current) {
                                      MessageBox(hwnd, L"Word not found in the dictionary.", L"Error", MB_OK | MB_ICONERROR);
                                  }
                              }
                              return TRUE;
                          }*/

        case IDC_VIEW_DEFINITION: {
            int index = SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETCURSEL, 0, 0);
            if (index != LB_ERR) {
                wchar_t word[256];
                SendDlgItemMessage(hwnd, IDC_LISTBOX, LB_GETTEXT, index, (LPARAM)word);
                Node* current = dictionary;
                while (current) {
                    if (current->data.word == word) {
                        DisplayDefinition(hwnd, current->data.word, current->data.definition);
                        break;
                    }
                    current = current->next;
                }
            }
            return TRUE;
        }

        case IDC_SEARCH_INPUT: {
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t query[256];
                GetDlgItemText(hwnd, IDC_SEARCH_INPUT, query, 256);
                if (wcslen(query) == 0) {
                    updateListBox(hwnd, words);
                }
                else {
                    vector<wstring> suggestions;
                    searchWordRealtime(dictionary, query, suggestions);
                    updateListBox(hwnd, suggestions);
                }
            }
            return TRUE;
        }

        case IDC_CLEAR_SEARCH: {
            SetDlgItemText(hwnd, IDC_SEARCH_INPUT, L"");
            updateListBox(hwnd, words);
            return TRUE;
        }

                             //Prune Functions
        case IDC_PRUNE: {
            int result = MessageBox(hwnd, L"Are you sure you want to prune the list and remove duplicates?", L"Confirm Prune", MB_YESNO | MB_ICONQUESTION);
            if (result == IDYES) {
                int duplicatesRemoved = pruneDictionary(dictionary);
                updateDictionaryAfterPrune(dictionary, words, hwnd);

                wstring message = L"Pruning complete. ";
                message += to_wstring(duplicatesRemoved);
                message += L" duplicate entries were removed.";
                MessageBox(hwnd, message.c_str(), L"Prune Results", MB_OK | MB_ICONINFORMATION);
            }
            return TRUE;
        }

        case WM_TIMER:
            if (wParam == 1) {
                if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
                    AddWordToList(hwnd);
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 1, 10, NULL);  // Restart the timer
                }
            }
            break;
            /*case IDC_WORD_INPUT:
            case IDC_DEFINITION_INPUT:
                if (HIWORD(wParam) == EN_SETFOCUS) {
                    hFocusedEdit = (HWND)lParam;
                }
                break;
                //}
                //break;

                    case WM_KEYDOWN:
                if (wParam == VK_RETURN) {
                    HWND focusedControl = GetFocus();
                    if (focusedControl == hWordEdit || focusedControl == hDefinitionEdit) {
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_ADD_WORD, BN_CLICKED), 0);
                        return TRUE;
                    }
                }
                break;*/

        case IDCANCEL:
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return TRUE;
        }
        break;

    /*case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            // Check for Cancel button
            if (GetDlgItem(hwnd, IDCANCEL)) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
                return TRUE;
            }
            // Check for No button
            else if (GetDlgItem(hwnd, IDNO)) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
                return TRUE;
            }
            // For dialogs with only a close button (like View Definition)
            else {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return TRUE;
            }
        } */
        //break; //this block was already commentedout break+if->break
        /*if (wParam == VK_ESCAPE) {
            if (GetDlgItem(hwnd, IDCANCEL)) {
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
            return TRUE;
        }
    }
        break;*/
        /*if (wParam == VK_F1) {
            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
            int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
            if (selectedIndex != LB_ERR) {
                wchar_t selectedWord[256];
                SendMessage(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)selectedWord);
                Node* current = dictionary;
                while (current) {
                    if (current->data.word == selectedWord) {
                        DisplayDefinition(hwnd, current->data.word, current->data.definition);
                        break;
                    }
                    current = current->next;
                }
            }
            return TRUE;
        }
        break; */
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            // Check for Cancel button
            if (GetDlgItem(hwnd, IDCANCEL)) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
                return TRUE;
            }
            // Check for No button
            else if (GetDlgItem(hwnd, IDNO)) {
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
                return TRUE;
            }
            // For dialogs with only a close button (like View Definition)
            else {
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return TRUE;
            }
        }
        if (wParam == VK_F1) {
            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
            int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
            if (selectedIndex != LB_ERR) {
                wchar_t selectedWord[256];
                SendMessage(hListBox, LB_GETTEXT, selectedIndex, (LPARAM)selectedWord);
                Node* current = dictionary;
                while (current) {
                    if (current->data.word == selectedWord) {
                        DisplayDefinition(hwnd, current->data.word, current->data.definition);
                        break;
                    }
                    current = current->next;
                }
            }
            return TRUE;
        }
        if (wParam == VK_F2) {
            HWND hListBox = GetDlgItem(hwnd, IDC_LISTBOX);
            int selectedIndex = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
            if (selectedIndex != LB_ERR) {
                // Simulate clicking the "Edit Word" button
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_EDIT_WORD, BN_CLICKED), 0);
            }
            return TRUE;
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

    case WM_SHOWWINDOW:
        if (wParam == TRUE) {  // Only set focus when the window is shown
            SetFocus(GetDlgItem(hwnd, IDC_SEARCH_INPUT));
        }
        return 0;

    /*case WM_DESTROY:
        // In your cleanup or WM_DESTROY handling
        UnhookWindowsHookEx(g_hHook);
        DestroyWindow(hwnd);
        PostQuitMessage(0);*/

    case WM_CLOSE:
        if (g_hHook) {
            UnhookWindowsHookEx(g_hHook);
        }
        DestroyWindow(hwnd);
        PostQuitMessage(0);
        return TRUE;
    }
    
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    HWND hDlg = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG), 0, DialogProc, 0);
    if (!hDlg) {
        MessageBox(NULL, L"Failed to create dialog", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Retrieve screen dimensions
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);

    // Retrieve window dimensions
    RECT windowRect;
    GetWindowRect(hDlg, &windowRect);
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Calculate position to center the window
    int posX = (desktop.right / 2) - (windowWidth / 2);
    int posY = (desktop.bottom / 2) - (windowHeight / 2);

    // Set window position
    SetWindowPos(hDlg, NULL, posX, posY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    ShowWindow(hDlg, nCmdShow);

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (bRet == -1) {
            // Handle the error and possibly exit
            break;
        }
        else if (!IsWindow(hDlg) || !IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}