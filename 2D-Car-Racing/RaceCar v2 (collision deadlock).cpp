#include <Windows.h>
#include <ctime>
#include <cstdlib>
#include <math.h>
#include <stdio.h>
#include <string>
#include "resource.h"  // Add this with your other includes

// --- Constants --- (Consider adding const double M_PI if not defined in cmath)
#ifndef M_PI
#define M_PI 3.14159265358979323846f  // Add 'f' here
#endif

// --- OBB / SAT Collision Detection Helpers ---
struct Vec2 { float x, y; };

void GetRectangleCorners(float cx, float cy, float halfW, float halfH, float angleRad, Vec2 out[4])
{
    // Local space corners
    Vec2 local[4] = {
        { -halfW, -halfH }, {  halfW, -halfH },
        {  halfW,  halfH }, { -halfW,  halfH }
    };
    float c = cosf(angleRad), s = sinf(angleRad);
    for (int i = 0; i < 4; ++i)
    {
        out[i].x = cx + local[i].x * c - local[i].y * s;
        out[i].y = cy + local[i].x * s + local[i].y * c;
    }
}

float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }

bool OverlapOnAxis(const Vec2 a[4], const Vec2 b[4], const Vec2& axis)
{
    float minA = Dot(a[0], axis), maxA = minA;
    float minB = Dot(b[0], axis), maxB = minB;
    for (int i = 1; i < 4; ++i)
    {
        float pA = Dot(a[i], axis);
        minA = fminf(minA, pA); maxA = fmaxf(maxA, pA);
        float pB = Dot(b[i], axis);
        minB = fminf(minB, pB); maxB = fmaxf(maxB, pB);
    }
    return !(maxA < minB || maxB < minA);
}

bool CheckOBBCollision(int x1, int y1, int w1, int h1, float angleDeg1,
    int x2, int y2, int w2, int h2, float angleDeg2)
{
    Vec2 c1[4], c2[4];
    // centers
    float cx1 = x1 + w1 * 0.5f, cy1 = y1 + h1 * 0.5f;
    float cx2 = x2 + w2 * 0.5f, cy2 = y2 + h2 * 0.5f;
    float r1 = angleDeg1 * (float)M_PI / 180.0f;
    float r2 = angleDeg2 * (float)M_PI / 180.0f;
    GetRectangleCorners(cx1, cy1, w1 * 0.5f, h1 * 0.5f, r1, c1);
    GetRectangleCorners(cx2, cy2, w2 * 0.5f, h2 * 0.5f, r2, c2);

    // Build 4 candidate axes (normals of each rect's edges)
    Vec2 axes[4] = {
        { c1[1].x - c1[0].x, c1[1].y - c1[0].y },
        { c1[3].x - c1[0].x, c1[3].y - c1[0].y },
        { c2[1].x - c2[0].x, c2[1].y - c2[0].y },
        { c2[3].x - c2[0].x, c2[3].y - c2[0].y }
    };

    for (int i = 0; i < 4; ++i)
    {
        Vec2 axis = { -axes[i].y, axes[i].x };              // perpendicular
        float len = sqrtf(axis.x * axis.x + axis.y * axis.y);
        if (len > 0.0001f) { axis.x /= len; axis.y /= len; }
        if (!OverlapOnAxis(c1, c2, axis)) return false;     // separation found
    }
    return true; // no separating axis -> collision
}


// Global Variables
const int WIDTH = 1366;
const int HEIGHT = 768;
const int ROAD_WIDTH = 200;
const int CAR_WIDTH = 50;
const int CAR_HEIGHT = 100;
const int TYRE_SIZE = 10;
const int FPS = 60;
const int TIMER = 4;
const int TURN_RADIUS = 5;
//const double PI = 3.14159265358979323846;
//const double M_PI = 3.14159265358979323846;

