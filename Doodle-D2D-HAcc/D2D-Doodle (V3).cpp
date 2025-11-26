#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <cmath>
#include <vector>
#include <mutex>
#include <fstream>
#include <thread>
#include <algorithm>
#include <tuple>
#include "resource.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwrite.lib")

//----------------------------------------------------------------
// Data Structures and Globals
//----------------------------------------------------------------

struct DrawPoint {
    int64_t x, y;
    DWORD timestamp;
    // Note: Struct size is likely 24 bytes due to alignment padding (8+8+4 + 4 padding)
    DrawPoint() : x(0), y(0), timestamp(0) {}
    DrawPoint(int64_t px, int64_t py) : x(px), y(py), timestamp(GetTickCount()) {}
};

struct SerializedStroke {
    std::vector<DrawPoint> points;
    COLORREF color;
    int brushSize;
    bool isEraser;
};

// Thread Safety
std::mutex strokeMutex;
std::mutex fileMutex;

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

int64_t scrollX = 0;
int64_t scrollY = 0;

float gridZoomFactor = 1.0f;
bool showGrid = true;
bool useAlphaGrid = false;
int gridOpacity = 255;
const int GRID_SIZE = 100;

HINSTANCE hInst;
HWND hWnd;

IDWriteFactory* pDWriteFactory = nullptr;
IDWriteTextFormat* pTextFormat = nullptr;
std::wstring g_statusText = L"";

HDC hStatusBufferDC = NULL;
HBITMAP hStatusBufferBitmap = NULL;

// Serialization
const wchar_t* STATE_FILE = L"canvas_state_v4.bin"; // New file for version 4
bool isLoading = false;
bool isSessionLoaded = false; // Safety flag to prevent wipe-on-launch
bool sessionDirty = false;

// Direct2D
ID2D1Factory* pFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1BitmapRenderTarget* pOffscreenRT = nullptr;
bool offscreenDirty = true;
int64_t lastOffscreenScrollX = -1;
int64_t lastOffscreenScrollY = -1;

//----------------------------------------------------------------
// Function Declarations
//----------------------------------------------------------------

void SaveCanvasState();
void LoadCanvasStateAsync(HWND hwnd);
void UpdateStatus(HWND hwnd);
void UpdateOffscreenBuffer(HWND hwnd);
HRESULT CreateDeviceResources(HWND hwnd);
void DiscardDeviceResources();
void DrawSmoothStroke(ID2D1RenderTarget* pRT, const std::vector<DrawPoint>& points, bool isEraser, COLORREF strokeColor, int strokeSize, int64_t offsetX, int64_t offsetY);
void DrawGrid(ID2D1RenderTarget* pRT, const D2D1_RECT_F& rect);

//----------------------------------------------------------------
// Serialization Functions
//----------------------------------------------------------------

void SaveCanvasState() {
    // Critical Safety: Do not overwrite the file if we haven't finished loading the previous state yet.
    // This prevents an "Empty File Wipe" if the user closes the app immediately after launch.
    if (!isSessionLoaded) return;

    std::lock_guard<std::mutex> fileLock(fileMutex);
    std::ofstream file(STATE_FILE, std::ios::binary);
    if (!file) return;

    const uint32_t FILE_VERSION = 5;
    file.write(reinterpret_cast<const char*>(&FILE_VERSION), sizeof(FILE_VERSION));

    file.write(reinterpret_cast<const char*>(&gridZoomFactor), sizeof(float));
    file.write(reinterpret_cast<const char*>(&showGrid), sizeof(bool));
    file.write(reinterpret_cast<const char*>(&useAlphaGrid), sizeof(bool));
    file.write(reinterpret_cast<const char*>(&gridOpacity), sizeof(int));
    file.write(reinterpret_cast<const char*>(&currentBrushColor), sizeof(COLORREF));
    file.write(reinterpret_cast<const char*>(&brushSize), sizeof(int));
    file.write(reinterpret_cast<const char*>(&scrollX), sizeof(int64_t));
    file.write(reinterpret_cast<const char*>(&scrollY), sizeof(int64_t));

    std::lock_guard<std::mutex> lock(strokeMutex);
    size_t count = strokeHistory.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& stroke : strokeHistory) {
        // Optimization: Simplify path before saving
        std::vector<DrawPoint> opt;
        if (!stroke.points.empty()) {
            opt.push_back(stroke.points[0]);
            for (size_t i = 1; i < stroke.points.size(); ++i) {
                int64_t dx = stroke.points[i].x - opt.back().x;
                int64_t dy = stroke.points[i].y - opt.back().y;
                if (sqrt(static_cast<double>(dx * dx + dy * dy)) >= MIN_DISTANCE)
                    opt.push_back(stroke.points[i]);
            }
        }

        size_t pc = opt.size();
        file.write(reinterpret_cast<const char*>(&pc), sizeof(pc));

        // Write Raw Structs (Includes Alignment Padding!)
        if (pc > 0)
            file.write(reinterpret_cast<const char*>(opt.data()), pc * sizeof(DrawPoint));

        file.write(reinterpret_cast<const char*>(&stroke.color), sizeof(COLORREF));
        file.write(reinterpret_cast<const char*>(&stroke.brushSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&stroke.isEraser), sizeof(bool));
    }
}

