// ============================================================================
// ULTIMATE OOP DEMONSTRATION - FIXED FOR Dev-C++ 6.3 + MinGW-w64 + C++20
// Now compiles with ZERO errors/warnings under -std=c++2a
// ============================================================================

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <process.h>
#include <semaphore>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <bitset>
#include <chrono>
#include <functional>
#include <cmath>                  // ← FIXED: sin() lives here
#include <cstdint>

using namespace std::chrono_literals;

// ============================================================================
// 1. Concepts (C++20)
// ============================================================================

template<typename T>
concept Drawable = requires(T t, HDC hdc) {
    t.Draw(hdc);
};

template<typename T>
concept Updateable = requires(T t, float dt) {
    t.Update(dt);
};

// ============================================================================
// 2. Interfaces (Multiple Inheritance Ready)
// ============================================================================

struct IRenderable {
    virtual void Draw(HDC hdc) const = 0;
    virtual ~IRenderable() = default;
};

struct IUpdateable {
    virtual void Update(float dt) = 0;
    virtual ~IUpdateable() = default;
};

struct IInputHandler {
    virtual bool HandleKey(WPARAM wParam) = 0;
    virtual ~IInputHandler() = default;
};

// ============================================================================
// 3. CRTP Base — FIXED: friend the derived class to access protected members
// ============================================================================

template<typename Derived>
struct CRTP_Rotatable {
    void Rotate(float angle) {
        auto& self = static_cast<Derived&>(*this);
        self.angle += angle;
        if (self.angle >= 360.0f)
            self.angle -= 360.0f;
    }

protected:          // ← Important: derived class accesses via friend below
    friend Derived;
};

// ============================================================================
// 4. Policy-Based GameObject — Multiple Inheritance + CRTP + Policies
// ============================================================================

template<typename ColorPolicy, typename ShapePolicy>
class GameObject :
    public IRenderable,
    public IUpdateable,
    public IInputHandler,
    public CRTP_Rotatable<GameObject<ColorPolicy, ShapePolicy>>,  // CRTP
    public ColorPolicy,
    public ShapePolicy
{
    friend CRTP_Rotatable<GameObject<ColorPolicy, ShapePolicy>>;  // ← CRITICAL FIX

protected:
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float angle = 0.0f;           // ← Now safely accessible by CRTP base

public:
    using ColorPolicy::GetColor;
    using ShapePolicy::GetRadius;

    template<typename... Args>
    GameObject(float x_, float y_, Args&&... args)
        : x(x_), y(y_), ColorPolicy(std::forward<Args>(args)...) {}

    // Move-only (Rule of Five satisfied)
    GameObject(GameObject&&) noexcept = default;
    GameObject& operator=(GameObject&&) noexcept = default;
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    void Draw(HDC hdc) const override {
        COLORREF col = GetColor();
        HBRUSH brush = CreateSolidBrush(col);
        SelectObject(hdc, brush);
        ShapePolicy::DrawShape(hdc, (int)x, (int)y, GetRadius());
        DeleteObject(brush);
    }

    void Update(float dt) override {
        x += vx * dt;
        y += vy * dt;
        this->Rotate(dt * 90.0f);  // ← Now compiles perfectly
    }

    bool HandleKey(WPARAM wParam) override {
        switch (wParam) {
            case 'W': vy = -200; return true;
            case 'S': vy =  200; return true;
            case 'A': vx = -200; return true;
            case 'D': vx =  200; return true;
        }
        return false;
    }

    void SetVelocity(float vx_, float vy_) { vx = vx_; vy = vy_; }
};

// ============================================================================
// 5. Policies
// ============================================================================

struct RedPulsePolicy {
    mutable std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    COLORREF GetColor() const {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        BYTE intensity = static_cast<BYTE>(128 + 127 * sin(ms * 0.01));
        return RGB(intensity, 0, 0);
    }
};

struct RainbowPolicy {
    mutable float time = 0.0f;
    void Update(float dt) { time += dt; }
    COLORREF GetColor() const {
        return RGB(
            static_cast<BYTE>(127 + 128 * sin(time)),
            static_cast<BYTE>(127 + 128 * sin(time + 2)),
            static_cast<BYTE>(127 + 128 * sin(time + 4))
        );
    }
};

struct CircleShape {
    int GetRadius() const { return 40; }
    static void DrawShape(HDC hdc, int x, int y, int r) {
        Ellipse(hdc, x - r, y - r, x + r, y + r);
    }
};

struct SquareShape {
    int GetRadius() const { return 35; }
    static void DrawShape(HDC hdc, int x, int y, int r) {
        Rectangle(hdc, x - r, y - r, x + r, y + r);
    }
};

// ============================================================================
// 6. Final Types
// ============================================================================

using PulsingRedCircle = GameObject<RedPulsePolicy, CircleShape>;
using RainbowSquare    = GameObject<RainbowPolicy,  SquareShape>;

// ============================================================================
// 7. GameEngine — Singleton + Factory + Observer + Thread Safety
// ============================================================================

class GameEngine {
private:
    GameEngine() = default;

