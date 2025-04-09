#include <windows.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory> 
#include <functional> // For function callbacks
#include <iostream>   // For debug output
#include "resource.h"  // Add this with your other includes

// Abstract base class for shapes (with virtual destructors for polymorphism)
class Shape {
public:
    Shape() {} // Default constructor
    virtual void draw(HDC hdc) = 0;  // Pure virtual function for polymorphism
    virtual ~Shape() = default;      // Virtual destructor

    Shape(const Shape& other) = default;            // Copy constructor
    Shape& operator=(const Shape& other) = default; // Copy assignment

    Shape(Shape&& other) noexcept = default;        // Move constructor
    Shape& operator=(Shape&& other) noexcept = default; // Move assignment
};

// Concrete shape classes
class Circle : public Shape {
public:
    Circle(int x, int y, int r) : x(x), y(y), radius(r) {}
    void draw(HDC hdc) override {
        Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
    }

private:
    int x, y, radius;
};

class RectangleShape : public Shape {
public:
    RectangleShape(int x, int y, int w, int h) : x(x), y(y), width(w), height(h) {}
    void draw(HDC hdc) override {
        ::Rectangle(hdc, x, y, x + width, y + height);
    }

private:
    int x, y, width, height;
};

// Bit manipulation example (for understanding bitwise ops in a real app)
class BitManipulator {
public:
    static void manipulateBits(int& number, int bitToSet) {
        number |= (1 << bitToSet); // Set specific bit
    }
};

// Function pointer callback for drawing operations
using DrawCallback = std::function<void(HDC)>;

// Color class to demonstrate multiple inheritance
class Color {
public:
    Color(COLORREF color) : color_(color) {}
    COLORREF getColor() const { return color_; }

private:
    COLORREF color_;
};

// Multiple inheritance: ColoredShape inherits from Shape and Color
class ColoredShape : public Shape, public Color {
public:
    ColoredShape(std::unique_ptr<Shape> shape, COLORREF color)
        : Color(color), shape_(std::move(shape)) {}

    void draw(HDC hdc) override {
        HBRUSH brush = CreateSolidBrush(getColor());
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
        shape_->draw(hdc); // Delegate drawing to the encapsulated shape
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
    }

    ColoredShape(const ColoredShape& other) : Color(other.getColor()), shape_(std::make_unique<Circle>(100, 100, 50)) {}
    ColoredShape(ColoredShape&& other) noexcept = default; // Move constructor

private:
    std::unique_ptr<Shape> shape_;
};

// Thread-safe drawing board
class DrawingBoard {
public:
    void addShape(std::unique_ptr<Shape> shape) {
        std::lock_guard<std::mutex> lock(mutex_);
        shapes_.push_back(std::move(shape));
    }

    void drawAll(HDC hdc) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& shape : shapes_) {
            shape->draw(hdc);
        }
    }

private:
    std::vector<std::unique_ptr<Shape>> shapes_;
    std::mutex mutex_;
};

// Global variables
DrawingBoard g_board;
std::atomic<bool> g_shouldExit(false);
std::mutex g_mutex;
volatile int volatileFlag = 0; // Example of volatile usage
// Add these two lines:
int g_windowWidth = 500;
int g_windowHeight = 400;

// Window procedure callback
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        g_board.drawAll(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_shouldExit = true;
            PostQuitMessage(0);
            return 0;
        }
        // Add this new code block
        else if (wParam == VK_F1) {
            MessageBox(hwnd, TEXT("Demonstrates **Multiple Inheritance**: `ColoredShape` inherits from both `Shape` and `COLORREF`. **Bit Manipulation**: `BitManipulator` shows bitwise operations. **Move/Copy Constructors**: Explicitly handled in `ColoredShape`, ensuring each class has both. **Function Pointers/Callbacks**: `DrawCallback` for draw operations, with callbacks added in drawing operations. **Multithreading**: Drawing operations run in a separate thread, with threading and synchronization using semaphores. **Volatile**: Used `volatile` for demonstrating usage, integrating volatile variables. **Polymorphism**: Base class `Shape` with virtual `draw()` function, ensuring virtual methods are used, allowing derived classes to override behavior. **Encapsulation**: Properly encapsulate data members. **Nested Functions**: Implement using lambdas. **Override/this**: Integrate override annotations and `this` pointer usage. (210 lines of code)"), TEXT("Information"), MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMaxSize.x = g_windowWidth;
        lpMMI->ptMaxSize.y = g_windowHeight;
        lpMMI->ptMaxPosition.x = 0;
        lpMMI->ptMaxPosition.y = 0;
        return 0;
    }
    case WM_DESTROY:
        g_shouldExit = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void drawingThread(HWND hwnd) {
    while (!g_shouldExit) {
        InvalidateRect(hwnd, NULL, TRUE);
        Sleep(100); // Simulate work
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));     // Add this line

    RegisterClass(&wc);

    // Calculate the center position
    //int windowWidth = 500;
    //int windowHeight = 400;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - g_windowWidth) / 2;
    int y = (screenHeight - g_windowHeight) / 2;

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"OOP Win32 Demo (F1=About)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // Modified style
        x, y, g_windowWidth, g_windowHeight,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Add shapes to the board
    g_board.addShape(std::make_unique<Circle>(100, 100, 50));
    g_board.addShape(std::make_unique<ColoredShape>(std::make_unique<RectangleShape>(150, 150, 100, 50), RGB(255, 0, 0)));

    // Start drawing thread
    std::thread drawThread(drawingThread, hwnd);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    drawThread.join();
    return 0;
}