void LoadCanvasStateAsync(HWND hwnd) {
    if (isLoading) return;
    isLoading = true;

    std::thread([hwnd]() {
        std::lock_guard<std::mutex> fileLock(fileMutex);
        std::ifstream file(STATE_FILE, std::ios::binary);

        // Default state if file missing/bad
        bool success = false;
        std::vector<SerializedStroke> strokes;
        float zoom = 1.0f;
        bool show_grid = true, alpha_grid = false;
        int opacity = 255;
        COLORREF color = RGB(24, 123, 205);
        int brush = 10;
        int64_t sx = 0, sy = 0;

        if (file.is_open()) {
            uint32_t version = 0;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));

            if (file && version == 5) {
                // Verify we are reading these fields correctly for Version 3 (or 4 if updated)
                file.read(reinterpret_cast<char*>(&zoom), sizeof(float));
                file.read(reinterpret_cast<char*>(&show_grid), sizeof(bool));
                file.read(reinterpret_cast<char*>(&alpha_grid), sizeof(bool));
                file.read(reinterpret_cast<char*>(&opacity), sizeof(int));
                file.read(reinterpret_cast<char*>(&color), sizeof(COLORREF));
                file.read(reinterpret_cast<char*>(&brush), sizeof(int));
                file.read(reinterpret_cast<char*>(&sx), sizeof(int64_t));
                file.read(reinterpret_cast<char*>(&sy), sizeof(int64_t));

                size_t count = 0;
                file.read(reinterpret_cast<char*>(&count), sizeof(count));

                if (file && count < 500000) {
                    strokes.reserve(count);
                    bool error = false;
                    for (size_t i = 0; i < count && file; ++i) {
                        SerializedStroke s;
                        size_t pc = 0;
                        file.read(reinterpret_cast<char*>(&pc), sizeof(pc));

                        if (!file || pc > 1000000) { error = true; break; }

                        s.points.reserve(pc);
                        for (size_t j = 0; j < pc; ++j) {
                            DrawPoint pt;
                            // FIX: Read exact sizeof(DrawPoint) to handle padding correctly
                            file.read(reinterpret_cast<char*>(&pt), sizeof(DrawPoint));
                            if (!file) { error = true; break; }
                            s.points.push_back(pt);
                        }
                        if (error) break;

                        file.read(reinterpret_cast<char*>(&s.color), sizeof(COLORREF));
                        file.read(reinterpret_cast<char*>(&s.brushSize), sizeof(int));
                        file.read(reinterpret_cast<char*>(&s.isEraser), sizeof(bool));
                        if (file) strokes.push_back(std::move(s));
                    }
                    if (!error && file) success = true;
                }
            }
            file.close();
        }
        else {
            // If file doesn't exist, we consider it "loaded" as empty
            success = true;
        }

        if (IsWindow(hwnd)) {
            auto* data = new std::tuple<
                float, bool, bool, int, COLORREF, int, int64_t, int64_t, std::vector<SerializedStroke>, bool
            >(zoom, show_grid, alpha_grid, opacity, color, brush, sx, sy, std::move(strokes), success);

            PostMessage(hwnd, WM_USER + 1, reinterpret_cast<WPARAM>(data), 0);
        }

        isLoading = false;
        }).detach();
}

//----------------------------------------------------------------
// Direct2D
//----------------------------------------------------------------

