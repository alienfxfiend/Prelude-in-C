#include <windows.h>
#include <vector>
#include <ctime>
#include <cstdlib>
#include "resource.h"  // Add this with your other includes

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define GRID_SIZE 20
#define SNAKE_INITIAL_LENGTH 5
#define MOVE_INTERVAL 100

enum Direction { UP, DOWN, LEFT, RIGHT };

struct Point {
    int x, y;
    Point(int _x = 0, int _y = 0) : x(_x), y(_y) {}
};

class Snake {
private:
    std::vector<Point> body;
    Direction currentDirection;
    Direction nextDirection;
    bool growing;

public:
    Snake() : currentDirection(RIGHT), nextDirection(RIGHT), growing(false) {
        for (int i = 0; i < SNAKE_INITIAL_LENGTH; ++i) {
            body.push_back(Point(5 - i, 5));
        }
    }

    void move() {
        currentDirection = nextDirection;
        Point newHead = body.front();
        switch (currentDirection) {
        case UP: newHead.y--; break;
        case DOWN: newHead.y++; break;
        case LEFT: newHead.x--; break;
        case RIGHT: newHead.x++; break;
        }

        // Ensure the snake wraps around the window area
        newHead.x = (newHead.x + (WINDOW_WIDTH / GRID_SIZE)) % (WINDOW_WIDTH / GRID_SIZE);
        newHead.y = (newHead.y + (WINDOW_HEIGHT / GRID_SIZE)) % (WINDOW_HEIGHT / GRID_SIZE);

        body.insert(body.begin(), newHead);
        if (!growing) {
            body.pop_back();
        }
        growing = false;
    }



    void grow() { growing = true; }

    void setDirection(Direction dir) {
        if ((dir == UP || dir == DOWN) && (currentDirection == LEFT || currentDirection == RIGHT)) {
            nextDirection = dir;
        }
        else if ((dir == LEFT || dir == RIGHT) && (currentDirection == UP || currentDirection == DOWN)) {
            nextDirection = dir;
        }
    }

    bool checkCollision() const {
        for (size_t i = 1; i < body.size(); ++i) {
            if (body[0].x == body[i].x && body[0].y == body[i].y) {
                return true;
            }
        }
        return false;
    }

    const std::vector<Point>& getBody() const { return body; }

    Direction getCurrentDirection() const {
        return currentDirection;
    }
};

class Game {
private:
    Snake snake;
    Point food;
    bool gameOver;
    bool paused;
    int score;

    HWND hwnd;
    HDC hdc, memDC;
    HBITMAP memBitmap;
    HBRUSH snakeBrush, foodBrush, backgroundBrush;

public:
    Game(HWND hWnd) : hwnd(hWnd), gameOver(false), paused(true), score(0) {
        srand(static_cast<unsigned>(time(nullptr)));
        spawnFood();

        hdc = GetDC(hwnd);
        memDC = CreateCompatibleDC(hdc);
        memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
        SelectObject(memDC, memBitmap);

        snakeBrush = CreateSolidBrush(RGB(0, 255, 0)); // Red (255, 0, 0)
        foodBrush = CreateSolidBrush(RGB(255, 0, 0)); // Red (255, 0, 0)
        backgroundBrush = CreateSolidBrush(RGB(0, 0, 0)); //khaki 196, 178, 137 // black 0,0,0
    }

    ~Game() {
        DeleteObject(snakeBrush);
        DeleteObject(foodBrush);
        DeleteObject(backgroundBrush);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        ReleaseDC(hwnd, hdc);
    }

    void spawnFood() {
        int gridWidth = WINDOW_WIDTH / GRID_SIZE;
        int gridHeight = WINDOW_HEIGHT / GRID_SIZE;
        do {
            food.x = rand() % gridWidth;
            food.y = rand() % gridHeight;
        } while (isSnakeOnPoint(food));
    }


    bool isSnakeOnPoint(const Point& p) const {
        for (const auto& segment : snake.getBody()) {
            if (segment.x == p.x && segment.y == p.y) {
                return true;
            }
        }
        return false;
    }

    void update() {
        if (gameOver || paused) return;

        snake.move();

        if (snake.checkCollision()) {
            gameOver = true;
            return;
        }

        if (snake.getBody().front().x == food.x && snake.getBody().front().y == food.y) {
            snake.grow();
            do {
                spawnFood();
            } while (isSnakeOnPoint(food));
            score++;
        }
    }

