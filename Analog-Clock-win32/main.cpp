#include <windows.h>
#include <gdiplus.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <commctrl.h>
#include <tchar.h>
#include <strsafe.h>
#include "resource.h"  // Add this with your other includes
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#define ID_TIMER 1
#define SIZE 475
#define CLOCK_MARGIN 20
#define ID_STATUSBAR 100
//SIZE 425
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawClock(HDC, int, int, int);
void DrawHand(HDC hdc, int x, int y, double angle, int length, COLORREF color, bool drawLineToCenter = true);
void UpdateStatusBar(HWND, SYSTEMTIME);
LRESULT CALLBACK StatusBarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
// Declare gdiplusToken as a global variable
ULONG_PTR gdiplusToken;

LRESULT CALLBACK CalendarSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        DestroyWindow(hWnd);
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow) {
    // Add this to WinMain before the window creation:
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    static TCHAR szAppName[] = TEXT("My Clock");
    HWND hwnd;
    MSG msg;
    WNDCLASS wndclass;
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    //wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppName;
    if (!RegisterClass(&wndclass)) {
        MessageBox(NULL, TEXT("Program requires Windows NT!"), szAppName, MB_ICONERROR);
        return 0;
    }
    RECT rect = { 0, 0, SIZE + 2 * CLOCK_MARGIN, SIZE + 2 * CLOCK_MARGIN + 30 };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate centered position
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    int xPos = (screenWidth - windowWidth) / 2;
    int yPos = (screenHeight - windowHeight) / 2;

    hwnd = CreateWindowEx(WS_EX_TOPMOST, // Make the window always on top
        szAppName, TEXT("Time is Meaningless The Insurgency is Live & Well (F1=About F2=Calendar)"),
        WS_OVERLAPPEDWINDOW, xPos, yPos,
        windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hwndStatus;
    static HBITMAP hBitmap;
    static int cxClient, cyClient;
    HDC hdc, memDC;
    PAINTSTRUCT ps;
    SYSTEMTIME st;
    RECT rect;
    int centerX, centerY;

    // Moved the declaration outside the switch
    int statwidths[] = { -1 };

    switch (message) {
    case WM_CREATE:
        // Enable double buffering
        SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_CLIPCHILDREN);

        hwndStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
            hwnd, (HMENU)ID_STATUSBAR, GetModuleHandle(NULL), NULL);

        SendMessage(hwndStatus, SB_SETPARTS, 1, (LPARAM)statwidths);
        SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)TEXT(""));
        SetWindowSubclass(hwndStatus, StatusBarProc, 0, 0);
        SetTimer(hwnd, ID_TIMER, 1000, NULL);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        // Add this new code block
        else if (wParam == VK_F1) {
            MessageBox(hwnd, TEXT("Analog Clock (incorporates SubClassing Controls) Programmed in C++ Win32 API (395 lines of code) by Entisoft Software (c) Evans Thorpemorton"), TEXT("Information"), MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        else if (wParam == VK_F2) {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            int calWidth = 350, calHeight = 300;
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
            int xPos = (screenWidth - calWidth) / 2;
            int yPos = (screenHeight - calHeight) / 2;
            HWND hCal = CreateWindowEx(WS_EX_TOPMOST, MONTHCAL_CLASS, NULL,
                WS_POPUP | WS_BORDER,
                xPos, yPos, calWidth, calHeight, hwnd, NULL, hInst, NULL);
            if (hCal) {
                SendMessage(hCal, MCM_SETFIRSTDAYOFWEEK, (WPARAM)1, 0);
                ShowWindow(hCal, SW_SHOW);
                // Subclass the calendar to intercept the Escape key
                SetWindowSubclass(hCal, CalendarSubclassProc, 0, 0);
            }
            return 0;
        }
        break;
    case WM_SIZE:
        cxClient = LOWORD(lParam);
        cyClient = HIWORD(lParam);
        if (hBitmap) {
            DeleteObject(hBitmap);
        }
        hdc = GetDC(hwnd);
        hBitmap = CreateCompatibleBitmap(hdc, cxClient, cyClient);
        ReleaseDC(hwnd, hdc);
        SendMessage(hwndStatus, WM_SIZE, 0, 0);
        return 0;
    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        memDC = CreateCompatibleDC(hdc);
        SelectObject(memDC, hBitmap);
        GetClientRect(hwnd, &rect);
        centerX = (rect.right - rect.left) / 2;
        centerY = ((rect.bottom - rect.top) - 20) / 2;
        FillRect(memDC, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        DrawClock(memDC, centerX, centerY, min(centerX, centerY) - CLOCK_MARGIN);
        BitBlt(hdc, 0, 0, cxClient, cyClient, memDC, 0, 0, SRCCOPY);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    case WM_TIMER:
        GetLocalTime(&st);
        UpdateStatusBar(hwndStatus, st);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        if (hBitmap) DeleteObject(hBitmap);
        // Add this to WM_DESTROY handler:
        Gdiplus::GdiplusShutdown(gdiplusToken);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void DrawClock(HDC hdc, int x, int y, int r) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    // Create memory DC for double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, 2 * r, 2 * r);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    // Calculate center point for the memory DC
    int centerX = r;  // Center X in memory DC coordinates
    int centerY = r;  // Center Y in memory DC coordinates

    // Draw gradient background
    TRIVERTEX vertex[2];
    vertex[0].x = 0;
    vertex[0].y = 0;
    vertex[0].Red = 0xc000;
    vertex[0].Green = 0xc000;
    vertex[0].Blue = 0xc000;
    vertex[0].Alpha = 0x0000;
    vertex[1].x = 2 * r;
    vertex[1].y = 2 * r;
    vertex[1].Red = 0x4000;
    vertex[1].Green = 0x4000;
    vertex[1].Blue = 0x4000;
    vertex[1].Alpha = 0x0000;
    GRADIENT_RECT gRect = { 0, 1 };
    GradientFill(memDC, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);

    // Draw clock outline
    HPEN hPen = CreatePen(PS_SOLID, 8, RGB(128, 128, 128));
    HPEN hOldPen = (HPEN)SelectObject(memDC, hPen);
    Ellipse(memDC, 0, 0, 2 * r, 2 * r);

    // Draw hour numbers
    HFONT hFont = CreateFont(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, TEXT("Segoe UI"));
    HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);
    SetTextColor(memDC, RGB(112, 128, 144));
    SetTextAlign(memDC, TA_CENTER | TA_BASELINE);
    SetBkMode(memDC, TRANSPARENT);

    for (int i = 1; i <= 12; i++) {
        double angle = (i * 30 - 90) * (3.14159265358979323846 / 180);
        int numX = centerX + (int)(cos(angle) * (r - 25));
        int numY = centerY + (int)(sin(angle) * (r - 25));
        WCHAR numStr[3];
        wsprintf(numStr, L"%d", i);
        TextOut(memDC, numX, numY, numStr, lstrlen(numStr));
    }

    // Draw markers
    for (int i = 0; i < 60; i++) {
        if (i % 5 == 0) {
            DrawHand(memDC, centerX, centerY, i * 6, r - 10, RGB(0, 0, 0), false);
        }
        else {
            DrawHand(memDC, centerX, centerY, i * 6, r - 5, RGB(0, 0, 0), false);
        }
    }

    // Draw clock hands
    DrawHand(memDC, centerX, centerY, st.wSecond * 6, r - 30, RGB(255, 0, 0));
    DrawHand(memDC, centerX, centerY, (st.wMinute * 6) + (st.wSecond / 10.0), r - 60, RGB(0, 0, 0));
    DrawHand(memDC, centerX, centerY, (st.wHour * 30) + (st.wMinute / 2.0), r - 90, RGB(0, 0, 255));

    // Draw center white circle
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, whiteBrush);
    Ellipse(memDC, centerX - 10, centerY - 10, centerX + 10, centerY + 10);
    SelectObject(memDC, oldBrush);
    DeleteObject(whiteBrush);

    // Add AM/PM text
    SelectObject(memDC, hOldFont);
    DeleteObject(hFont);

    hFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, TEXT("Segoe UI"));
    SelectObject(memDC, hFont);
    SetTextColor(memDC, RGB(128, 128, 128));
    TextOut(memDC, centerX, centerY + (r - 70), st.wHour < 12 ? TEXT("AM") : TEXT("PM"), 2);

    // Copy the final result to the screen
    BitBlt(hdc, x - r, y - r, 2 * r, 2 * r, memDC, 0, 0, SRCCOPY);

    // Final cleanup
    SelectObject(memDC, hOldFont);
    SelectObject(memDC, hOldPen);
    SelectObject(memDC, hOldBitmap);
    DeleteObject(hFont);
    DeleteObject(hPen);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
}