HRESULT CreateDeviceResources(HWND hwnd) {
    if (pRenderTarget) return S_OK;
    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

    HRESULT hr = pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &pRenderTarget
    );
    if (SUCCEEDED(hr)) {
        hr = pRenderTarget->CreateCompatibleRenderTarget(
            D2D1::SizeF((FLOAT)rc.right, (FLOAT)rc.bottom),
            &pOffscreenRT
        );
        if (SUCCEEDED(hr)) {
            offscreenDirty = true;
            lastOffscreenScrollX = -1;
            lastOffscreenScrollY = -1;
        }
    }
    return hr;
}

void DiscardDeviceResources() {
    if (pOffscreenRT) { pOffscreenRT->Release(); pOffscreenRT = nullptr; }
    if (pRenderTarget) { pRenderTarget->Release(); pRenderTarget = nullptr; }
}

//----------------------------------------------------------------
// Drawing
//----------------------------------------------------------------

void DrawSmoothStroke(ID2D1RenderTarget* pRT, const std::vector<DrawPoint>& points, bool isEraser, COLORREF strokeColor, int strokeSize, int64_t offsetX, int64_t offsetY) {
    if (points.empty()) return;

    D2D1_COLOR_F color = isEraser ? D2D1::ColorF(D2D1::ColorF::White) :
        D2D1::ColorF(GetRValue(strokeColor) / 255.0f, GetGValue(strokeColor) / 255.0f, GetBValue(strokeColor) / 255.0f);

    ID2D1SolidColorBrush* pBrush = nullptr;
    if (FAILED(pRT->CreateSolidColorBrush(color, &pBrush))) return;

    if (points.size() == 1) {
        const DrawPoint& pt = points[0];
        // Offset calculation is done in int64 BEFORE float conversion to preserve precision
        float x = (float)(pt.x - offsetX);
        float y = (float)(pt.y - offsetY);
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), (FLOAT)strokeSize, (FLOAT)strokeSize), pBrush);
    }
    else {
        for (size_t i = 1; i < points.size(); ++i) {
            const DrawPoint& prev = points[i - 1];
            const DrawPoint& curr = points[i];
            int64_t dx = curr.x - prev.x;
            int64_t dy = curr.y - prev.y;
            double distance = sqrt(static_cast<double>(dx * dx + dy * dy));
            if (distance > 0) {
                int steps = std::max(1, (int)(distance / 2));
                for (int step = 0; step <= steps; ++step) {
                    double t = step / (double)steps;
                    // Subtract Scroll Offset here
                    float x = (float)((prev.x - offsetX) + dx * t);
                    float y = (float)((prev.y - offsetY) + dy * t);
                    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), (FLOAT)strokeSize, (FLOAT)strokeSize), pBrush);
                }
            }
        }
    }
    pBrush->Release();
}

void DrawGrid(ID2D1RenderTarget* pRT, const D2D1_RECT_F& rect) {
    ID2D1SolidColorBrush* pGridBrush = nullptr;
    // Check if Alpha mode is on. If so, calculate alpha (0.0 to 1.0), otherwise use 1.0 (Opaque)
    float alpha = useAlphaGrid ? (gridOpacity / 255.0f) : 1.0f;
    HRESULT hr = pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.55f, 0.0f, alpha), &pGridBrush);
    if (FAILED(hr)) return;
    float scaledGridSize = GRID_SIZE * gridZoomFactor;

    int64_t modX = scrollX % (int64_t)scaledGridSize;
    if (modX < 0) modX += (int64_t)scaledGridSize;

    int64_t modY = scrollY % (int64_t)scaledGridSize;
    if (modY < 0) modY += (int64_t)scaledGridSize;

    float startX = -(float)modX;
    float startY = -(float)modY;

    for (float x = startX; x < rect.right; x += scaledGridSize) {
        pRT->DrawLine(D2D1::Point2F(x, rect.top), D2D1::Point2F(x, rect.bottom), pGridBrush, 1.0f);
    }
    for (float y = startY; y < rect.bottom; y += scaledGridSize) {
        pRT->DrawLine(D2D1::Point2F(rect.left, y), D2D1::Point2F(rect.right, y), pGridBrush, 1.0f);
    }
    pGridBrush->Release();
}

