#include <windows.h>
#include <iostream>
#include <vector>
#include <shellapi.h>
#include "resource.h"

#define WINDOW_CLASS_NAME L"MouseSpotterClass"
#define WINDOW_TITLE L"MouseSpotter"
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1001
#define ID_TRAY_EXIT 1002
#define ID_TRAY_TOGGLE 1003
#define ID_TRAY_ABOUT 1004

bool g_isEnabled = true;
COLORREF g_spotterColor = RGB(255, 0, 0);
std::vector<HWND> g_excludedWindows;
int g_spotterSize = 64;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void EnableDebugPrivileges();
bool IsWindowExcluded(HWND windowHandle);
void DrawSpotter(HWND hwnd, HDC hdc);

class MouseSpotter {
private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    bool m_isRunning;
    NOTIFYICONDATA m_nid;

    void InitializeTrayIcon() {
        ZeroMemory(&m_nid, sizeof(NOTIFYICONDATA));
        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.hWnd = m_hwnd;
        m_nid.uID = ID_TRAYICON;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));
        wcscpy_s(m_nid.szTip, L"MouseSpotter");

        if (!Shell_NotifyIcon(NIM_ADD, &m_nid)) {
            MessageBox(NULL, L"Failed to add tray icon!", L"Error", MB_ICONERROR | MB_OK);
        }
    }

    void ShowTrayMenu() {
        if (!m_hwnd) {
            MessageBox(NULL, L"Invalid window handle!", L"Error", MB_ICONERROR | MB_OK);
            return;
        }

        // Get the cursor position
        POINT curPoint;
        GetCursorPos(&curPoint);

        // Create the popup menu
        HMENU hPopMenu = CreatePopupMenu();
        if (!hPopMenu) {
            MessageBox(NULL, L"Failed to create tray menu!", L"Error", MB_ICONERROR | MB_OK);
            return;
        }

        // Add menu items
        InsertMenu(hPopMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_TOGGLE, g_isEnabled ? L"Disable" : L"Enable");
        InsertMenu(hPopMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_ABOUT, L"About");
        InsertMenu(hPopMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hPopMenu, 3, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

        // Ensure the window is in the foreground
        SetForegroundWindow(m_hwnd);

        // Display the menu
        TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, curPoint.x, curPoint.y, 0, m_hwnd, NULL);

        // Post a dummy message to ensure the menu works correctly
        PostMessage(m_hwnd, WM_NULL, 0, 0);

        // Destroy the menu after use
        DestroyMenu(hPopMenu);
    }