int playerX = 100;
int playerY = HEIGHT - CAR_HEIGHT - 50;
//int playerSpeedX = 0;
//int playerSpeedY = 0;
int aiX = playerX + CAR_WIDTH + 20;
int aiY = playerY;
float aiAngle = -M_PI / 2;  // Add this line
//int aiSpeedX = 0;
//int aiSpeedY = 0;
int speed = 5;
int aiSpeed = 5;
int timer = TIMER;
int playerTyre1X = playerX + 10;
int playerTyre1Y = playerY + CAR_HEIGHT - TYRE_SIZE;
int playerTyre2X = playerX + CAR_WIDTH - TYRE_SIZE - 10;
int playerTyre2Y = playerY + CAR_HEIGHT - TYRE_SIZE;
int aiTyre1X = aiX + 10;
int aiTyre1Y = aiY + CAR_HEIGHT - TYRE_SIZE;
int aiTyre2X = aiX + CAR_WIDTH - TYRE_SIZE - 10;
int aiTyre2Y = aiY + CAR_HEIGHT - TYRE_SIZE;
float playerAngle = -M_PI / 2;  // Initialize to face North by default
//float playerAngle = 0.0f;

bool gameStarted = false;
bool gameOver = false;
bool playerWon = false;
bool godMode = true;
//int timer = 30 * 10; // 30 seconds * 10 (timer resolution)