    void render() {
        RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        FillRect(memDC, &rect, backgroundBrush);

        // Draw snake body
        for (const auto& segment : snake.getBody()) {
            RECT snakeRect = {
                segment.x * GRID_SIZE,
                segment.y * GRID_SIZE,
                (segment.x + 1) * GRID_SIZE,
                (segment.y + 1) * GRID_SIZE
            };
            FillRect(memDC, &snakeRect, snakeBrush);
        }

        // Draw snake eyes
        if (!snake.getBody().empty()) {
            const Point& head = snake.getBody().front();
            int eyeSize = 4; // 3 pixel radius for each eye
            int eyeOffset = GRID_SIZE / 9; // 5->7 Offset from the edge of the head

            // Calculate eye positions based on snake's direction
            int leftEyeX, leftEyeY, rightEyeX, rightEyeY;
            switch (snake.getCurrentDirection()) {
            case UP:
                leftEyeX = head.x * GRID_SIZE + eyeOffset;
                rightEyeX = (head.x + 1) * GRID_SIZE - eyeOffset - eyeSize * 2;
                leftEyeY = rightEyeY = head.y * GRID_SIZE + eyeOffset;
                break;
            case DOWN:
                leftEyeX = head.x * GRID_SIZE + eyeOffset;
                rightEyeX = (head.x + 1) * GRID_SIZE - eyeOffset - eyeSize * 2;
                leftEyeY = rightEyeY = (head.y + 1) * GRID_SIZE - eyeOffset - eyeSize * 2;
                break;
            case LEFT:
                leftEyeX = rightEyeX = head.x * GRID_SIZE + eyeOffset;
                leftEyeY = head.y * GRID_SIZE + eyeOffset;
                rightEyeY = (head.y + 1) * GRID_SIZE - eyeOffset - eyeSize * 2;
                break;
            case RIGHT:
                leftEyeX = rightEyeX = (head.x + 1) * GRID_SIZE - eyeOffset - eyeSize * 2;
                leftEyeY = head.y * GRID_SIZE + eyeOffset;
                rightEyeY = (head.y + 1) * GRID_SIZE - eyeOffset - eyeSize * 2;
                break;
            }

            // Draw the eyes
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255)); // yellow 255, 255, 0 darkpurple 128, 0, 128 purple 157, 0, 255 white 255,255,255
            SelectObject(memDC, whiteBrush);
            Ellipse(memDC, leftEyeX, leftEyeY, leftEyeX + eyeSize * 2, leftEyeY + eyeSize * 2);
            Ellipse(memDC, rightEyeX, rightEyeY, rightEyeX + eyeSize * 2, rightEyeY + eyeSize * 2);
            DeleteObject(whiteBrush);
        }

        // Draw food
        if (food.x >= 0 && food.y >= 0) {
            RECT foodRect = {
                food.x * GRID_SIZE,
                food.y * GRID_SIZE,
                (food.x + 1) * GRID_SIZE,
                (food.y + 1) * GRID_SIZE
            };
            FillRect(memDC, &foodRect, foodBrush);
        }

        WCHAR scoreText[32];
        swprintf_s(scoreText, L"Score: %d", score);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOut(memDC, 10, 10, scoreText, wcslen(scoreText));

        if (gameOver) {
            const WCHAR* gameOverText = L"Game Over! Press any arrow key to restart.";
            TextOut(memDC, WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2, gameOverText, wcslen(gameOverText));
        }
        else if (paused) {
            const WCHAR* pausedText = L"Paused. Press any arrow key to start.";
            TextOut(memDC, WINDOW_WIDTH / 2 - 120, WINDOW_HEIGHT / 2, pausedText, wcslen(pausedText));
        }

        BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memDC, 0, 0, SRCCOPY);
    }

    void togglePause() {
        paused = !paused;
    }

    void reset() {
        snake = Snake();
        do {
            spawnFood();
        } while (isSnakeOnPoint(food));
        score = 0;
        gameOver = false;
        paused = true;
    }

    void handleKeyPress(WPARAM wParam) {
        switch (wParam) {
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
            if (gameOver) {
                reset();
            }
            else {
                Direction newDir;
                switch (wParam) {
                case VK_UP: newDir = UP; break;
                case VK_DOWN: newDir = DOWN; break;
                case VK_LEFT: newDir = LEFT; break;
                case VK_RIGHT: newDir = RIGHT; break;
                }
                snake.setDirection(newDir);
                if (paused) paused = false;
            }
            break;
        case VK_SPACE:
            togglePause();
            break;
        case VK_ESCAPE:
            reset();
            break;
        case VK_F1:  // Add this case for F1 key
            MessageBoxW(hwnd, L"Snake Game 1.3 Programmed in C++ Win32 API (383 lines of code) by Entisoft Software (c) Evans Thorpemorton", L"About", MB_OK | MB_ICONINFORMATION);
            break;
        }
    }

    static void AdjustWindowSize(HWND hwnd) {
        RECT rcClient, rcWindow;
        GetClientRect(hwnd, &rcClient);
        GetWindowRect(hwnd, &rcWindow);
        int width = (rcWindow.right - rcWindow.left) - (rcClient.right - rcClient.left) + WINDOW_WIDTH;
        int height = (rcWindow.bottom - rcWindow.top) - (rcClient.bottom - rcClient.top) + WINDOW_HEIGHT;
        SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}; // Add this closing brace to end the Game class

Game* game = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        game = new Game(hwnd);
        SetTimer(hwnd, 1, MOVE_INTERVAL, nullptr);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        delete game;
        PostQuitMessage(0);
        return 0;

    case WM_TIMER:
        game->update();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        game->render();
        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_KEYDOWN:
        game->handleKeyPress(wParam);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"SnakeGameWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));     // Add this line
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    // Get the screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate the window position to center it
    int windowWidth = WINDOW_WIDTH + 16;
    int windowHeight = WINDOW_HEIGHT + 39;
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Snake Game (ArrowKeys=Move Space=Pause Escape=Reset)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, WINDOW_WIDTH + 16, WINDOW_HEIGHT + 39,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hwnd == nullptr) {
        return 0;
    }

    Game::AdjustWindowSize(hwnd);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