void DrawHand(HDC hdc, int x, int y, double angle, int length, COLORREF color, bool drawLineToCenter) {
    double radian = (angle - 90) * (3.14159265358979323846 / 180);
    int extensionLength = length * 0.1;

    // Create GDI+ Graphics object
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    if (drawLineToCenter) {
        if (color == RGB(255, 0, 0)) {  // Second hand
            Gdiplus::Point pts[3];
            
            // Calculate points
            pts[0].X = x + (int)(cos(radian) * length);
            pts[0].Y = y + (int)(sin(radian) * length);

            double perpRadian = radian + 3.14159265358979323846 / 2;
            int baseWidth = 8;
            pts[1].X = x - (int)(cos(radian) * extensionLength) + (int)(cos(perpRadian) * baseWidth);
            pts[1].Y = y - (int)(sin(radian) * extensionLength) + (int)(sin(perpRadian) * baseWidth);
            pts[2].X = x - (int)(cos(radian) * extensionLength) - (int)(cos(perpRadian) * baseWidth);
            pts[2].Y = y - (int)(sin(radian) * extensionLength) - (int)(sin(perpRadian) * baseWidth);

            // Create brush and pen
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
            graphics.FillPolygon(&brush, pts, 3);
        }
        else {
            // Hour and minute hands
            int penThickness = (color == RGB(0, 0, 255)) ? 6 : 4;
            Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), 
                            (Gdiplus::REAL)penThickness);
            
            int endX = x + (int)(cos(radian) * length);
            int endY = y + (int)(sin(radian) * length);
            int startX = x - (int)(cos(radian) * extensionLength);
            int startY = y - (int)(sin(radian) * extensionLength);

            graphics.DrawLine(&pen, startX, startY, endX, endY);
        }
    }
    else {
        // Markers
        int markerThickness = ((int)angle % 30 == 0) ? 4 : 2;
        int markerLength = ((int)angle % 30 == 0) ? 15 : 10; // Longer markers for hours

        Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), 
                        (Gdiplus::REAL)markerThickness);

        int endX = x + (int)(cos(radian) * length);
        int endY = y + (int)(sin(radian) * length);
        int startX = endX + (int)(cos(radian) * markerLength);
        int startY = endY + (int)(sin(radian) * markerLength);

        graphics.DrawLine(&pen, endX, endY, startX, startY);
    }
}

