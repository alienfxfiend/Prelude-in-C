#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>  // add if not already included
#include <cmath>
#include <vector>
#include <mutex>
#include <fstream>
#include <thread>
#include <algorithm>
#include "resource.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "comctl32.lib")

//----------------------------------------------------------------
// Data Structures and Globals
//----------------------------------------------------------------

struct DrawPoint {
    int x, y;
    DWORD timestamp;
    DrawPoint() : x(0), y(0), timestamp(0) {}
    DrawPoint(int px, int py) : x(px), y(py), timestamp(GetTickCount()) {}
};

struct SerializedStroke {
    std::vector<DrawPoint> points;
    COLORREF color;
    int brushSize;
    bool isEraser;
};

std::mutex strokeMutex;
std::vector<SerializedStroke> strokeHistory;
std::vector<DrawPoint> strokeBuffer;
const double MIN_DISTANCE = 2.0;

COLORREF currentBrushColor = RGB(24, 123, 205);
int brushSize = 10;
int currentStrokeBrushSize = 10;
bool isDrawing = false;
bool isEraserMode = false;
bool isPaintbrushSelected = true;
bool isSpacePressed = false;
POINT lastMousePos = { 0, 0 };

int scrollX = 0;
int scrollY = 0;
float gridZoomFactor = 1.0f;
bool showGrid = true;
bool useAlphaGrid = false;
int gridOpacity = 255;
// Global view transform for panning.
D2D1_MATRIX_3X2_F g_viewTransform = D2D1::Matrix3x2F::Identity();
const int GRID_SIZE = 100;

HINSTANCE hInst;
HWND hWnd;
//HWND hStatusBar = NULL;
// Global DirectWrite objects:
IDWriteFactory* pDWriteFactory = nullptr;
IDWriteTextFormat* pTextFormat = nullptr;
std::wstring g_statusText = L"";
// Add DirectWrite globals and a global status string:
//IDWriteFactory* pDWriteFactory = nullptr;
//IDWriteTextFormat* pTextFormat = nullptr;
DWORD lastStatusUpdateTime = 0;
const DWORD STATUS_UPDATE_INTERVAL = 50;
HDC hStatusBufferDC = NULL;
HBITMAP hStatusBufferBitmap = NULL;

// Serialization globals
const wchar_t* STATE_FILE = L"canvas_state2.bin";
bool isLoading = false;
bool sessionDirty = false;

// For Direct2D
ID2D1Factory* pFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1BitmapRenderTarget* pOffscreenRT = nullptr;
bool offscreenDirty = true;
int lastOffscreenScrollX = 0;
int lastOffscreenScrollY = 0;

//----------------------------------------------------------------
// Function Declarations
//----------------------------------------------------------------

void SaveCanvasState();
void LoadCanvasStateAsync(HWND hwnd);
void UpdateStatus(HWND hwnd);
void InitializeStatusBuffer(HWND hStatus);
void UpdateOffscreenBuffer(HWND hwnd);
HRESULT CreateDeviceResources(HWND hwnd);
void DiscardDeviceResources();
void DrawSmoothStroke(ID2D1RenderTarget* pRT, const std::vector<DrawPoint>& points, bool isEraser, COLORREF strokeColor, int strokeSize, int offsetX, int offsetY);
void DrawGrid(ID2D1RenderTarget* pRT, const D2D1_RECT_F& rect);

//----------------------------------------------------------------
// Serialization Functions
//----------------------------------------------------------------

