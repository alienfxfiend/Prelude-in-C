#define NOMINMAX
#include <windows.h>
#include <windowsx.h>  // Add this line
#include <commctrl.h>
#include <string>
#include <vector>
#include <algorithm>  // Add this at the top with other includes
#include <functional>
#include <ctime>    // for time()
#include <cstdlib>  // for rand() and srand()
#include <random>
#include <sstream>
#include <iostream>
#include <map>
#include <richedit.h> // Include for Rich Edit control
#include <shellapi.h>
#include "resource.h"

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "comctl32.lib") // Link with the common controls library
//#pragma comment(lib, "riched32.lib")  // Link with the rich edit library
#pragma comment(lib, "Shell32.lib")

#define WIN_WIDTH 550
#define WIN_HEIGHT 400

// Structure for a binary tree node
struct Node {
    int data;
    Node* left;
    Node* right;

    Node(int val) : data(val), left(nullptr), right(nullptr) {}
};

// Function to insert a new node into the binary tree
Node* insert(Node* root, int val) {
    if (!root) {
        return new Node(val);
    }

    if (val < root->data) {
        root->left = insert(root->left, val);
    }
    else {
        root->right = insert(root->right, val);
    }
    return root;
}


// Helper function to get the height of the tree
int getHeight(Node* root) {
    if (!root) return 0;
    return 1 + std::max(getHeight(root->left), getHeight(root->right));
}

// Helper function to add padding
std::string getPadding(int level) {
    return std::string(level * 6, ' ');
}

std::string getListString(Node* root, std::string prefix = "") {
    if (!root) return "";

    std::string result = prefix + "|_ " + std::to_string(root->data) + "\r\n";

    if (root->left) {
        bool hasRight = (root->right != nullptr);
        std::string newPrefix = prefix + (hasRight ? "|  " : "   ");
        result += getListString(root->left, newPrefix);
    }

    if (root->right) {
        std::string newPrefix = prefix + "   ";
        result += getListString(root->right, newPrefix);
    }

    return result;
}

void deleteTree(Node* node) {
    if (node) {
        deleteTree(node->left);
        deleteTree(node->right);
        delete node;
    }
}