void UpdateStatusBar(HWND hwndStatus, SYSTEMTIME st) {
    TCHAR szStatus[256];
    TCHAR szDayOfWeek[32], szMonth[32];
    TCHAR szSuffix[3];
    int weekNumber;

    GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, TEXT("dddd"), szDayOfWeek, sizeof(szDayOfWeek) / sizeof(TCHAR));
    GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, TEXT("MMMM"), szMonth, sizeof(szMonth) / sizeof(TCHAR));

    SYSTEMTIME newYearDay = { st.wYear, 1, 0, 1 };
    FILETIME ftNewYearDay, ftCurrentDay;
    SystemTimeToFileTime(&newYearDay, &ftNewYearDay);
    SystemTimeToFileTime(&st, &ftCurrentDay);
    ULARGE_INTEGER uliNewYearDay, uliCurrentDay;
    uliNewYearDay.LowPart = ftNewYearDay.dwLowDateTime;
    uliNewYearDay.HighPart = ftNewYearDay.dwHighDateTime;
    uliCurrentDay.LowPart = ftCurrentDay.dwLowDateTime;
    uliCurrentDay.HighPart = ftCurrentDay.dwHighDateTime;
    weekNumber = (int)(((uliCurrentDay.QuadPart - uliNewYearDay.QuadPart) / (7 * 24 * 60 * 60 * 10000000ULL)) + 1);

    if (st.wDay == 1 || st.wDay == 21 || st.wDay == 31)
        _tcscpy_s(szSuffix, _T("st"));
    else if (st.wDay == 2 || st.wDay == 22)
        _tcscpy_s(szSuffix, _T("nd"));
    else if (st.wDay == 3 || st.wDay == 23)
        _tcscpy_s(szSuffix, _T("rd"));
    else
        _tcscpy_s(szSuffix, _T("th"));

    // Get time zone information
    TIME_ZONE_INFORMATION tzi;
    GetTimeZoneInformation(&tzi);
    int bias = -(tzi.Bias); // Negative because Bias is in opposite direction
    int hours = bias / 60;
    int minutes = abs(bias % 60);

    StringCbPrintf(szStatus, sizeof(szStatus),
        TEXT("%s %d%s %s %d #%d %d/%02d/%04d %02d:%02d:%02d %s GMT%s%d:%02d"),
        szDayOfWeek, st.wDay, szSuffix, szMonth, st.wYear, weekNumber,
        st.wDay, st.wMonth, st.wYear,
        (st.wHour == 0 || st.wHour == 12) ? 12 : st.wHour % 12,
        st.wMinute, st.wSecond,
        st.wHour < 12 ? TEXT("AM") : TEXT("PM"),
        (hours >= 0) ? TEXT("+") : TEXT(""),
        hours,
        minutes);

    SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)szStatus);
    InvalidateRect(hwndStatus, NULL, TRUE);
}

LRESULT CALLBACK StatusBarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        TCHAR szText[256];
        int len = SendMessage(hWnd, SB_GETTEXT, 0, (LPARAM)szText);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

        DrawText(hdc, szText, len, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}