void SaveCanvasState() {
    std::ofstream file(STATE_FILE, std::ios::binary | std::ios::out);
    if (!file)
        return;
    file.write(reinterpret_cast<const char*>(&gridZoomFactor), sizeof(float));
    file.write(reinterpret_cast<const char*>(&showGrid), sizeof(bool));
    file.write(reinterpret_cast<const char*>(&useAlphaGrid), sizeof(bool));
    file.write(reinterpret_cast<const char*>(&gridOpacity), sizeof(int));
    file.write(reinterpret_cast<const char*>(&currentBrushColor), sizeof(COLORREF));
    file.write(reinterpret_cast<const char*>(&brushSize), sizeof(int));
    {
        std::lock_guard<std::mutex> lock(strokeMutex);
        size_t strokeCount = strokeHistory.size();
        file.write(reinterpret_cast<const char*>(&strokeCount), sizeof(size_t));
        for (const auto& stroke : strokeHistory) {
            std::vector<DrawPoint> optimizedPoints;
            if (!stroke.points.empty()) {
                optimizedPoints.push_back(stroke.points[0]);
                for (size_t i = 1; i < stroke.points.size(); ++i) {
                    const DrawPoint& prev = optimizedPoints.back();
                    const DrawPoint& curr = stroke.points[i];
                    double dx = curr.x - prev.x;
                    double dy = curr.y - prev.y;
                    double distance = sqrt(dx * dx + dy * dy);
                    if (distance >= MIN_DISTANCE)
                        optimizedPoints.push_back(curr);
                }
            }
            size_t pointCount = optimizedPoints.size();
            file.write(reinterpret_cast<const char*>(&pointCount), sizeof(size_t));
            if (pointCount > 0)
                file.write(reinterpret_cast<const char*>(optimizedPoints.data()), pointCount * sizeof(DrawPoint));
            file.write(reinterpret_cast<const char*>(&stroke.color), sizeof(COLORREF));
            file.write(reinterpret_cast<const char*>(&stroke.brushSize), sizeof(int));
            file.write(reinterpret_cast<const char*>(&stroke.isEraser), sizeof(bool));
        }
    }
    file.close();
}

void LoadCanvasStateAsync(HWND hwnd) {
    isLoading = true;
    std::thread([hwnd]() {
        std::ifstream file(STATE_FILE, std::ios::binary | std::ios::in);
        if (!file) {
            isLoading = false;
            return;
        }
        try {
            file.read(reinterpret_cast<char*>(&gridZoomFactor), sizeof(float));
            file.read(reinterpret_cast<char*>(&showGrid), sizeof(bool));
            file.read(reinterpret_cast<char*>(&useAlphaGrid), sizeof(bool));
            file.read(reinterpret_cast<char*>(&gridOpacity), sizeof(int));
            file.read(reinterpret_cast<char*>(&currentBrushColor), sizeof(COLORREF));
            file.read(reinterpret_cast<char*>(&brushSize), sizeof(int));
            size_t strokeCount = 0;
            file.read(reinterpret_cast<char*>(&strokeCount), sizeof(size_t));
            std::vector<SerializedStroke> loadedStrokes;
            for (size_t i = 0; i < strokeCount && file.good(); ++i) {
                SerializedStroke stroke;
                size_t pointCount = 0;
                file.read(reinterpret_cast<char*>(&pointCount), sizeof(size_t));
                if (pointCount > 0 && pointCount < 1000000) {
                    for (size_t j = 0; j < pointCount; ++j) {
                        DrawPoint point;
                        file.read(reinterpret_cast<char*>(&point.x), sizeof(int));
                        file.read(reinterpret_cast<char*>(&point.y), sizeof(int));
                        file.read(reinterpret_cast<char*>(&point.timestamp), sizeof(DWORD));
                        stroke.points.push_back(point);
                    }
                    file.read(reinterpret_cast<char*>(&stroke.color), sizeof(COLORREF));
                    file.read(reinterpret_cast<char*>(&stroke.brushSize), sizeof(int));
                    file.read(reinterpret_cast<char*>(&stroke.isEraser), sizeof(bool));
                    loadedStrokes.push_back(stroke);
                }
            }
            {
                std::lock_guard<std::mutex> lock(strokeMutex);
                strokeHistory = std::move(loadedStrokes);
            }
        }
        catch (...) {
            isLoading = false;
            return;
        }
        file.close();
        isLoading = false;
        // Post a message to update offscreen buffer after loading
        PostMessage(hwnd, WM_USER + 1, 0, 0);
        }).detach();
}

//----------------------------------------------------------------
// Direct2D Initialization and Resource Management
//----------------------------------------------------------------