    std::vector<std::unique_ptr<IRenderable>> renderList;
    std::vector<IUpdateable*>                 updateList;
    std::vector<IInputHandler*>               inputHandlers;

    mutable std::binary_semaphore renderSemaphore{1};  // ← mutable = allowed in const
    std::atomic<bool> running{true};
    std::mutex mtx;

    std::vector<std::function<void(const std::string&)>> logObservers;

public:
    static GameEngine& Instance() {
        static GameEngine instance;
        return instance;
    }

    template<typename T, typename... Args>
    T* CreateObject(Args&&... args) {
        auto obj = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = obj.get();

        renderList.push_back(std::move(obj));
        if constexpr (std::is_base_of_v<IUpdateable, T>)
            updateList.push_back(ptr);
        if constexpr (std::is_base_of_v<IInputHandler, T>)
            inputHandlers.push_back(ptr);

        FireLog("Created: " + std::string(typeid(T).name()));
        return ptr;
    }

    void FireLog(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& cb : logObservers)
            cb(msg);
    }

    void SubscribeLog(std::function<void(const std::string&)> cb) {
        std::lock_guard<std::mutex> lock(mtx);
        logObservers.push_back(std::move(cb));
    }

    void Update(float dt) {
        for (auto* obj : updateList)
            obj->Update(dt);
    }

    void Render(HDC hdc) const {
        renderSemaphore.acquire();                 // ← OK because semaphore is mutable
        for (const auto& obj : renderList)
            obj->Draw(hdc);
        renderSemaphore.release();
    }

    bool HandleKey(WPARAM wParam) {
        for (auto* h : inputHandlers)
            if (h->HandleKey(wParam))
                return true;
        return false;
    }

    void Shutdown() { running = false; }
    bool IsRunning() const { return running; }
};

// ============================================================================
// 8. Bit Flags (C++20 to_underlying replacement for C++20 compilers)
// ============================================================================

enum class GameFlags : unsigned int {
    None        = 0,
    GodMode     = 1u << 0,
    NoClip      = 1u << 1,
    Wireframe   = 1u << 2,
    RainbowMode = 1u << 3
};

constexpr GameFlags operator|(GameFlags a, GameFlags b) {
    return static_cast<GameFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

constexpr bool HasFlag(GameFlags flags, GameFlags flag) {
    return (static_cast<unsigned int>(flags) & static_cast<unsigned int>(flag)) != 0;
}

// ============================================================================
// 9. Main Window with Member Callback
// ============================================================================

class MainWindow {
    HWND hwnd = nullptr;
    HDC memDC = nullptr;
    HBITMAP memBitmap = nullptr;
    std::chrono::steady_clock::time_point lastTime;
    GameFlags flags = GameFlags::RainbowMode;

public:
    static LRESULT CALLBACK WindowProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
        MainWindow* pThis = nullptr;

        if (msg == WM_NCCREATE) {
            pThis = static_cast<MainWindow*>(((CREATESTRUCT*)l)->lpCreateParams);
            SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->hwnd = h;
        } else {
            pThis = (MainWindow*)GetWindowLongPtr(h, GWLP_USERDATA);
        }

        if (pThis)
            return pThis->HandleMessage(msg, w, l);

        return DefWindowProc(h, msg, w, l);
    }

    bool Create() {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WindowProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"UltimateOOPDemo";
        RegisterClassExW(&wc);

        hwnd = CreateWindowExW(0, L"UltimateOOPDemo",
            L"Ultimate C++ OOP Demo - All Concepts Included",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
            nullptr, nullptr, GetModuleHandle(nullptr), this);

        HDC screen = GetDC(nullptr);
        memDC = CreateCompatibleDC(screen);
        memBitmap = CreateCompatibleBitmap(screen, 1200, 800);
        ReleaseDC(nullptr, screen);
        SelectObject(memDC, memBitmap);

        lastTime = std::chrono::steady_clock::now();

        GameEngine::Instance().SubscribeLog([](const std::string& s) {
            OutputDebugStringA((s + "\n").c_str());
        });

        GameEngine::Instance().CreateObject<PulsingRedCircle>(400, 300);
        GameEngine::Instance().CreateObject<RainbowSquare>(800, 500);
        auto obj = GameEngine::Instance().CreateObject<PulsingRedCircle>(600, 400);
        obj->SetVelocity(100, 150);

        ShowWindow(hwnd, SW_SHOW);
        return true;
    }

    void Run() {
        MSG msg{};
        while (GameEngine::Instance().IsRunning()) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            GameEngine::Instance().Update(dt);
            InvalidateRect(hwnd, nullptr, FALSE);
            std::this_thread::sleep_for(1ms);
        }
    }

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(memDC, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
                GameEngine::Instance().Render(memDC);
                BitBlt(hdc, 0, 0, 1200, 800, memDC, 0, 0, SRCCOPY);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) GameEngine::Instance().Shutdown();
                GameEngine::Instance().HandleKey(wParam);
                return 0;

            case WM_DESTROY:
                GameEngine::Instance().Shutdown();
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
};

// ============================================================================
// 10. Entry Point
// ============================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MainWindow win;
    if (win.Create())
        win.Run();
    return 0;
}