// --- Forward Declarations (If needed) ---
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // Set to NULL for custom background drawing
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"RacingGame";
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    RegisterClassEx(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowX = (screenWidth - WIDTH) / 2;
    int windowY = (screenHeight - HEIGHT) / 2;

    HWND hWnd = CreateWindowEx(0, L"RacingGame", L"Racing Game (ArrowKeys=Move G=GodMode)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, // Added WS_VISIBLE
        windowX, windowY, WIDTH, HEIGHT,
        NULL, NULL, hInstance, NULL);

    // Removed ShowWindow, WS_VISIBLE in CreateWindowEx handles it

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam; // Return final message code
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        SetTimer(hWnd, 1, 1000 / FPS, NULL); // Timer fires based on FPS
        // Initial car positions before game starts (if needed)
        playerX = WIDTH / 4; // Example start
        playerY = HEIGHT - CAR_HEIGHT - 50;
        playerAngle = 0; // Pointing up
        aiX = WIDTH * 3 / 4; // Example start
        aiY = HEIGHT - CAR_HEIGHT - 50;
        aiAngle = 0; // Pointing up
        break;
    case WM_TIMER:
        if (timer > 0 && !gameStarted)
        {
            timer--;
            InvalidateRect(hWnd, NULL, FALSE); // Request redraw for countdown
        }
        else if (timer <= 0 && !gameStarted)
        {
            gameStarted = true;
            srand((unsigned int)time(0));
            aiSpeed = rand() % 3 + 4; // Adjust AI speed range if needed (4-6)

            // Set initial positions for the race start
            playerX = ROAD_WIDTH / 2 - CAR_WIDTH / 2; // Start on left lane
            playerY = HEIGHT - CAR_HEIGHT - 20;       // Near bottom
            playerAngle = 0;                          // Facing up

            aiX = ROAD_WIDTH / 2 - CAR_WIDTH / 2;    // Start on left lane
            aiY = HEIGHT - CAR_HEIGHT - 150;         // Ahead of player
            aiAngle = 0;                             // Facing up

            InvalidateRect(hWnd, NULL, FALSE); // Redraw for game start
        }
        else if (gameStarted) // Game logic runs when gameStarted is true
        {
            static bool gKeyPressed = false;
            if (GetAsyncKeyState('G') & 0x8000) // Use 0x8000 for currently pressed state
            {
                // Basic toggle needs a flag to prevent rapid switching                
                if (!gKeyPressed) {
                    godMode = !godMode;
                    gKeyPressed = true; // Mark as pressed
                }
            }
            else {
                gKeyPressed = false; // Reset flag when key is released
            }

            // --- Save Previous States ---
            float prevPlayerXf = (float)playerX;
            float prevPlayerYf = (float)playerY;
            float prevPlayerAnglef = playerAngle;
            float prevAiXf = (float)aiX;
            float prevAiYf = (float)aiY;
            float prevAiAnglef = aiAngle;

            // --- Player Movement & Rotation ---
            float prevPlayerX = (float)playerX; // Store previous state
            float prevPlayerY = (float)playerY;
            float prevPlayerAngle = playerAngle;
            float playerRadAngle = playerAngle * (float)M_PI / 180.0f; // Convert degrees to radians if using degrees

            float angularVelocity = 3.0f; // Degrees per frame for turning
            float currentSpeed = 0;

            if (GetAsyncKeyState(VK_UP) & 0x8000) {
                currentSpeed = (float)speed;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
                currentSpeed = -(float)speed / 2; // Slower reverse
            }

            if (currentSpeed != 0) { // Only allow turning when moving
                if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
                    playerAngle -= angularVelocity;
                }
                if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
                    playerAngle += angularVelocity;
                }
            }

            // Normalize angle (optional but good practice)
            if (playerAngle >= 360.0f) playerAngle -= 360.0f;
            if (playerAngle < 0.0f) playerAngle += 360.0f;

            // Update position based on angle (using radians)
            playerRadAngle = playerAngle * (float)M_PI / 180.0f;
            playerX += (int)(sin(playerRadAngle) * currentSpeed);
            playerY -= (int)(cos(playerRadAngle) * currentSpeed); // Y decreases upwards

            // --- AI Movement Logic ---
            // Simple AI: Follows road path, adjusts angle at corners
            // (This is a basic example, can be made more complex)

            // Define path points or regions
            // --- AI Movement Logic ---
            int corner1Y = ROAD_WIDTH * 2;
            int corner2X = WIDTH - ROAD_WIDTH;

            if (aiY > corner1Y && aiX < corner2X) {
                aiAngle = 0;
                aiY -= aiSpeed;
            }
            else if (aiY <= corner1Y && aiX < corner2X) {
                aiAngle = 90;
                aiX += aiSpeed;
                if (abs(aiY - ROAD_WIDTH) > aiSpeed)
                    aiY += (ROAD_WIDTH - aiY > 0) ? 1 : -1;
            }
            else if (aiX >= corner2X && aiY < HEIGHT - CAR_HEIGHT) {
                aiAngle = 180;
                aiY += aiSpeed;
            }
            else if (aiY >= HEIGHT - CAR_HEIGHT && aiX >= corner2X) {
                aiX = ROAD_WIDTH / 2 - CAR_WIDTH / 2;
                aiY = HEIGHT - CAR_HEIGHT - 20;
                aiAngle = 0;
            }
            // Basic wrap around or finish line logic could go here

                // --- Collision & Road-Boundary Checks ---
    // --- Collision & Road Boundary Checks (God Mode OFF) ---
            if (!godMode)
            {
                // 1) OBB vs OBB collision?
                if (CheckOBBCollision(
                    playerX, playerY, CAR_WIDTH, CAR_HEIGHT, playerAngle,
                    aiX, aiY, CAR_WIDTH, CAR_HEIGHT, aiAngle))
                {
                    /*// revert both cars
                    playerX = (int)prevPlayerXf;  playerY = (int)prevPlayerYf;
                    playerAngle = prevPlayerAnglef;
                    aiX = (int)prevAiXf;      aiY = (int)prevAiYf;
                    aiAngle = prevAiAnglef;*/

                    // === Professional Collision Resolution using SAT + MTV (Minimum Translation Vector) ===
                    Vec2 pCorners[4], aCorners[4];
                    float pCx = playerX + CAR_WIDTH * 0.5f;
                    float pCy = playerY + CAR_HEIGHT * 0.5f;
                    float aCx = aiX + CAR_WIDTH * 0.5f;
                    float aCy = aiY + CAR_HEIGHT * 0.5f;
                    float pRad = playerAngle * (float)M_PI / 180.0f;
                    float aRad = aiAngle * (float)M_PI / 180.0f;

                    GetRectangleCorners(pCx, pCy, CAR_WIDTH * 0.5f, CAR_HEIGHT * 0.5f, pRad, pCorners);
                    GetRectangleCorners(aCx, aCy, CAR_WIDTH * 0.5f, CAR_HEIGHT * 0.5f, aRad, aCorners);

                    Vec2 axes[4] = {
                        { pCorners[1].x - pCorners[0].x, pCorners[1].y - pCorners[0].y },
                        { pCorners[3].x - pCorners[0].x, pCorners[3].y - pCorners[0].y },
                        { aCorners[1].x - aCorners[0].x, aCorners[1].y - aCorners[0].y },
                        { aCorners[3].x - aCorners[0].x, aCorners[3].y - aCorners[0].y }
                    };

                    float minOverlap = FLT_MAX;
                    Vec2 mtv = { 0, 0 };
                    bool collisionConfirmed = false;

                    for (int i = 0; i < 4; ++i)
                    {
                        Vec2 axis = { -axes[i].y, axes[i].x };
                        float len = sqrtf(axis.x * axis.x + axis.y * axis.y);
                        if (len < 0.001f) continue;
                        axis.x /= len; axis.y /= len;

                        float minP = FLT_MAX, maxP = -FLT_MAX;
                        float minA = FLT_MAX, maxA = -FLT_MAX;
                        for (int j = 0; j < 4; ++j)
                        {
                            float pp = Dot(pCorners[j], axis);
                            float pa = Dot(aCorners[j], axis);
                            minP = fminf(minP, pp); maxP = fmaxf(maxP, pp);
                            minA = fminf(minA, pa); maxA = fmaxf(maxA, pa);
                        }

                        float overlap = fminf(maxP - minA, maxA - minP);
                        if (overlap <= 0) { collisionConfirmed = false; break; }
                        collisionConfirmed = true;

                        if (overlap < minOverlap)
                        {
                            minOverlap = overlap;
                            mtv = axis;

                            // Ensure MTV points from AI ? Player
                            float d = Dot({ pCx - aCx, pCy - aCy }, axis);
                            if (d < 0) { mtv.x = -mtv.x; mtv.y = -mtv.y; }
                        }
                    }

                    if (collisionConfirmed)
                    {
                        float push = minOverlap + 3.0f;  // Extra push to fully separate
                        playerX += (int)(mtv.x * push);
                        playerY += (int)(mtv.y * push);

                        // Optional: small spin to help player steer out
                        float cross = (aCx - pCx) * mtv.y - (aCy - pCy) * mtv.x;
                        playerAngle += (cross > 0 ? 8.0f : -8.0f);
                    }
                }
                //}

                /*// 2) Off-road check for player
                RECT leftRoad = { 0,                 0, ROAD_WIDTH,         HEIGHT };
                RECT rightRoad = { WIDTH - ROAD_WIDTH,  0, WIDTH,              HEIGHT };
                RECT topRoad = { 0,                 0, WIDTH,              ROAD_WIDTH * 2 };
                RECT pRect = { playerX, playerY,
                                   playerX + CAR_WIDTH,
                                   playerY + CAR_HEIGHT };
                RECT temp;
                bool onLeft = IntersectRect(&temp, &pRect, &leftRoad);
                bool onRight = IntersectRect(&temp, &pRect, &rightRoad);
                bool onTop = IntersectRect(&temp, &pRect, &topRoad);
                if (!(onLeft || onRight || onTop))
                {
                    // revert player only
                    playerX = (int)prevPlayerXf;
                    playerY = (int)prevPlayerYf;
                    playerAngle = prevPlayerAnglef;
                }*/
            }

            // --- Screen Edge Clamping (Always) ---
            if (playerX < -CAR_WIDTH) playerX = -CAR_WIDTH;
            if (playerX > WIDTH)       playerX = WIDTH;
            if (playerY < -CAR_HEIGHT) playerY = -CAR_HEIGHT;
            if (playerY > HEIGHT)      playerY = HEIGHT;


            /*// --- Collision Detection & Handling (Simplified) ---
            // Basic AABB check (doesn't account for rotation)
            RECT playerRect = { playerX, playerY, playerX + CAR_WIDTH, playerY + CAR_HEIGHT };
            RECT aiRect = { aiX, aiY, aiX + CAR_WIDTH, aiY + CAR_HEIGHT };
            RECT intersection;

            if (!godMode && IntersectRect(&intersection, &playerRect, &aiRect))
            {
                // Collision occurred - crude response: move player back slightly
                playerX = (int)prevPlayerX;
                playerY = (int)prevPlayerY;
                playerAngle = prevPlayerAngle;
                // Could add bounce effect or game over here
            }

            // --- Boundary Checks (Simple) ---
            // Prevent player from going completely off-screen (adjust as needed)
            if (playerX < -CAR_WIDTH) playerX = -CAR_WIDTH;
            if (playerX > WIDTH) playerX = WIDTH;
            if (playerY < -CAR_HEIGHT) playerY = -CAR_HEIGHT;
            if (playerY > HEIGHT) playerY = HEIGHT;
            // More sophisticated road boundary checks needed for proper gameplay */

           // Request redraw
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break; // End WM_TIMER
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // --- Double Buffering Setup ---
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            // --- Drawing Starts Here (Draw onto memDC) ---

            // 1. Draw Background (Light Green)
            HBRUSH lightGreenBrush = CreateSolidBrush(RGB(144, 238, 144));
            FillRect(memDC, &clientRect, lightGreenBrush);
            DeleteObject(lightGreenBrush);

            // 2. Draw Roads (Black)
            HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
            RECT verticalRoad = { 0, 0, ROAD_WIDTH, HEIGHT };
            FillRect(memDC, &verticalRoad, blackBrush);
            RECT horizontalRoad = { 0, 0, WIDTH, ROAD_WIDTH * 2 };
            FillRect(memDC, &horizontalRoad, blackBrush);
            // Add second vertical road on the right if needed for a circuit
            RECT verticalRoad2 = { WIDTH - ROAD_WIDTH, 0, WIDTH, HEIGHT };
            FillRect(memDC, &verticalRoad2, blackBrush);
            DeleteObject(blackBrush);

            // 3. Draw Road Markings (Yellow Dashed Lines)
            HBRUSH yellowBrush = CreateSolidBrush(RGB(255, 255, 0));
            HGDIOBJ oldYellowBrush = SelectObject(memDC, yellowBrush);
            // Vertical dashes (Left Road)
            for (int y = 0; y < HEIGHT; y += 80) {
                Rectangle(memDC, ROAD_WIDTH / 2 - 5, y, ROAD_WIDTH / 2 + 5, y + 40);
            }
            // Vertical dashes (Right Road - if exists)
            for (int y = 0; y < HEIGHT; y += 80) {
                Rectangle(memDC, WIDTH - ROAD_WIDTH / 2 - 5, y, WIDTH - ROAD_WIDTH / 2 + 5, y + 40);
            }
            // Horizontal dashes
            for (int x = 0; x < WIDTH; x += 80) {
                Rectangle(memDC, x, ROAD_WIDTH - 5, x + 40, ROAD_WIDTH + 5);
            }
            SelectObject(memDC, oldYellowBrush);
            DeleteObject(yellowBrush);

            // --- Draw Player Car (Red) ---
            // Uses playerAngle in degrees, convert to radians for transformation
            float playerRadAngleDraw = playerAngle * (float)M_PI / 180.0f;
            HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
            HGDIOBJ oldRedBrush = SelectObject(memDC, redBrush);
            int savedDCPlayer = SaveDC(memDC);
            SetGraphicsMode(memDC, GM_ADVANCED);
            XFORM xformPlayer;
            xformPlayer.eM11 = (FLOAT)cos(playerRadAngleDraw);
            xformPlayer.eM12 = (FLOAT)sin(playerRadAngleDraw);
            xformPlayer.eM21 = (FLOAT)-sin(playerRadAngleDraw); // Negative sin for standard rotation
            xformPlayer.eM22 = (FLOAT)cos(playerRadAngleDraw);
            xformPlayer.eDx = (FLOAT)playerX + CAR_WIDTH / 2;
            xformPlayer.eDy = (FLOAT)playerY + CAR_HEIGHT / 2;
            SetWorldTransform(memDC, &xformPlayer);

            // Draw Player Body (relative coords)
            Rectangle(memDC, -CAR_WIDTH / 2, -CAR_HEIGHT / 2, CAR_WIDTH / 2, CAR_HEIGHT / 2);

            // HEADLIGHTS FIRST
            SelectObject(memDC, CreateSolidBrush(RGB(255, 255, 0)));
            Rectangle(memDC, -CAR_WIDTH / 2 + 2, -CAR_HEIGHT / 2 + 2, -CAR_WIDTH / 4, -CAR_HEIGHT / 2 + 6);
            Rectangle(memDC, CAR_WIDTH / 4, -CAR_HEIGHT / 2 + 2, CAR_WIDTH / 2 - 2, -CAR_HEIGHT / 2 + 6);

            // Draw Player Headlights (relative coords)
            HBRUSH pHeadlightBrush = CreateSolidBrush(RGB(255, 255, 220)); // Light Yellow
            HGDIOBJ oldPHeadlightBrush = SelectObject(memDC, pHeadlightBrush);
            int pHeadlightSize = 8;
            int pHeadlightXOffset = CAR_WIDTH / 4;
            int pHeadlightYOffset = -CAR_HEIGHT / 2 + 10 - 8;
            Ellipse(memDC, -pHeadlightXOffset - pHeadlightSize / 2, pHeadlightYOffset, -pHeadlightXOffset + pHeadlightSize / 2, pHeadlightYOffset + pHeadlightSize);
            Ellipse(memDC, pHeadlightXOffset - pHeadlightSize / 2, pHeadlightYOffset, pHeadlightXOffset + pHeadlightSize / 2, pHeadlightYOffset + pHeadlightSize);
            SelectObject(memDC, oldPHeadlightBrush);
            DeleteObject(pHeadlightBrush);

            // Draw Player Windows (relative coords)
            HBRUSH pWinBrush = CreateSolidBrush(RGB(60, 60, 60)); // Dark Gray
            HGDIOBJ oldPWinBrush = SelectObject(memDC, pWinBrush);
            int pWsWidth = CAR_WIDTH * 3 / 4;
            int pWsHeight = CAR_HEIGHT / 5;
            int pWsY = -CAR_HEIGHT / 2 + 25 + 10;
            Rectangle(memDC, -pWsWidth / 2, pWsY, pWsWidth / 2, pWsY + pWsHeight); // Windscreen
            int pSideWWidth = CAR_WIDTH / 8;
            int pSideWHeight = CAR_HEIGHT / 4;
            int pSideWY = pWsY + pWsHeight / 2 - pSideWHeight / 2 + 10;
            int pSideWXOffset = CAR_WIDTH / 2 - 5 - pSideWWidth / 2;
            Rectangle(memDC, -pSideWXOffset - pSideWWidth / 2, pSideWY, -pSideWXOffset + pSideWWidth / 2, pSideWY + pSideWHeight); // Left Side
            Rectangle(memDC, pSideWXOffset - pSideWWidth / 2, pSideWY, pSideWXOffset + pSideWWidth / 2, pSideWY + pSideWHeight); // Right Side
            SelectObject(memDC, oldPWinBrush);
            DeleteObject(pWinBrush);

            // Draw Player Tyres (relative coords)
            HBRUSH pTyreBrush = CreateSolidBrush(RGB(0, 0, 0));
            HGDIOBJ oldPTyreBrush = SelectObject(memDC, pTyreBrush);
            int pTyreWidth = 10;
            int pTyreHeight = 15;

            // Keeping original vertical positions:
            int pFrontTyreY = -CAR_HEIGHT / 2 + 15; // Original front Y position
            int pRearTyreY = CAR_HEIGHT / 2 - pTyreHeight - 5; // Original rear Y position

            // Adjust horizontal positions to stick to the edges of the car
            int pLeftTyreX = -CAR_WIDTH / 2; // Left edge of the car
            int pRightTyreX = CAR_WIDTH / 2;  // Right edge of the car

            // Draw tyres at the left and right positions
            // Front Left Tyre
            Rectangle(memDC, pLeftTyreX - pTyreWidth / 2, pFrontTyreY, pLeftTyreX + pTyreWidth / 2, pFrontTyreY + pTyreHeight); // FL
            // Front Right Tyre
            Rectangle(memDC, pRightTyreX - pTyreWidth / 2, pFrontTyreY, pRightTyreX + pTyreWidth / 2, pFrontTyreY + pTyreHeight); // FR
            // Rear Left Tyre
            Rectangle(memDC, pLeftTyreX - pTyreWidth / 2, pRearTyreY, pLeftTyreX + pTyreWidth / 2, pRearTyreY + pTyreHeight); // RL
            // Rear Right Tyre
            Rectangle(memDC, pRightTyreX - pTyreWidth / 2, pRearTyreY, pRightTyreX + pTyreWidth / 2, pRearTyreY + pTyreHeight); // RR

            SelectObject(memDC, oldPTyreBrush);
            DeleteObject(pTyreBrush);

            // Restore player DC
            RestoreDC(memDC, savedDCPlayer);
            SelectObject(memDC, oldRedBrush);
            DeleteObject(redBrush);

            // --- Draw AI Car (Blue) ---
            // Uses aiAngle in degrees, convert to radians for transformation
            float aiRadAngleDraw = aiAngle * (float)M_PI / 180.0f;
            HBRUSH blueBrush = CreateSolidBrush(RGB(0, 0, 255));
            HGDIOBJ oldBlueBrush = SelectObject(memDC, blueBrush);
            int savedDCAi = SaveDC(memDC);
            SetGraphicsMode(memDC, GM_ADVANCED);
            XFORM xformAi; // Use a different name
            xformAi.eM11 = (FLOAT)cos(aiRadAngleDraw);
            xformAi.eM12 = (FLOAT)sin(aiRadAngleDraw);
            xformAi.eM21 = (FLOAT)-sin(aiRadAngleDraw); // Negative sin for standard rotation
            xformAi.eM22 = (FLOAT)cos(aiRadAngleDraw);
            xformAi.eDx = (FLOAT)aiX + CAR_WIDTH / 2;
            xformAi.eDy = (FLOAT)aiY + CAR_HEIGHT / 2;
            SetWorldTransform(memDC, &xformAi); // Apply the transformation

            // Draw AI Body (relative coords)
            Rectangle(memDC, -CAR_WIDTH / 2, -CAR_HEIGHT / 2, CAR_WIDTH / 2, CAR_HEIGHT / 2);

            // HEADLIGHTS FIRST
            SelectObject(memDC, CreateSolidBrush(RGB(255, 255, 0)));
            Rectangle(memDC, -CAR_WIDTH / 2 + 2, -CAR_HEIGHT / 2 + 2, -CAR_WIDTH / 4, -CAR_HEIGHT / 2 + 6);
            Rectangle(memDC, CAR_WIDTH / 4, -CAR_HEIGHT / 2 + 2, CAR_WIDTH / 2 - 2, -CAR_HEIGHT / 2 + 6);

            // Draw AI Headlights (relative coords)
            HBRUSH aiHeadlightBrush = CreateSolidBrush(RGB(255, 255, 220)); // Light Yellow
            HGDIOBJ oldAiHeadlightBrush = SelectObject(memDC, aiHeadlightBrush);
            int aiHeadlightSize = 8;
            int aiHeadlightXOffset = CAR_WIDTH / 4;
            int aiHeadlightYOffset = -CAR_HEIGHT / 2 + 10 - 8; // Near "top" edge in local coords
            Ellipse(memDC, -aiHeadlightXOffset - aiHeadlightSize / 2, aiHeadlightYOffset, -aiHeadlightXOffset + aiHeadlightSize / 2, aiHeadlightYOffset + aiHeadlightSize);
            Ellipse(memDC, aiHeadlightXOffset - aiHeadlightSize / 2, aiHeadlightYOffset, aiHeadlightXOffset + aiHeadlightSize / 2, aiHeadlightYOffset + aiHeadlightSize);
            SelectObject(memDC, oldAiHeadlightBrush);
            DeleteObject(aiHeadlightBrush);

            // Draw AI Windows (relative coords)
            HBRUSH aiWinBrush = CreateSolidBrush(RGB(60, 60, 60)); // Dark Gray
            HGDIOBJ oldAiWinBrush = SelectObject(memDC, aiWinBrush);
            int aiWsWidth = CAR_WIDTH * 3 / 4;
            int aiWsHeight = CAR_HEIGHT / 5;
            int aiWsY = -CAR_HEIGHT / 2 + 25 + 10; // Near "top" edge
            Rectangle(memDC, -aiWsWidth / 2, aiWsY, aiWsWidth / 2, aiWsY + aiWsHeight); // Windscreen
            int aiSideWWidth = CAR_WIDTH / 8;
            int aiSideWHeight = CAR_HEIGHT / 4;
            int aiSideWY = aiWsY + aiWsHeight / 2 - aiSideWHeight / 2 + 10; // Centered vertically
            int aiSideWXOffset = CAR_WIDTH / 2 - 5 - aiSideWWidth / 2; // Offset from center
            Rectangle(memDC, -aiSideWXOffset - aiSideWWidth / 2, aiSideWY, -aiSideWXOffset + aiSideWWidth / 2, aiSideWY + aiSideWHeight); // Left
            Rectangle(memDC, aiSideWXOffset - aiSideWWidth / 2, aiSideWY, aiSideWXOffset + aiSideWWidth / 2, aiSideWY + aiSideWHeight); // Right
            SelectObject(memDC, oldAiWinBrush);
            DeleteObject(aiWinBrush);

            // Draw AI Tyres (relative coords)
            HBRUSH aiTyreBrush = CreateSolidBrush(RGB(0, 0, 0));
            HGDIOBJ oldAiTyreBrush = SelectObject(memDC, aiTyreBrush);
            int aiTyreWidth = 10;
            int aiTyreHeight = 15;

            // Keeping original vertical positions:
            int aiFrontTyreY = -CAR_HEIGHT / 2 + 15; // Original front Y position
            int aiRearTyreY = CAR_HEIGHT / 2 - aiTyreHeight - 5; // Original rear Y position

            // Adjust horizontal positions to stick to the edges of the car
            int aiLeftTyreX = -CAR_WIDTH / 2; // Left edge of the car
            int aiRightTyreX = CAR_WIDTH / 2;  // Right edge of the car

            // Draw tyres at the left and right positions
            // Front Left Tyre
            Rectangle(memDC, aiLeftTyreX - aiTyreWidth / 2, aiFrontTyreY, aiLeftTyreX + aiTyreWidth / 2, aiFrontTyreY + aiTyreHeight); // FL
            // Front Right Tyre
            Rectangle(memDC, aiRightTyreX - aiTyreWidth / 2, aiFrontTyreY, aiRightTyreX + aiTyreWidth / 2, aiFrontTyreY + aiTyreHeight); // FR
            // Rear Left Tyre
            Rectangle(memDC, aiLeftTyreX - aiTyreWidth / 2, aiRearTyreY, aiLeftTyreX + aiTyreWidth / 2, aiRearTyreY + aiTyreHeight); // RL
            // Rear Right Tyre
            Rectangle(memDC, aiRightTyreX - aiTyreWidth / 2, aiRearTyreY, aiRightTyreX + aiTyreWidth / 2, aiRearTyreY + aiTyreHeight); // RR

            SelectObject(memDC, oldAiTyreBrush);
            DeleteObject(aiTyreBrush);

            // Restore AI DC state
            RestoreDC(memDC, savedDCAi);
            SelectObject(memDC, oldBlueBrush);
            DeleteObject(blueBrush);


            // --- Draw Overlay Text ---
            SetBkMode(memDC, TRANSPARENT); // Make text background transparent

            // Timer display (only before game starts)
            if (!gameStarted && timer > 0)
            {
                char timerText[10];
                // Display seconds correctly (integer division), ensure >= 1
                int secondsLeft = max(1, (timer + FPS - 1) / FPS);
                sprintf_s(timerText, "%d", secondsLeft);
                SetTextColor(memDC, RGB(255, 255, 0)); // Yellow countdown
                HFONT hFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
                HFONT oldFont = (HFONT)SelectObject(memDC, hFont);
                SetTextAlign(memDC, TA_CENTER | TA_BASELINE); // Center align text
                TextOutA(memDC, WIDTH / 2, HEIGHT / 2, timerText, strlen(timerText));
                SetTextAlign(memDC, TA_LEFT | TA_TOP); // Reset alignment
                SelectObject(memDC, oldFont); // Restore old font
                DeleteObject(hFont); // Delete created font
            }
            else if (!gameStarted && timer <= 0) {
                // Optionally display "GO!" briefly
            }


            // God Mode Status Display
            if (godMode)
            {
                SetTextColor(memDC, RGB(0, 255, 0)); // Green text for God Mode
                HFONT hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
                HFONT oldFont = (HFONT)SelectObject(memDC, hFont);
                TextOutA(memDC, 10, 10, "God Mode ON (G)", 15);
                SelectObject(memDC, oldFont);
                DeleteObject(hFont);
            }

            // --- Double Buffering End ---
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

            // --- Cleanup ---
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
        }
        break; // End WM_PAINT
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_F1)
        {
            MessageBoxW(hWnd, L"2D Racing Game 3.0 Programmed in C++ Win32 API (491 lines of code) by Entisoft Software (c) Evans Thorpemorton", L"About", MB_OK | MB_ICONINFORMATION); // orig 395 lines
        }
        //break;
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        break;

        // Add WM_ERASEBKGND to prevent default background flicker
    case WM_ERASEBKGND:
        return 1; // Indicate that we handled background erasing (by not doing it)

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}