HRESULT CreateDeviceResources(HWND hwnd) {
    if (pRenderTarget)
        return S_OK;
    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

    HRESULT hr = pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &pRenderTarget
    );
    if (SUCCEEDED(hr)) {
        // Create an offscreen compatible render target for persistent drawing.
        hr = pRenderTarget->CreateCompatibleRenderTarget(
            D2D1::SizeF((FLOAT)rc.right, (FLOAT)rc.bottom),
            &pOffscreenRT
        );
        if (SUCCEEDED(hr)) {
            // Mark offscreen as dirty so it is initially updated.
            offscreenDirty = true;
            lastOffscreenScrollX = scrollX;
            lastOffscreenScrollY = scrollY;
        }
    }
    return hr;
}

void DiscardDeviceResources() {
    if (pOffscreenRT) {
        pOffscreenRT->Release();
        pOffscreenRT = nullptr;
    }
    if (pRenderTarget) {
        pRenderTarget->Release();
        pRenderTarget = nullptr;
    }
}

//----------------------------------------------------------------
// Drawing Functions (Direct2D versions)
//----------------------------------------------------------------

void DrawSmoothStroke(ID2D1RenderTarget* pRT, const std::vector<DrawPoint>& points, bool isEraser, COLORREF strokeColor, int strokeSize, int offsetX, int offsetY) {
    if (points.empty())
        return;

    // Determine color; for eraser use white.
    D2D1_COLOR_F color = isEraser ? D2D1::ColorF(D2D1::ColorF::White) :
        D2D1::ColorF(
            GetRValue(strokeColor) / 255.0f,
            GetGValue(strokeColor) / 255.0f,
            GetBValue(strokeColor) / 255.0f
        );

    ID2D1SolidColorBrush* pBrush = nullptr;
    if (FAILED(pRT->CreateSolidColorBrush(color, &pBrush)))
        return;

    if (points.size() == 1) {
        const DrawPoint& pt = points[0];
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            D2D1::Point2F((FLOAT)(pt.x - offsetX), (FLOAT)(pt.y - offsetY)),
            (FLOAT)strokeSize, (FLOAT)strokeSize);
        pRT->FillEllipse(ellipse, pBrush);
    }
    else {
        for (size_t i = 1; i < points.size(); ++i) {
            const DrawPoint& prev = points[i - 1];
            const DrawPoint& curr = points[i];
            double dx = curr.x - prev.x;
            double dy = curr.y - prev.y;
            double distance = sqrt(dx * dx + dy * dy);
            if (distance > 0) {
                int steps = std::max(1, (int)(distance / 2));
                for (int step = 0; step <= steps; ++step) {
                    double t = step / (double)steps;
                    int x = (int)(prev.x + dx * t);
                    int y = (int)(prev.y + dy * t);
                    D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                        D2D1::Point2F((FLOAT)(x - offsetX), (FLOAT)(y - offsetY)),
                        (FLOAT)strokeSize, (FLOAT)strokeSize);
                    pRT->FillEllipse(ellipse, pBrush);
                }
            }
        }
    }
    pBrush->Release();
}

void DrawGrid(ID2D1RenderTarget* pRT, const D2D1_RECT_F& rect) {
    ID2D1SolidColorBrush* pGridBrush = nullptr;
    HRESULT hr = pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.55f, 0.0f), &pGridBrush);
    if (FAILED(hr)) return;
    int scaledGridSize = (int)(GRID_SIZE * gridZoomFactor);

    // Compute proper mod values so grid lines appear even for negative scrolls.
    int modX = scrollX % scaledGridSize;
    if (modX < 0)
        modX += scaledGridSize;
    // Determine starting X in world coordinates.
    float startX = (float)(scrollX - modX);
    int modY = scrollY % scaledGridSize;
    if (modY < 0)
        modY += scaledGridSize;
    float startY = (float)(scrollY - modY);

    // Draw vertical gridlines covering the rect.
    for (float x = startX; x < rect.right; x += scaledGridSize) {
        pRT->DrawLine(D2D1::Point2F(x, rect.top), D2D1::Point2F(x, rect.bottom), pGridBrush, 1.0f);
    }
    // Draw horizontal gridlines covering the rect.
    for (float y = startY; y < rect.bottom; y += scaledGridSize) {
        pRT->DrawLine(D2D1::Point2F(rect.left, y), D2D1::Point2F(rect.right, y), pGridBrush, 1.0f);
    }
    pGridBrush->Release();
}