void UpdateOffscreenBuffer(HWND hwnd) {
    if (!pOffscreenRT) return;

    bool forceRebuild = offscreenDirty || (scrollX != lastOffscreenScrollX) || (scrollY != lastOffscreenScrollY);
    if (!forceRebuild) return;

    pOffscreenRT->BeginDraw();
    pOffscreenRT->Clear(D2D1::ColorF(D2D1::ColorF::White));

    {
        std::lock_guard<std::mutex> lock(strokeMutex);
        for (const auto& stroke : strokeHistory) {
            DrawSmoothStroke(pOffscreenRT, stroke.points, stroke.isEraser, stroke.color, stroke.brushSize, scrollX, scrollY);
        }
    }

    pOffscreenRT->EndDraw();

    lastOffscreenScrollX = scrollX;
    lastOffscreenScrollY = scrollY;
    offscreenDirty = false;
}

void UpdateStatus(HWND hwnd) {
    wchar_t status[512];
    BYTE r = GetRValue(currentBrushColor);
    BYTE g = GetGValue(currentBrushColor);
    BYTE b = GetBValue(currentBrushColor);

    // Logic for Grid Text
    const wchar_t* gridText = L": Off";
    if (showGrid) {
        gridText = useAlphaGrid ? L"(Alpha): On" : L": On";
    }

    // Calculate Opacity Percentage (0-100)
    int opacityPercent = (int)((gridOpacity / 255.0f) * 100.0f);

    // Logic for Opacity Text (Only show if Grid is On)
    wchar_t opacityString[32] = L"";
    if (showGrid) {
        swprintf_s(opacityString, 32, L" | Opacity: %d%%", opacityPercent);
    }

    swprintf_s(status, 512,
        L"Mode: %s | Brush: %d | Color: RGB(%d,%d,%d) | Grid%s | Zoom: %.1fx%s | Canvas Pos: (%lld,%lld)",
        isEraserMode ? L"Eraser" : L"Draw",
        brushSize, r, g, b,
        gridText,
        gridZoomFactor,
        opacityString, // Insert the conditional string here
        scrollX, scrollY
    );
    g_statusText = status;
}