public:
    MouseSpotter(HINSTANCE hInstance) : m_hInstance(hInstance), m_isRunning(true) {
        InitializeWindow();
    }

    ~MouseSpotter() {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
        UnregisterClass(WINDOW_CLASS_NAME, m_hInstance);
    }

    void HandleTrayMessage(WPARAM wParam, LPARAM lParam) {
        switch (lParam) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu();
            break;
        case WM_LBUTTONDBLCLK:
            g_isEnabled = !g_isEnabled;
            if (g_isEnabled) {
                ShowWindow(m_hwnd, SW_SHOW);
            }
            else {
                ShowWindow(m_hwnd, SW_HIDE);
            }
            break;
        }
    }

    bool InitializeWindow() {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = WINDOW_CLASS_NAME;
        wc.hbrBackground = NULL;
        wc.hCursor = LoadCursor(NULL, IDC_CROSS);
        wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));
        wc.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));

        if (!RegisterClassEx(&wc)) {
            MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
            return false;
        }

        m_hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            WINDOW_CLASS_NAME,
            WINDOW_TITLE,
            WS_POPUP,
            0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            NULL, NULL, m_hInstance, this
        );

        if (!m_hwnd) {
            MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
            return false;
        }

        SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        InitializeTrayIcon();
        return true;
    }

    void Run() {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        MSG msg;
        POINT cursorPos;
        RECT spotterRect;

        while (m_isRunning) {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    m_isRunning = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (!g_isEnabled) {
                Sleep(100);
                continue;
            }

            GetCursorPos(&cursorPos);
            HWND foregroundWindow = GetForegroundWindow();
            if (!IsWindowExcluded(foregroundWindow)) {
                spotterRect.left = cursorPos.x - (g_spotterSize / 2);
                spotterRect.top = cursorPos.y - (g_spotterSize / 2);
                spotterRect.right = spotterRect.left + g_spotterSize;
                spotterRect.bottom = spotterRect.top + g_spotterSize;

                SetWindowPos(m_hwnd, HWND_TOPMOST,
                    spotterRect.left, spotterRect.top,
                    g_spotterSize, g_spotterSize,
                    SWP_NOACTIVATE);
                InvalidateRect(m_hwnd, NULL, TRUE);
            }
            Sleep(1);
        }
    }

    HWND GetHWND() const { return m_hwnd; }
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static MouseSpotter* pSpotter = nullptr;

    if (uMsg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pSpotter = reinterpret_cast<MouseSpotter*>(pCreate->lpCreateParams);
    }

    switch (uMsg) {
    case WM_TRAYICON:
        if (pSpotter) {
            pSpotter->HandleTrayMessage(wParam, lParam);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            return 0;
        case ID_TRAY_TOGGLE:
            g_isEnabled = !g_isEnabled;
            if (g_isEnabled) {
                ShowWindow(hwnd, SW_SHOW);
            }
            else {
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        case ID_TRAY_ABOUT:
            MessageBox(NULL, L"Ctrl+Alt+C to Toggle\nAlt+F3 to Exit", L"Hotkey Info", MB_ICONINFORMATION | MB_OK);
            return 0;
        }
        break;


    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawSpotter(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        UnregisterHotKey(hwnd, 1);
        UnregisterHotKey(hwnd, 2);
        UnregisterHotKey(hwnd, 3);
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_F1:
            MessageBox(NULL, L"Ctrl+Alt+C to Toggle\nAlt+F3 to Exit", L"Hotkey Info", MB_ICONINFORMATION | MB_OK);
            return 0;
        }
        break;

    case WM_CREATE: {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pSpotter = reinterpret_cast<MouseSpotter*>(pCreate->lpCreateParams);
        RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 'C');
        RegisterHotKey(hwnd, 2, MOD_ALT, VK_F3);
        RegisterHotKey(hwnd, 3, 0, VK_F1);
        return 0;
    }

    case WM_HOTKEY:
        if (wParam == 1) {
            g_isEnabled = !g_isEnabled;
            if (g_isEnabled) {
                ShowWindow(hwnd, SW_SHOW);
            }
            else {
                ShowWindow(hwnd, SW_HIDE);
            }
        }
        else if (wParam == 2) {
            PostQuitMessage(0);
        }
        else if (wParam == 3) {
            MessageBox(NULL, L"Ctrl+Alt+C to Toggle\nAlt+F3 to Exit", L"Hotkey Info", MB_ICONINFORMATION | MB_OK);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//transparent crosshair
void DrawSpotter(HWND hwnd, HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);

    // Create a pen for drawing
    HPEN hPen = CreatePen(PS_SOLID, 2, g_spotterColor);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // Create a hollow brush for transparent circle
    HBRUSH hBrush = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    int centerX = g_spotterSize / 2;
    int centerY = g_spotterSize / 2;
    int radius = g_spotterSize / 4;

    // Draw the circle first
    Ellipse(hdc, centerX - radius, centerY - radius,
        centerX + radius, centerY + radius);

    // Draw crosshair over the circle
    MoveToEx(hdc, centerX, 0, NULL);
    LineTo(hdc, centerX, g_spotterSize);
    MoveToEx(hdc, 0, centerY, NULL);
    LineTo(hdc, g_spotterSize, centerY);

    // Clean up
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}

//crosshair black
/*void DrawSpotter(HWND hwnd, HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    // Create pen for drawing
    HPEN hPen = CreatePen(PS_SOLID, 2, g_spotterColor);
    SelectObject(hdc, hPen);

    // Calculate center point
    int centerX = g_spotterSize / 2;
    int centerY = g_spotterSize / 2;

    // Draw vertical line
    MoveToEx(hdc, centerX, 0, NULL);
    LineTo(hdc, centerX, g_spotterSize);

    // Draw horizontal line
    MoveToEx(hdc, 0, centerY, NULL);
    LineTo(hdc, g_spotterSize, centerY);

    // Clean up
    DeleteObject(hPen);
}*/

//red circle
/*void DrawSpotter(HWND hwnd, HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    // Create pen and brush for drawing
    HPEN hPen = CreatePen(PS_SOLID, 2, g_spotterColor);
    HBRUSH hBrush = CreateSolidBrush(g_spotterColor);

    SelectObject(hdc, hPen);
    SelectObject(hdc, hBrush);

    // Draw crosshair or circle
    Ellipse(hdc, 0, 0, g_spotterSize, g_spotterSize);

    // Clean up
    DeleteObject(hPen);
    DeleteObject(hBrush);
}*/

bool IsWindowExcluded(HWND windowHandle) {
    return std::find(g_excludedWindows.begin(), g_excludedWindows.end(), windowHandle) != g_excludedWindows.end();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    MouseSpotter spotter(hInstance);
    spotter.Run();
    return 0;
}