//----------------------------------------------------------------
// Offscreen Buffer Update (using pOffscreenRT)
//----------------------------------------------------------------

void UpdateOffscreenBuffer(HWND hwnd) {
    if (!pOffscreenRT)
        return;
    pOffscreenRT->BeginDraw();
    // Clear offscreen render target to white.
    pOffscreenRT->Clear(D2D1::ColorF(D2D1::ColorF::White));
    // Redraw all strokes.
    {
        std::lock_guard<std::mutex> lock(strokeMutex);
        for (const auto& stroke : strokeHistory) {
            DrawSmoothStroke(pOffscreenRT, stroke.points, stroke.isEraser, stroke.color, stroke.brushSize, scrollX, scrollY);
        }
    }
    HRESULT hr = pOffscreenRT->EndDraw();
    // Mark offscreen as clean.
    offscreenDirty = false;
    lastOffscreenScrollX = scrollX;
    lastOffscreenScrollY = scrollY;
}

//----------------------------------------------------------------
// Status Bar Functions (GDI remains unchanged)
//----------------------------------------------------------------

void InitializeStatusBuffer(HWND hStatus) {
    if (hStatusBufferDC) {
        DeleteDC(hStatusBufferDC);
        DeleteObject(hStatusBufferBitmap);
    }
    HDC hdc = GetDC(hStatus);
    RECT rect;
    GetClientRect(hStatus, &rect);
    hStatusBufferDC = CreateCompatibleDC(hdc);
    hStatusBufferBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    SelectObject(hStatusBufferDC, hStatusBufferBitmap);
    ReleaseDC(hStatus, hdc);
}

void UpdateStatus(HWND hwnd) {
    wchar_t status[512];
    BYTE r = GetRValue(currentBrushColor);
    BYTE g = GetGValue(currentBrushColor);
    BYTE b = GetBValue(currentBrushColor);
    swprintf_s(status, 512,
        L"Mode: %s | Brush: %d | Color: RGB(%d,%d,%d) | Grid: %s | Zoom: %.1fx | Canvas Pos: (%d,%d)",
        isEraserMode ? L"Eraser" : L"Draw",
        brushSize,
        r, g, b,
        showGrid ? L"On" : L"Off",
        gridZoomFactor,
        scrollX, scrollY
    );
    g_statusText = status;
}

/* void UpdateStatus(HWND hwnd) {
    DWORD currentTime = GetTickCount();
    if (currentTime - lastStatusUpdateTime < STATUS_UPDATE_INTERVAL)
        return;
    lastStatusUpdateTime = currentTime;
    if (!hStatusBar)
        return;
    if (!hStatusBufferDC) {
        InitializeStatusBuffer(hStatusBar);
    }
    RECT statusRect;
    GetClientRect(hStatusBar, &statusRect);
    wchar_t status[512];
    BYTE r = GetRValue(currentBrushColor);
    BYTE g = GetGValue(currentBrushColor);
    BYTE b = GetBValue(currentBrushColor);
    swprintf_s(status, 512,
        L"Mode: %s | Brush: %d | Color: RGB(%d,%d,%d) | Grid: %s%s | Zoom: %.1fx | Opacity: %d%% | Canvas Pos: (%d,%d)",
        isEraserMode ? L"Eraser" : L"Draw",
        brushSize,
        r, g, b,
        showGrid ? L"On" : L"Off",
        useAlphaGrid ? L"(Alpha)" : L"",
        gridZoomFactor,
        (gridOpacity * 100) / 255,
        scrollX, scrollY
    );
    SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)status);
} */