//----------------------------------------------------------------
// Window Procedure
//----------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
    {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &pFactory);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&pDWriteFactory);
        if (pDWriteFactory) {
            pDWriteFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &pTextFormat);
            if (pTextFormat) {
                pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
        }
        LoadCanvasStateAsync(hwnd);
        return 0;
    }
    case WM_SIZE:
    {
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        if (pRenderTarget) pRenderTarget->Resize(D2D1::SizeU(rcClient.right, rcClient.bottom));
        else CreateDeviceResources(hwnd);

        if (pOffscreenRT) { pOffscreenRT->Release(); pOffscreenRT = nullptr; }
        if (pRenderTarget) {
            pRenderTarget->CreateCompatibleRenderTarget(D2D1::SizeF((FLOAT)rcClient.right, (FLOAT)rcClient.bottom), &pOffscreenRT);
            offscreenDirty = true;
            UpdateOffscreenBuffer(hwnd);
        }
        UpdateStatus(hwnd);
        return 0;
    }
    case WM_KEYDOWN:
    {
        if (GetKeyState(VK_MENU) & 0x8000) return DefWindowProc(hwnd, uMsg, wParam, lParam);
        bool viewChanged = false;
        if (wParam == VK_LEFT) { scrollX -= 200; viewChanged = true; }
        else if (wParam == VK_RIGHT) { scrollX += 200; viewChanged = true; }
        else if (wParam == VK_UP) { scrollY -= 200; viewChanged = true; }
        else if (wParam == VK_DOWN) { scrollY += 200; viewChanged = true; }
        else if (wParam == VK_HOME) { scrollX = 0; scrollY = 0; viewChanged = true; }

        if (viewChanged) {
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_SPACE && !isSpacePressed) {
            isSpacePressed = true;
            GetCursorPos(&lastMousePos);
            ScreenToClient(hwnd, &lastMousePos);
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            SetCapture(hwnd);
        }
        else if (wParam == 0x50) { isPaintbrushSelected = true; isEraserMode = false; UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        else if (wParam == 0x45) { isPaintbrushSelected = false; isEraserMode = true; UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        else if (wParam == 'Q') {
            CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
            static COLORREF customColors[16] = { 0 };
            cc.hwndOwner = hwnd;
            cc.rgbResult = currentBrushColor;
            cc.lpCustColors = customColors;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) currentBrushColor = cc.rgbResult;
            UpdateStatus(hwnd);
        }
        else if (wParam == VK_ADD || wParam == VK_OEM_PLUS) { brushSize = std::min(50, brushSize + 5); UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        else if (wParam == VK_SUBTRACT || wParam == VK_OEM_MINUS) { brushSize = std::max(5, brushSize - 5); UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        else if (wParam == 0x43) {
            std::lock_guard<std::mutex> lock(strokeMutex);
            strokeHistory.clear();
            sessionDirty = true;
            offscreenDirty = true;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wParam == 'G') { showGrid = !showGrid; offscreenDirty = true; UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, TRUE); }
        else if (wParam == 'A') {
            // Only toggle Alpha if Grid is actually ON
            if (showGrid) {
                useAlphaGrid = !useAlphaGrid;
                UpdateStatus(hwnd);
                InvalidateRect(hwnd, NULL, FALSE); // Repaint to show alpha change
            }
        }
        else if (wParam == VK_PRIOR) { gridZoomFactor *= 1.1f; offscreenDirty = true; UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        else if (wParam == VK_NEXT) { gridZoomFactor = std::max(0.1f, gridZoomFactor * 0.9f); offscreenDirty = true; UpdateStatus(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        // Opacity Controls: [ (VK_OEM_4) decreases, ] (VK_OEM_6) increases
        else if (wParam == VK_OEM_4) { // '[' key
            if (showGrid) { // Only allow change if Grid is On
                gridOpacity = std::max(0, gridOpacity - 15);
                UpdateStatus(hwnd);
                if (useAlphaGrid) InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (wParam == VK_OEM_6) { // ']' key
            if (showGrid) { // Only allow change if Grid is On
                gridOpacity = std::min(255, gridOpacity + 15);
                UpdateStatus(hwnd);
                if (useAlphaGrid) InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (wParam == VK_ESCAPE) { PostQuitMessage(0); return 0; }
        else if (wParam == VK_F1) {
            MessageBox(hwnd, TEXT("I made an Infinite Canvas app using Direct2D True 64Bit /w Hardware Acceleration, no need for bloated Godot Engine/ Frameworks or M$ Infinite Canvas Control! Eternity of effort paid off! (713 lines of code) by Entisoft Software (c) Evans Thorpemorton pen=24,123,205\n(P=Brush E=Eraser C=Clear +-=BrushSize Space+Drag=Scroll Home=Center Q=Color G=Grid A=AlphaTransparency []=Opacity PgUp = ZoomIn PgDown = ZoomOut F1=About)"), TEXT("Information"), MB_OK | MB_ICONINFORMATION);
        } //674lines->713 

        return 0;
    }
    case WM_KEYUP:
    {
        if (wParam == VK_SPACE) {
            isSpacePressed = false;
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            ReleaseCapture();
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        isDrawing = true;
        currentStrokeBrushSize = brushSize;
        int64_t worldX = (int64_t)GET_X_LPARAM(lParam) + scrollX;
        int64_t worldY = (int64_t)GET_Y_LPARAM(lParam) + scrollY;
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
            sessionDirty = true;

            SaveCanvasState();

            offscreenDirty = true;
            UpdateOffscreenBuffer(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        int64_t clientX = (int64_t)GET_X_LPARAM(lParam);
        int64_t clientY = (int64_t)GET_Y_LPARAM(lParam);

        if (isSpacePressed) {
            int64_t deltaX = clientX - lastMousePos.x;
            int64_t deltaY = clientY - lastMousePos.y;
            scrollX -= deltaX;
            scrollY -= deltaY;
            lastMousePos.x = (LONG)clientX;
            lastMousePos.y = (LONG)clientY;
            UpdateStatus(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (isDrawing && (wParam & MK_LBUTTON)) {
            int64_t worldX = clientX + scrollX;
            int64_t worldY = clientY + scrollY;
            if (strokeBuffer.empty() ||
                (pow(worldX - strokeBuffer.back().x, 2) + pow(worldY - strokeBuffer.back().y, 2) >= MIN_DISTANCE * MIN_DISTANCE)) {
                strokeBuffer.push_back(DrawPoint(worldX, worldY));
            }
            RECT dirty;
            LONG clientPrevX = (LONG)(strokeBuffer.back().x - scrollX);
            LONG clientPrevY = (LONG)(strokeBuffer.back().y - scrollY);
            dirty.left = std::min((LONG)clientPrevX, (LONG)clientX) - brushSize - 10;
            dirty.top = std::min((LONG)clientPrevY, (LONG)clientY) - brushSize - 10;
            dirty.right = std::max((LONG)clientPrevX, (LONG)clientX) + brushSize + 10;
            dirty.bottom = std::max((LONG)clientPrevY, (LONG)clientY) + brushSize + 10;
            InvalidateRect(hwnd, &dirty, FALSE);
        }
        return 0;
    }
    case WM_USER + 1: // Message from Load Thread
    {
        if (wParam) {
            auto* data = reinterpret_cast<std::tuple<float, bool, bool, int, COLORREF, int, int64_t, int64_t, std::vector<SerializedStroke>, bool>*>(wParam);

            // Only update state if load was successful
            if (std::get<9>(*data)) {
                gridZoomFactor = std::get<0>(*data);
                showGrid = std::get<1>(*data);
                useAlphaGrid = std::get<2>(*data);
                gridOpacity = std::get<3>(*data);
                currentBrushColor = std::get<4>(*data);
                brushSize = std::get<5>(*data);
                scrollX = std::get<6>(*data);
                scrollY = std::get<7>(*data);

                {
                    std::lock_guard<std::mutex> lock(strokeMutex);
                    strokeHistory = std::move(std::get<8>(*data));
                }
            }
            // Flag that we are safe to save now (state is fully initialized)
            isSessionLoaded = true;

            delete data;
        }

        offscreenDirty = true;
        lastOffscreenScrollX = -1;
        UpdateOffscreenBuffer(hwnd);
        UpdateStatus(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

        UpdateOffscreenBuffer(hwnd);
        if (pOffscreenRT) {
            ID2D1Bitmap* pBitmap = nullptr;
            pOffscreenRT->GetBitmap(&pBitmap);
            if (pBitmap) {
                RECT rc; GetClientRect(hwnd, &rc);
                pRenderTarget->DrawBitmap(pBitmap, D2D1::RectF(0, 0, (float)rc.right, (float)rc.bottom));
                pBitmap->Release();
            }
        }
        else {
            std::lock_guard<std::mutex> lock(strokeMutex);
            for (const auto& stroke : strokeHistory) {
                DrawSmoothStroke(pRenderTarget, stroke.points, stroke.isEraser, stroke.color, stroke.brushSize, scrollX, scrollY);
            }
        }

        if (isDrawing && !strokeBuffer.empty()) {
            DrawSmoothStroke(pRenderTarget, strokeBuffer, isEraserMode, currentBrushColor, currentStrokeBrushSize, scrollX, scrollY);
        }

        RECT rcClient; GetClientRect(hwnd, &rcClient);
        if (showGrid) {
            DrawGrid(pRenderTarget, D2D1::RectF(0, 0, (float)rcClient.right, (float)rcClient.bottom));
        }

        D2D1_RECT_F statusRect = D2D1::RectF(0, (FLOAT)rcClient.bottom - 30.0f, (FLOAT)rcClient.right, (FLOAT)rcClient.bottom);
        ID2D1SolidColorBrush* pStatusBgBrush = nullptr;
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f), &pStatusBgBrush);
        pRenderTarget->FillRectangle(statusRect, pStatusBgBrush);
        pStatusBgBrush->Release();
        ID2D1SolidColorBrush* pTextBrush = nullptr;
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pTextBrush);
        pRenderTarget->DrawTextW(g_statusText.c_str(), (UINT32)g_statusText.length(), pTextFormat, &statusRect, pTextBrush, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
        pTextBrush->Release();

        pRenderTarget->EndDraw();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lParam) == HTCLIENT) {
            if (isSpacePressed) { SetCursor(LoadCursor(NULL, IDC_SIZEALL)); return TRUE; }
            else if (isPaintbrushSelected || isEraserMode) { SetCursor(LoadCursor(NULL, IDC_CROSS)); return TRUE; }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    case WM_DESTROY:
    {
        SaveCanvasState();
        DiscardDeviceResources();
        if (pFactory) pFactory->Release();
        if (pTextFormat) pTextFormat->Release();
        if (pDWriteFactory) pDWriteFactory->Release();
        PostQuitMessage(0);
        return 0;
    }
    default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

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
    hWnd = CreateWindowEx(0, CLASS_NAME, L"Infinite Canvas Doodle App (P=Brush E=Eraser C=Clear +-=BrushSize Space+Drag=Scroll Home=Center Q=Color G=Grid A=AlphaTransparency []=Opacity PgUp = ZoomIn PgDown = ZoomOut F1=About)", WS_OVERLAPPEDWINDOW | WS_MAXIMIZE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    if (!hWnd) return 0;
    SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_COMPOSITED);
    ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}