// Modified getTreeString function with correct lambda capture
std::string getTreeString(Node* root) {
    if (!root) return "";

    struct NodePosition {
        int x;          // x coordinate
        int width;      // node width
        int parentX;    // parent's x coordinate
        int depth;      // depth in tree
        bool isLeft;    // is left child
        Node* node;     // pointer to actual node
    };

    std::vector<std::string> levels;
    std::vector<NodePosition> nodePositions;
    int maxLevelWidth = 0;
    const int BASE_OFFSET = 25; //orig 40
    const int MIN_SPACING = 3; //orig 4

    // First pass: Calculate positions and store node information
    std::function<void(Node*, int, int, int, bool)> calculatePositions =
        [&](Node* node, int depth, int x, int parentX, bool isLeft) {
        if (!node) return;

        std::string nodeStr = std::to_string(node->data);
        int nodeWidth = nodeStr.length();

        // Store node information
        nodePositions.push_back({
            x,          // x coordinate
            nodeWidth,  // width
            parentX,    // parent x
            depth,      // depth
            isLeft,     // is left child
            node       // node pointer
            });

        // Calculate spacing based on depth and node width
        int spacing = std::max(MIN_SPACING, nodeWidth + 2);
        if (depth > 4) {
            spacing = std::max(3, spacing - (depth - 4) / 2);
        }

        // Calculate children positions
        if (node->left) {
            calculatePositions(node->left, depth + 2, x - spacing, x, true);
        }
        if (node->right) {
            calculatePositions(node->right, depth + 2, x + spacing, x + nodeWidth - 1, false);
        }
    };

    // Second pass: Draw nodes and validate connections
    std::function<void(const NodePosition&)> drawNode =
        [&](const NodePosition& pos) {
        // Ensure enough levels
        while (levels.size() <= pos.depth + 1) {
            levels.push_back(std::string());
        }

        // Extend level if needed
        if (levels[pos.depth].length() <= pos.x + pos.width) {
            levels[pos.depth].resize(pos.x + pos.width + 1, ' ');
        }

        // Place node value
        std::string nodeStr = std::to_string(pos.node->data);
        levels[pos.depth].replace(pos.x, pos.width, nodeStr);

        // Handle branch to parent (if not root)
        if (pos.depth > 0) {
            int branchRow = pos.depth - 1;

            // Ensure branch level is wide enough
            int maxX = std::max(pos.x + pos.width, pos.parentX) + 1;
            if (levels[branchRow].length() <= maxX) {
                levels[branchRow].resize(maxX + 1, ' ');
            }

            // Clear existing branches in the area
            for (int i = std::min(pos.x, pos.parentX); i <= std::max(pos.x + pos.width, pos.parentX); i++) {
                if (levels[branchRow][i] != '/' && levels[branchRow][i] != '\\') {
                    levels[branchRow][i] = ' ';
                }
            }

            // Place branch character
            if (pos.isLeft) {
                levels[branchRow][pos.x + pos.width - 1] = '/';
            }
            else {
                levels[branchRow][pos.x] = '\\';
            }
        }

        maxLevelWidth = std::max(maxLevelWidth, (int)levels[pos.depth].length());
    };

    // Third pass: Validate and fix connections
    auto validateConnections = [&]() {
        for (size_t i = 1; i < levels.size(); i++) {
            std::vector<bool> hasConnection(levels[i].length(), false);

            // Mark existing connections
            for (const auto& pos : nodePositions) {
                if (pos.depth == i) {
                    int start = std::max(0, pos.x - 2);
                    int end = std::min((int)levels[i - 1].length() - 1, pos.x + pos.width + 1);
                    for (int j = start; j <= end; j++) {
                        if (levels[i - 1][j] == '/' || levels[i - 1][j] == '\\') {
                            hasConnection[pos.x] = true;
                            break;
                        }
                    }
                }
            }

            // Fix missing connections
            for (const auto& pos : nodePositions) {
                if (pos.depth == i && !hasConnection[pos.x]) {
                    if (pos.isLeft) {
                        if (pos.x + pos.width - 1 < levels[i - 1].length()) {
                            levels[i - 1][pos.x + pos.width - 1] = '/';
                        }
                    }
                    else {
                        if (pos.x < levels[i - 1].length()) {
                            levels[i - 1][pos.x] = '\\';
                        }
                    }
                }
            }

            // Remove duplicate slashes
            for (size_t j = 1; j < levels[i - 1].length(); j++) {
                if ((levels[i - 1][j] == '/' && levels[i - 1][j - 1] == '/') ||
                    (levels[i - 1][j] == '\\' && levels[i - 1][j - 1] == '\\')) {
                    levels[i - 1][j - 1] = ' ';
                }
            }
        }
    };

    // Execute all passes
    calculatePositions(root, 0, BASE_OFFSET, 0, false);

    // Sort nodes by depth for proper drawing order
    std::sort(nodePositions.begin(), nodePositions.end(),
        [](const NodePosition& a, const NodePosition& b) {
            return a.depth < b.depth;
        });

    // Draw all nodes
    for (const auto& pos : nodePositions) {
        drawNode(pos);
    }

    // Validate and fix connections
    validateConnections();

    // Build final string with proper alignment
    std::string result;
    for (const auto& level : levels) {
        std::string trimmedLevel = level;
        while (!trimmedLevel.empty() && trimmedLevel.back() == ' ') {
            trimmedLevel.pop_back();
        }
        result += trimmedLevel + "\r\n";
    }

    return result;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {

    const wchar_t CLASS_NAME[] = L"BinaryTreeVisualizer (Graphing Application)";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    RegisterClass(&wc);

    // Remove WS_MAXIMIZEBOX and WS_THICKFRAME from the window style
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX) & ~WS_THICKFRAME;

    // Get the screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate the window position to center it
    int windowWidth = WIN_WIDTH + 16;
    int windowHeight = WIN_HEIGHT + 39;
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Binary Tree Visualizer (MathxCore) F1=About",    // Window text
        style,            // Window style

        posX, posY, 535, 385,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit, hDisplay, hTreeViewRadio, hListViewRadio, hGenerateButton;
    static Node* root = nullptr;

    switch (uMsg) {
    case WM_CREATE: {
        // Create Input Field
        hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            10, 10, 100, 20, hwnd, NULL, NULL, NULL);

        // Create multiline Edit control for display
            // Create display Edit control with proper styles for newlines
        hDisplay = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | ES_LEFT,
            10, 40, 510, 300, hwnd, NULL, NULL, NULL);

        // Radio Buttons
        hTreeViewRadio = CreateWindowEx(0, L"BUTTON", L"Tree View",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            120, 10, 100, 20, hwnd, (HMENU)1, NULL, NULL);

        hListViewRadio = CreateWindowEx(0, L"BUTTON", L"List View",
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            230, 10, 100, 20, hwnd, (HMENU)2, NULL, NULL);

        // Generate Button
        hGenerateButton = CreateWindowEx(0, L"BUTTON", L"Generate Tree",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            340, 10, 100, 20, hwnd, (HMENU)3, NULL, NULL);

        Button_SetCheck(hTreeViewRadio, BST_CHECKED);

        // Set monospace font
            // Set monospace font for better tree display
        HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        SendMessage(hDisplay, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Set tab stops for the display Edit control (optional)
        DWORD tabStops = 16; // in dialog units
        SendMessage(hDisplay, EM_SETTABSTOPS, 1, (LPARAM)&tabStops);

        // Set text alignment to center
        //SendMessage(hDisplay, EM_SETALIGNMENT, ES_CENTER, 0);

        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 3) { // Generate button clicked
            wchar_t buffer[256];
            GetWindowText(hEdit, buffer, 256);

            wchar_t* endptr;
            long numNodes = std::wcstol(buffer, &endptr, 10);

            if (*endptr == L'\0' && numNodes > 0 && numNodes <= 100) {
                // Delete old tree
                if (root != nullptr) {
                    deleteTree(root);
                    root = nullptr;
                }

                // Generate new tree
                try {
                    std::vector<int> numbers(numNodes);
                    for (int i = 0; i < numNodes; ++i) {
                        numbers[i] = i + 1;
                    }

                    std::random_device rd;
                    std::mt19937 g(rd());
                    std::shuffle(numbers.begin(), numbers.end(), g);

                    for (int num : numbers) {
                        root = insert(root, num);
                    }

                    // Generate tree string
                    std::string treeString;
                    if (Button_GetCheck(hTreeViewRadio) == BST_CHECKED) {
                        treeString = getTreeString(root);
                    }
                    else {
                        treeString = getListString(root);
                    }

                    // Convert to wide string and set text
                    int size_needed = MultiByteToWideChar(CP_UTF8, 0, treeString.c_str(), -1, NULL, 0);
                    if (size_needed > 0) {
                        std::vector<wchar_t> wideStr(size_needed);
                        MultiByteToWideChar(CP_UTF8, 0, treeString.c_str(), -1, &wideStr[0], size_needed);
                        SetWindowText(hDisplay, &wideStr[0]);
                    }

                    // Scroll to top
                    SendMessage(hDisplay, WM_VSCROLL, SB_TOP, 0);
                    SendMessage(hDisplay, EM_SETSEL, 0, 0);
                    SendMessage(hDisplay, EM_SCROLLCARET, 0, 0);
                }
                catch (const std::exception& e) {
                    MessageBoxA(hwnd, e.what(), "Error", MB_OK | MB_ICONERROR);
                }
            }
            else {
                MessageBox(hwnd, L"Please enter a number between 1 and 100.",
                    L"Invalid Input", MB_OK | MB_ICONWARNING);
            }
            return 0;
        }
        else if (LOWORD(wParam) == 1 || LOWORD(wParam) == 2) {  // Radio button handling
            if (root != nullptr) {
                std::string treeString;
                if (Button_GetCheck(hTreeViewRadio) == BST_CHECKED) {
                    treeString = getTreeString(root);
                    std::string::size_type pos = 0;
                    while ((pos = treeString.find('\n', pos)) != std::string::npos) {
                        treeString.replace(pos, 1, "\r\n");
                        pos += 2;
                    }
                }
                else {
                    treeString = getListString(root);
                }

                int size_needed = MultiByteToWideChar(CP_UTF8, 0, treeString.c_str(), -1, NULL, 0);
                if (size_needed == 0) {
                    OutputDebugString(L"MultiByteToWideChar failed (Radio)\n");
                    break;
                }
                std::wstring wideStr(size_needed - 1, 0);
                int converted = MultiByteToWideChar(CP_UTF8, 0, treeString.c_str(), -1, &wideStr[0], size_needed);
                if (converted == 0) {
                    OutputDebugString(L"MultiByteToWideChar failed (Radio)\n");
                    break;
                }

                SetWindowText(hDisplay, wideStr.c_str());

                LONG_PTR style = GetWindowLongPtr(hDisplay, GWL_STYLE);
                style |= ES_MULTILINE | ES_AUTOVSCROLL;
                SetWindowLongPtr(hDisplay, GWL_STYLE, style);

                SendMessage(hDisplay, WM_VSCROLL, SB_TOP, 0);
                SendMessage(hDisplay, EM_SETSEL, 0, 0);
                SendMessage(hDisplay, EM_SCROLLCARET, 0, 0);
            }
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        // Add this new code block
        else if (wParam == VK_F1) {
            MessageBox(hwnd, TEXT("Tree/ List Diagrams (incorporates robust GDI drawing techniques) Programmed in C++ Win32 API (479 lines of code) by Entisoft Software (c) Evans Thorpemorton"), TEXT("Information"), MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;

    case WM_DESTROY:
        delete root; // Clean up
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}