//----------------------------------------------------------------
// Window Procedure
//----------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    HRESULT hr;
    switch (uMsg) {
    case WM_CREATE:
    {
        // Initialize Direct2D Factory
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &pFactory);
        if (FAILED(hr))
            return -1;

        // Initialize DirectWrite Factory and Text Format for the status text.
        HRESULT hrDWrite = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&pDWriteFactory)
        );
        if (SUCCEEDED(hrDWrite))
        {
            hrDWrite = pDWriteFactory->CreateTextFormat(
                L"Segoe UI",                // Font family name.
                NULL,                       // Use system font collection.
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                14.0f,                      // Font size.
                L"",                        // Locale.
                &pTextFormat
            );
            if (SUCCEEDED(hrDWrite))
            {
                pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
        }

        // (Remove GDI status bar creation; status will be rendered via Direct2D.)

        // Device resources (pRenderTarget and pOffscreenRT) will be created in WM_SIZE.
        LoadCanvasStateAsync(hwnd);
        return 0;
    }
    case WM_SIZE:
    {
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // Resize (or create) the main render target.
        if (pRenderTarget)
        {
            pRenderTarget->Resize(D2D1::SizeU(rcClient.right, rcClient.bottom));
        }
        else
        {
            HRESULT hr = CreateDeviceResources(hwnd);
            if (FAILED(hr))
                return -1;
        }

        // Recreate the offscreen render target.
        if (pOffscreenRT)
        {
            pOffscreenRT->Release();
            pOffscreenRT = nullptr;
        }
        HRESULT hr = pRenderTarget->CreateCompatibleRenderTarget(
            D2D1::SizeF((FLOAT)rcClient.right, (FLOAT)rcClient.bottom),
            &pOffscreenRT
        );
        if (SUCCEEDED(hr))
        {
            offscreenDirty = true;               // Force update of the offscreen buffer.
            lastOffscreenScrollX = scrollX;
            lastOffscreenScrollY = scrollY;
            UpdateOffscreenBuffer(hwnd);         // Rebuild the offscreen content.
        }

        // Update status (which now contains the grid state) and force a full redraw.
        UpdateStatus(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
        case WM_KEYDOWN:
    {
            if (GetKeyState(VK_MENU) & 0x8000)
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
            if (wParam == VK_LEFT) {
                scrollX -= 200; //default scroll for all arrowkeys = 20
                g_viewTransform = D2D1::Matrix3x2F::Translation((FLOAT)-scrollX, (FLOAT)-scrollY);
                UpdateStatus(hwnd);           // <-- Added this line
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (wParam == VK_RIGHT) {
                scrollX += 200;
                g_viewTransform = D2D1::Matrix3x2F::Translation((FLOAT)-scrollX, (FLOAT)-scrollY);
                UpdateStatus(hwnd);           // <-- Added this line
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (wParam == VK_UP) {
                scrollY -= 200;
                g_viewTransform = D2D1::Matrix3x2F::Translation((FLOAT)-scrollX, (FLOAT)-scrollY);
                UpdateStatus(hwnd);           // <-- Added this line
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else if (wParam == VK_DOWN) {
                scrollY += 200;
                g_viewTransform = D2D1::Matrix3x2F::Translation((FLOAT)-scrollX, (FLOAT)-scrollY);
                UpdateStatus(hwnd);           // <-- Added this line
                InvalidateRect(hwnd, NULL, FALSE);
            }
        else if (wParam == VK_SPACE && !isSpacePressed) {
            isSpacePressed = true;
            GetCursorPos(&lastMousePos);
            ScreenToClient(hwnd, &lastMousePos);
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            SetCapture(hwnd);
        }
        else if (wParam == 0x50) {
            isPaintbrushSelected = true;
            isEraserMode = false;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 0x45) {
            isPaintbrushSelected = false;
            isEraserMode = true;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 'Q') {
            CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
            static COLORREF customColors[16] = { 0 };
            cc.hwndOwner = hwnd;
            cc.rgbResult = currentBrushColor;
            cc.lpCustColors = customColors;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc))
                currentBrushColor = cc.rgbResult;
            UpdateStatus(hwnd);
            offscreenDirty = true;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == VK_ADD || wParam == VK_OEM_PLUS) {
            brushSize = std::min(50, brushSize + 5);
            offscreenDirty = true;  // Ensure new brush size is applied in drawing.
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == VK_SUBTRACT || wParam == VK_OEM_MINUS) {
            brushSize = std::max(5, brushSize - 5);
            offscreenDirty = true;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 0x43) {
            std::lock_guard<std::mutex> lock(strokeMutex);
            strokeHistory.clear();
            sessionDirty = true;  // Mark session as changed.
            offscreenDirty = true;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == VK_HOME) {
                scrollX = 0;
                scrollY = 0;
                g_viewTransform = D2D1::Matrix3x2F::Translation(0.0f, 0.0f);
                UpdateStatus(hwnd);  // <-- Add this line to update the status bar.
                InvalidateRect(hwnd, NULL, TRUE);
            }
        else if (wParam == 'G') {
            showGrid = !showGrid;
            offscreenDirty = true;  // Mark offscreen dirty so grid redraws.
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 'A') {
            useAlphaGrid = !useAlphaGrid;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_PRIOR) {
            gridZoomFactor *= 1.1f;
            offscreenDirty = true;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_NEXT) {
            gridZoomFactor *= 0.9f;
            if (gridZoomFactor < 0.1f)
                gridZoomFactor = 0.1f;
            offscreenDirty = true;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_OEM_6 && useAlphaGrid) {
            gridOpacity = std::min(255, gridOpacity + 15);
            offscreenDirty = true;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_OEM_4 && useAlphaGrid) {
            gridOpacity = std::max(0, gridOpacity - 15);
            offscreenDirty = true;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_ESCAPE) {
            if (isSpacePressed) {
                isSpacePressed = false;
                ReleaseCapture();
            }
            if (sessionDirty) {
                SaveCanvasState();
                sessionDirty = false;
            }
            PostQuitMessage(0);
            return 0;
        }
        else if (wParam == VK_F1) {
            MessageBox(hwnd,
                L"Infinite Canvas Doodle App (Direct2D Accelerated)\n"
                L"P=Brush, E=Eraser, C=Clear, +/-=BrushSize, Space+Drag/Arrow Keys=Panning, Home=Reset, Q=Color, G=Grid, A=Alpha, PgUp=ZoomIn, PgDn=ZoomOut, F1=About || Dream Come True! (877 lines of code) by Entisoft Software (c) Evans Thorpemorton Pen=24,123,205",
                L"Information", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        return 0;
    }
    case WM_KEYUP:
    {
        if (wParam == VK_SPACE) {
            isSpacePressed = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            ReleaseCapture();
            return 0;
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        isDrawing = true;
        currentStrokeBrushSize = brushSize;
        int worldX = GET_X_LPARAM(lParam) + scrollX;
        int worldY = GET_Y_LPARAM(lParam) + scrollY;
        strokeBuffer.clear();
        strokeBuffer.push_back(DrawPoint(worldX, worldY));
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP:
    {
        if (isDrawing) {
            isDrawing = false;
            SerializedStroke stroke;
            stroke.points = strokeBuffer;
            stroke.color = currentBrushColor;
            stroke.brushSize = brushSize;
            stroke.isEraser = isEraserMode;
            {
                std::lock_guard<std::mutex> lock(strokeMutex);
                strokeHistory.push_back(stroke);
            }
            strokeBuffer.clear();
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
            sessionDirty = true;
            if (sessionDirty) {
                SaveCanvasState();
                sessionDirty = false;
            }
            offscreenDirty = true;
            UpdateOffscreenBuffer(hwnd);
            UpdateStatus(hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (isSpacePressed) {
            int deltaX = x - lastMousePos.x;
            int deltaY = y - lastMousePos.y;
            scrollX -= deltaX;
            scrollY -= deltaY;
            lastMousePos.x = x;
            lastMousePos.y = y;
            // Update the global view transform.
            g_viewTransform = D2D1::Matrix3x2F::Translation((FLOAT)-scrollX, (FLOAT)-scrollY);
            UpdateStatus(hwnd);  // <-- Added: update status after panning.
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (isDrawing && (wParam & MK_LBUTTON)) {
            int worldX = x + scrollX;
            int worldY = y + scrollY;
            if (strokeBuffer.empty())
                strokeBuffer.push_back(DrawPoint(worldX, worldY));
            else {
                const DrawPoint& lastPt = strokeBuffer.back();
                double dx = worldX - lastPt.x;
                double dy = worldY - lastPt.y;
                double distance = sqrt(dx * dx + dy * dy);
                if (distance >= MIN_DISTANCE)
                    strokeBuffer.push_back(DrawPoint(worldX, worldY));
            }
            RECT dirty;
            int clientPrevX = strokeBuffer.back().x - scrollX;
            int clientPrevY = strokeBuffer.back().y - scrollY;
            int clientNewX = x;
            int clientNewY = y;
            dirty.left = std::min(clientPrevX, clientNewX) - brushSize;
            dirty.top = std::min(clientPrevY, clientNewY) - brushSize;
            dirty.right = std::max(clientPrevX, clientNewX) + brushSize;
            dirty.bottom = std::max(clientPrevY, clientNewY) + brushSize;
            InvalidateRect(hwnd, &dirty, FALSE);
        }
        return 0;
    }
    case WM_USER + 1:
    {
        // Custom message after state loading.
        offscreenDirty = true;
        UpdateOffscreenBuffer(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);

        pRenderTarget->BeginDraw();

        // Clear to white.
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        // Set transform for panning (world coordinates).
        pRenderTarget->SetTransform(g_viewTransform);

        // Draw all strokes in world coordinates.
        {
            std::lock_guard<std::mutex> lock(strokeMutex);
            for (const auto& stroke : strokeHistory) {
                DrawSmoothStroke(pRenderTarget, stroke.points, stroke.isEraser, stroke.color, stroke.brushSize, 0, 0);
            }
        }
        if (isDrawing && !strokeBuffer.empty()) {
            DrawSmoothStroke(pRenderTarget, strokeBuffer, isEraserMode, currentBrushColor, currentStrokeBrushSize, 0, 0);
        }

        // Draw grid in world coordinates.
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        D2D1_RECT_F worldRect = D2D1::RectF((FLOAT)scrollX, (FLOAT)scrollY,
            (FLOAT)(scrollX + rcClient.right), (FLOAT)(scrollY + rcClient.bottom));
        if (showGrid) {
            DrawGrid(pRenderTarget, worldRect);
        }

        // Reset transform to identity for UI.
        pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

        // Draw status bar in screen coordinates.
        D2D1_RECT_F statusRect = D2D1::RectF(0, (FLOAT)rcClient.bottom - 30.0f, (FLOAT)rcClient.right, (FLOAT)rcClient.bottom);
        ID2D1SolidColorBrush* pStatusBgBrush = nullptr;
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f), &pStatusBgBrush);
        pRenderTarget->FillRectangle(statusRect, pStatusBgBrush);
        pStatusBgBrush->Release();
        ID2D1SolidColorBrush* pTextBrush = nullptr;
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pTextBrush);
        pRenderTarget->DrawTextW(
            g_statusText.c_str(),
            static_cast<UINT32>(g_statusText.length()),
            pTextFormat,
            &statusRect,
            pTextBrush,
            D2D1_DRAW_TEXT_OPTIONS_NONE,
            DWRITE_MEASURING_MODE_NATURAL
        );
        pTextBrush->Release();

        HRESULT hr = pRenderTarget->EndDraw();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT) {
            if (isSpacePressed) {
                SetCursor(LoadCursor(NULL, IDC_SIZEALL));
                return TRUE;
            }
            else if (isPaintbrushSelected || isEraserMode) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
                return TRUE;
            }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    case WM_DESTROY:
    {
        if (sessionDirty)
        {
            SaveCanvasState();
            sessionDirty = false;
        }
        DiscardDeviceResources();
        if (pFactory)
        {
            pFactory->Release();
            pFactory = nullptr;
        }
        if (pTextFormat)
        {
            pTextFormat->Release();
            pTextFormat = nullptr;
        }
        if (pDWriteFactory)
        {
            pDWriteFactory->Release();
            pDWriteFactory = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

//----------------------------------------------------------------
// WinMain
//----------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);
    const wchar_t CLASS_NAME[] = L"InfiniteCanvasClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClass(&wc);
    hInst = hInstance;
    hWnd = CreateWindowEx(0, CLASS_NAME,
        L"Infinite Canvas Doodle App (Direct2D Accelerated, P=Brush, E=Eraser, C=Clear, +/-=BrushSize, Space+Drag/Arrow=Panning, Home=Reset, Q=Color, G=Grid, A=Alpha, PgUp=ZoomIn, PgDn=ZoomOut, F1=About)",
        WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);
    if (hWnd == NULL)
        return 0;
    // Enable double buffering via WS_EX_COMPOSITED.
    SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_COMPOSITED);
    ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
