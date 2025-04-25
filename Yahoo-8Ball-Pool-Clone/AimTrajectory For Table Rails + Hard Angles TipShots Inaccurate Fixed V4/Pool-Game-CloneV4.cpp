#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <cmath>
#include <string>
#include <sstream> // Required for wostringstream
#include <algorithm> // Required for std::max, std::min
#include <ctime>    // Required for srand, time
#include <cstdlib> // Required for srand, rand (often included by others, but good practice)
#include <commctrl.h> // Needed for radio buttons etc. in dialog (if using native controls)
#include "resource.h"

#pragma comment(lib, "Comctl32.lib") // Link against common controls library
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// --- Constants ---
const float PI = 3.1415926535f;
const float BALL_RADIUS = 10.0f;
const float TABLE_LEFT = 100.0f;
const float TABLE_TOP = 100.0f;
const float TABLE_WIDTH = 700.0f;
const float TABLE_HEIGHT = 350.0f;
const float TABLE_RIGHT = TABLE_LEFT + TABLE_WIDTH;
const float TABLE_BOTTOM = TABLE_TOP + TABLE_HEIGHT;
const float CUSHION_THICKNESS = 20.0f;
const float HOLE_VISUAL_RADIUS = 22.0f; // Visual size of the hole
const float POCKET_RADIUS = HOLE_VISUAL_RADIUS; // Make detection radius match visual size (or slightly larger)
const float MAX_SHOT_POWER = 15.0f;
const float FRICTION = 0.985f; // Friction factor per frame
const float MIN_VELOCITY_SQ = 0.01f * 0.01f; // Stop balls below this squared velocity
const float HEADSTRING_X = TABLE_LEFT + TABLE_WIDTH * 0.30f; // 30% line
const float RACK_POS_X = TABLE_LEFT + TABLE_WIDTH * 0.65f; // 65% line for rack apex
const float RACK_POS_Y = TABLE_TOP + TABLE_HEIGHT / 2.0f;
const UINT ID_TIMER = 1;
const int TARGET_FPS = 60; // Target frames per second for timer

// --- Enums ---
// --- MODIFIED/NEW Enums ---
enum GameState {
    SHOWING_DIALOG,     // NEW: Game is waiting for initial dialog input
    PRE_BREAK_PLACEMENT,// Player placing cue ball for break
    BREAKING,           // Player is aiming/shooting the break shot
    AIMING,             // Player is aiming
    AI_THINKING,        // NEW: AI is calculating its move
    SHOT_IN_PROGRESS,   // Balls are moving
    ASSIGNING_BALLS,    // Turn after break where ball types are assigned
    PLAYER1_TURN,
    PLAYER2_TURN,
    BALL_IN_HAND_P1,
    BALL_IN_HAND_P2,
    GAME_OVER
};

enum BallType {
    NONE,
    SOLID,  // Yellow (1-7)
    STRIPE, // Red (9-15)
    EIGHT_BALL, // Black (8)
    CUE_BALL // White (0)
};

// NEW Enums for Game Mode and AI Difficulty
enum GameMode {
    HUMAN_VS_HUMAN,
    HUMAN_VS_AI
};

enum AIDifficulty {
    EASY,
    MEDIUM,
    HARD
};

// --- Structs ---
struct Ball {
    int id;             // 0=Cue, 1-7=Solid, 8=Eight, 9-15=Stripe
    BallType type;
    float x, y;
    float vx, vy;
    D2D1_COLOR_F color;
    bool isPocketed;
};

struct PlayerInfo {
    BallType assignedType;
    int ballsPocketedCount;
    std::wstring name;
};

// --- Global Variables ---

// Direct2D & DirectWrite
ID2D1Factory* pFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
IDWriteFactory* pDWriteFactory = nullptr;
IDWriteTextFormat* pTextFormat = nullptr;
IDWriteTextFormat* pLargeTextFormat = nullptr; // For "Foul!"

// Game State
HWND hwndMain = nullptr;
GameState currentGameState = SHOWING_DIALOG; // Start by showing dialog
std::vector<Ball> balls;
int currentPlayer = 1; // 1 or 2
PlayerInfo player1Info = { BallType::NONE, 0, L"Player 1" };
PlayerInfo player2Info = { BallType::NONE, 0, L"CPU" }; // Default P2 name
bool foulCommitted = false;
std::wstring gameOverMessage = L"";
bool firstBallPocketedAfterBreak = false;
std::vector<int> pocketedThisTurn;

// NEW Game Mode/AI Globals
GameMode gameMode = HUMAN_VS_HUMAN; // Default mode
AIDifficulty aiDifficulty = MEDIUM; // Default difficulty
bool isPlayer2AI = false;           // Is Player 2 controlled by AI?
bool aiTurnPending = false;         // Flag: AI needs to take its turn when possible
// bool aiIsThinking = false;       // Replaced by AI_THINKING game state

// Input & Aiming
POINT ptMouse = { 0, 0 };
bool isAiming = false;
bool isDraggingCueBall = false;
bool isSettingEnglish = false;
D2D1_POINT_2F aimStartPoint = { 0, 0 };
float cueAngle = 0.0f;
float shotPower = 0.0f;
float cueSpinX = 0.0f; // Range -1 to 1
float cueSpinY = 0.0f; // Range -1 to 1

// UI Element Positions
D2D1_RECT_F powerMeterRect = { TABLE_RIGHT + CUSHION_THICKNESS + 10, TABLE_TOP, TABLE_RIGHT + CUSHION_THICKNESS + 40, TABLE_BOTTOM };
D2D1_RECT_F spinIndicatorRect = { TABLE_LEFT - CUSHION_THICKNESS - 60, TABLE_TOP + 20, TABLE_LEFT - CUSHION_THICKNESS - 20, TABLE_TOP + 60 }; // Circle area
D2D1_POINT_2F spinIndicatorCenter = { spinIndicatorRect.left + (spinIndicatorRect.right - spinIndicatorRect.left) / 2.0f, spinIndicatorRect.top + (spinIndicatorRect.bottom - spinIndicatorRect.top) / 2.0f };
float spinIndicatorRadius = (spinIndicatorRect.right - spinIndicatorRect.left) / 2.0f;
D2D1_RECT_F pocketedBallsBarRect = { TABLE_LEFT, TABLE_BOTTOM + CUSHION_THICKNESS + 30, TABLE_RIGHT, TABLE_BOTTOM + CUSHION_THICKNESS + 70 };

// Corrected Pocket Center Positions (aligned with table corners/edges)
const D2D1_POINT_2F pocketPositions[6] = {
    {TABLE_LEFT, TABLE_TOP},                           // Top-Left
    {TABLE_LEFT + TABLE_WIDTH / 2.0f, TABLE_TOP},      // Top-Middle
    {TABLE_RIGHT, TABLE_TOP},                          // Top-Right
    {TABLE_LEFT, TABLE_BOTTOM},                        // Bottom-Left
    {TABLE_LEFT + TABLE_WIDTH / 2.0f, TABLE_BOTTOM},   // Bottom-Middle
    {TABLE_RIGHT, TABLE_BOTTOM}                        // Bottom-Right
};

// Colors
const D2D1_COLOR_F TABLE_COLOR = D2D1::ColorF(0.0f, 0.5f, 0.1f); // Darker Green
const D2D1_COLOR_F CUSHION_COLOR = D2D1::ColorF(D2D1::ColorF::Red);
const D2D1_COLOR_F POCKET_COLOR = D2D1::ColorF(D2D1::ColorF::Black);
const D2D1_COLOR_F CUE_BALL_COLOR = D2D1::ColorF(D2D1::ColorF::White);
const D2D1_COLOR_F EIGHT_BALL_COLOR = D2D1::ColorF(D2D1::ColorF::Black);
const D2D1_COLOR_F SOLID_COLOR = D2D1::ColorF(D2D1::ColorF::Yellow); // Solids = Yellow
const D2D1_COLOR_F STRIPE_COLOR = D2D1::ColorF(D2D1::ColorF::Red);   // Stripes = Red
const D2D1_COLOR_F AIM_LINE_COLOR = D2D1::ColorF(D2D1::ColorF::White, 0.7f); // Semi-transparent white
const D2D1_COLOR_F FOUL_TEXT_COLOR = D2D1::ColorF(D2D1::ColorF::Red);
const D2D1_COLOR_F TURN_ARROW_COLOR = D2D1::ColorF(D2D1::ColorF::Blue);
const D2D1_COLOR_F ENGLISH_DOT_COLOR = D2D1::ColorF(D2D1::ColorF::Red);
const D2D1_COLOR_F UI_TEXT_COLOR = D2D1::ColorF(D2D1::ColorF::Black);

// --- Forward Declarations ---
HRESULT CreateDeviceResources();
void DiscardDeviceResources();
void OnPaint();
void OnResize(UINT width, UINT height);
void InitGame();
void GameUpdate();
void UpdatePhysics();
void CheckCollisions();
bool CheckPockets(); // Returns true if any ball was pocketed
void ProcessShotResults();
void ApplyShot(float power, float angle, float spinX, float spinY);
void RespawnCueBall(bool behindHeadstring);
bool AreBallsMoving();
void SwitchTurns();
void AssignPlayerBallTypes(BallType firstPocketedType);
void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed);
Ball* GetBallById(int id);
Ball* GetCueBall();

// Drawing Functions
void DrawScene(ID2D1RenderTarget* pRT);
void DrawTable(ID2D1RenderTarget* pRT);
void DrawBalls(ID2D1RenderTarget* pRT);
void DrawCueStick(ID2D1RenderTarget* pRT);
void DrawAimingAids(ID2D1RenderTarget* pRT);
void DrawUI(ID2D1RenderTarget* pRT);
void DrawPowerMeter(ID2D1RenderTarget* pRT);
void DrawSpinIndicator(ID2D1RenderTarget* pRT);
void DrawPocketedBallsIndicator(ID2D1RenderTarget* pRT);
void DrawBallInHandIndicator(ID2D1RenderTarget* pRT);

// Helper Functions
float GetDistance(float x1, float y1, float x2, float y2);
float GetDistanceSq(float x1, float y1, float x2, float y2);
bool IsValidCueBallPosition(float x, float y, bool checkHeadstring);
template <typename T> void SafeRelease(T** ppT);

// --- NEW Forward Declarations ---

// AI Related
struct AIShotInfo; // Define below
void TriggerAIMove();
void AIMakeDecision();
void AIPlaceCueBall();
AIShotInfo AIFindBestShot();
AIShotInfo EvaluateShot(Ball* targetBall, int pocketIndex);
bool IsPathClear(D2D1_POINT_2F start, D2D1_POINT_2F end, int ignoredBallId1, int ignoredBallId2);
Ball* FindFirstHitBall(D2D1_POINT_2F start, float angle, float& hitDistSq); // Added hitDistSq output
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist);
D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, int pocketIndex);
bool IsValidAIAimAngle(float angle); // Basic check

// Dialog Related
INT_PTR CALLBACK NewGameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ShowNewGameDialog(HINSTANCE hInstance);
void ResetGame(HINSTANCE hInstance); // Function to handle F2 reset

// --- Forward Declaration for Window Procedure --- <<< Add this line HERE
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- NEW Struct for AI Shot Evaluation ---
struct AIShotInfo {
    bool possible = false;          // Is this shot considered viable?
    Ball* targetBall = nullptr;     // Which ball to hit
    int pocketIndex = -1;           // Which pocket to aim for (0-5)
    D2D1_POINT_2F ghostBallPos = { 0,0 }; // Where cue ball needs to hit target ball
    float angle = 0.0f;             // Calculated shot angle
    float power = 0.0f;             // Calculated shot power
    float score = -1.0f;            // Score for this shot (higher is better)
    bool involves8Ball = false;     // Is the target the 8-ball?
};

// --- NEW Dialog Procedure ---
INT_PTR CALLBACK NewGameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        // Set initial state based on current global settings (or defaults)
        CheckRadioButton(hDlg, IDC_RADIO_2P, IDC_RADIO_CPU, (gameMode == HUMAN_VS_HUMAN) ? IDC_RADIO_2P : IDC_RADIO_CPU);

        CheckRadioButton(hDlg, IDC_RADIO_EASY, IDC_RADIO_HARD,
            (aiDifficulty == EASY) ? IDC_RADIO_EASY : ((aiDifficulty == MEDIUM) ? IDC_RADIO_MEDIUM : IDC_RADIO_HARD));

        // Enable/Disable AI group based on initial mode
        EnableWindow(GetDlgItem(hDlg, IDC_GROUP_AI), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_EASY), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_MEDIUM), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_HARD), gameMode == HUMAN_VS_AI);

        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_RADIO_2P:
        case IDC_RADIO_CPU:
        {
            bool isCPU = IsDlgButtonChecked(hDlg, IDC_RADIO_CPU) == BST_CHECKED;
            // Enable/Disable AI group controls based on selection
            EnableWindow(GetDlgItem(hDlg, IDC_GROUP_AI), isCPU);
            EnableWindow(GetDlgItem(hDlg, IDC_RADIO_EASY), isCPU);
            EnableWindow(GetDlgItem(hDlg, IDC_RADIO_MEDIUM), isCPU);
            EnableWindow(GetDlgItem(hDlg, IDC_RADIO_HARD), isCPU);
        }
        return (INT_PTR)TRUE;

        case IDOK:
            // Retrieve selected options and store in global variables
            if (IsDlgButtonChecked(hDlg, IDC_RADIO_CPU) == BST_CHECKED) {
                gameMode = HUMAN_VS_AI;
                if (IsDlgButtonChecked(hDlg, IDC_RADIO_EASY) == BST_CHECKED) aiDifficulty = EASY;
                else if (IsDlgButtonChecked(hDlg, IDC_RADIO_MEDIUM) == BST_CHECKED) aiDifficulty = MEDIUM;
                else if (IsDlgButtonChecked(hDlg, IDC_RADIO_HARD) == BST_CHECKED) aiDifficulty = HARD;
            }
            else {
                gameMode = HUMAN_VS_HUMAN;
            }
            EndDialog(hDlg, IDOK); // Close dialog, return IDOK
            return (INT_PTR)TRUE;

        case IDCANCEL: // Handle Cancel or closing the dialog
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break; // End WM_COMMAND
    }
    return (INT_PTR)FALSE; // Default processing
}

// --- NEW Helper to Show Dialog ---
void ShowNewGameDialog(HINSTANCE hInstance) {
    if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_NEWGAMEDLG), hwndMain, NewGameDialogProc, 0) == IDOK) {
        // User clicked Start, reset game with new settings
        isPlayer2AI = (gameMode == HUMAN_VS_AI); // Update AI flag
        if (isPlayer2AI) {
            switch (aiDifficulty) {
            case EASY: player2Info.name = L"CPU (Easy)"; break;
            case MEDIUM: player2Info.name = L"CPU (Medium)"; break;
            case HARD: player2Info.name = L"CPU (Hard)"; break;
            }
        }
        else {
            player2Info.name = L"Player 2";
        }
        // Update window title
        std::wstring windowTitle = L"Direct2D 8-Ball Pool";
        if (gameMode == HUMAN_VS_HUMAN) windowTitle += L" (Human vs Human)";
        else windowTitle += L" (Human vs " + player2Info.name + L")";
        SetWindowText(hwndMain, windowTitle.c_str());

        InitGame(); // Re-initialize game logic & board
        InvalidateRect(hwndMain, NULL, TRUE); // Force redraw
    }
    else {
        // User cancelled dialog - maybe just resume game? Or exit?
        // For simplicity, we do nothing, game continues as it was.
        // To exit on cancel from F2, would need more complex state management.
    }
}

// --- NEW Reset Game Function ---
void ResetGame(HINSTANCE hInstance) {
    // Call the helper function to show the dialog and re-init if OK clicked
    ShowNewGameDialog(hInstance);
}

// --- WinMain ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    if (FAILED(CoInitialize(NULL))) {
        MessageBox(NULL, L"COM Initialization Failed.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // --- NEW: Show configuration dialog FIRST ---
    if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_NEWGAMEDLG), NULL, NewGameDialogProc, 0) != IDOK) {
        // User cancelled the dialog
        CoUninitialize();
        return 0; // Exit gracefully if dialog cancelled
    }
    // Global gameMode and aiDifficulty are now set by the DialogProc

    // Set AI flag based on game mode
    isPlayer2AI = (gameMode == HUMAN_VS_AI);
    if (isPlayer2AI) {
        switch (aiDifficulty) {
        case EASY: player2Info.name = L"CPU (Easy)"; break;
        case MEDIUM: player2Info.name = L"CPU (Medium)"; break;
        case HARD: player2Info.name = L"CPU (Hard)"; break;
        }
    }
    else {
        player2Info.name = L"Player 2";
    }
    // --- End of Dialog Logic ---


    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Direct2D_8BallPool";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Window Registration Failed.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return -1;
    }

    // --- Change Window Title based on mode ---
    std::wstring windowTitle = L"Direct2D 8-Ball Pool";
    if (gameMode == HUMAN_VS_HUMAN) windowTitle += L" (Human vs Human)";
    else windowTitle += L" (Human vs " + player2Info.name + L")";


    hwndMain = CreateWindowEx(
        0, L"Direct2D_8BallPool", windowTitle.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        NULL, NULL, hInstance, NULL
    );

    if (!hwndMain) {
        MessageBox(NULL, L"Window Creation Failed.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return -1;
    }

    // Initialize Direct2D Resources AFTER window creation
    if (FAILED(CreateDeviceResources())) {
        MessageBox(NULL, L"Failed to create Direct2D resources.", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwndMain);
        CoUninitialize();
        return -1;
    }

    InitGame(); // Initialize game state AFTER resources are ready & mode is set

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    if (!SetTimer(hwndMain, ID_TIMER, 1000 / TARGET_FPS, NULL)) {
        MessageBox(NULL, L"Could not SetTimer().", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwndMain);
        CoUninitialize();
        return -1;
    }

    MSG msg = { };
    // --- Modified Main Loop ---
    // Handles the case where the game starts in SHOWING_DIALOG state (handled now before loop)
    // or gets reset to it via F2. The main loop runs normally once game starts.
    while (GetMessage(&msg, NULL, 0, 0)) {
        // We might need modeless dialog handling here if F2 shows dialog
        // while window is active, but DialogBoxParam is modal.
        // Let's assume F2 hides main window, shows dialog, then restarts game loop.
        // Simpler: F2 calls ResetGame which calls DialogBoxParam (modal) then InitGame.
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    KillTimer(hwndMain, ID_TIMER);
    DiscardDeviceResources();
    CoUninitialize();

    return (int)msg.wParam;
}

// --- WndProc ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // Resources are now created in WinMain after CreateWindowEx
        return 0;

    case WM_PAINT:
        OnPaint();
        // Validate the entire window region after painting
        ValidateRect(hwnd, NULL);
        return 0;

    case WM_SIZE: {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        OnResize(width, height);
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER) {
            GameUpdate(); // Update game logic and physics
            InvalidateRect(hwnd, NULL, FALSE); // Request redraw
        }
        return 0;

        // --- NEW: Handle F2 Key for Reset ---
    case WM_KEYDOWN:
        if (wParam == VK_F2) {
            // Get HINSTANCE from the window handle
            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            ResetGame(hInstance); // Call reset function
        }
        return 0; // Indicate key was processed

    case WM_MOUSEMOVE: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        Ball* cueBall = GetCueBall();
        if (!cueBall) return 0;

        // Logic for dragging cue ball during ball-in-hand (unchanged)
        if (isDraggingCueBall && (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT)) {
            bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
            if (IsValidCueBallPosition((float)ptMouse.x, (float)ptMouse.y, behindHeadstring)) {
                cueBall->x = (float)ptMouse.x;
                cueBall->y = (float)ptMouse.y;
                cueBall->vx = cueBall->vy = 0; // Ensure it's stopped
            }
        }
        // Logic for aiming drag (unchanged math, just context)
        else if (isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) {
            float dx = (float)ptMouse.x - cueBall->x;
            float dy = (float)ptMouse.y - cueBall->y;
            // Prevent setting angle if mouse is exactly on cue ball center
            if (dx != 0 || dy != 0) {
                cueAngle = atan2f(dy, dx);
            }
            // Calculate power based on distance pulled back from the initial click point (aimStartPoint)
            float pullDist = GetDistance((float)ptMouse.x, (float)ptMouse.y, aimStartPoint.x, aimStartPoint.y);
            // Scale power more aggressively, maybe? Or keep scale factor 10.0
            shotPower = std::min(pullDist / 10.0f, MAX_SHOT_POWER); // Scale power, clamp to max
        }
        // Logic for setting english (unchanged)
        else if (isSettingEnglish) {
            float dx = (float)ptMouse.x - spinIndicatorCenter.x;
            float dy = (float)ptMouse.y - spinIndicatorCenter.y;
            float dist = GetDistance(dx, dy, 0, 0);
            if (dist > spinIndicatorRadius) { // Clamp to edge
                dx *= spinIndicatorRadius / dist;
                dy *= spinIndicatorRadius / dist;
            }
            cueSpinX = dx / spinIndicatorRadius; // Normalize to -1 to 1
            cueSpinY = dy / spinIndicatorRadius;
        }
        // InvalidateRect is handled by WM_TIMER
        return 0;
    }

    case WM_LBUTTONDOWN: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        // Check if clicking on Spin Indicator (unchanged)
        float spinDistSq = GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, spinIndicatorCenter.x, spinIndicatorCenter.y);
        if (spinDistSq < spinIndicatorRadius * spinIndicatorRadius) {
            isSettingEnglish = true;
            // Update spin immediately on click
            float dx = (float)ptMouse.x - spinIndicatorCenter.x;
            float dy = (float)ptMouse.y - spinIndicatorCenter.y;
            cueSpinX = dx / spinIndicatorRadius;
            cueSpinY = dy / spinIndicatorRadius;
            return 0; // Don't process other clicks if setting english
        }


        Ball* cueBall = GetCueBall();
        if (!cueBall) return 0;

        // Logic for Ball-in-Hand placement click (unchanged)
        if (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT) {
            float distSq = GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y);
            if (distSq < BALL_RADIUS * BALL_RADIUS * 4) { // Allow clicking near the ball to start drag
                isDraggingCueBall = true;
            }
            else { // If clicking elsewhere on the table (and valid), place the ball
                bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
                if (IsValidCueBallPosition((float)ptMouse.x, (float)ptMouse.y, behindHeadstring)) {
                    cueBall->x = (float)ptMouse.x;
                    cueBall->y = (float)ptMouse.y;
                    cueBall->vx = cueBall->vy = 0;
                    isDraggingCueBall = false;
                    // Transition state appropriate to ending placement
                    if (currentGameState == PRE_BREAK_PLACEMENT) {
                        // Depends on who is breaking
                        currentGameState = BREAKING;
                        // If AI was breaking, aiTurnPending should still be true
                    }
                    else if (currentGameState == BALL_IN_HAND_P1) {
                        currentGameState = PLAYER1_TURN;
                    }
                    else if (currentGameState == BALL_IN_HAND_P2) {
                        // If AI placed ball, AIMakeDecision should have been called? Or trigger now?
                        // Assuming SwitchTurns/Respawn set aiTurnPending correctly earlier
                        currentGameState = PLAYER2_TURN; // Ready for AI/Human P2 to aim
                    }
                }
            }
        }
        // --- MODIFIED: Logic for starting aim ---
        else if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN || currentGameState == BREAKING) {
            // Allow initiating aim by clicking in a larger radius around the cue ball
            float distSq = GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y);
            // Increased radius check (e.g., 5x ball radius squared)
            if (distSq < BALL_RADIUS * BALL_RADIUS * 25) { // Click somewhat close to cue ball
                isAiming = true;
                aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y); // Store where aiming drag started
                shotPower = 0; // Reset power
                // Transition to AIMING state (if not already BREAKING)
                if (currentGameState != BREAKING) {
                    currentGameState = AIMING;
                }
                // Set initial cueAngle based on click relative to ball, for immediate feedback
                float dx = (float)ptMouse.x - cueBall->x;
                float dy = (float)ptMouse.y - cueBall->y;
                if (dx != 0 || dy != 0) {
                    cueAngle = atan2f(dy, dx);
                    // If starting aim by clicking, maybe point stick towards mouse initially?
                    // Current logic updates angle on MOUSEMOVE anyway.
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        if (isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) {
            isAiming = false; // Stop the aiming drag visual state

            // --- MODIFIED: Increased threshold for taking shot ---
            if (shotPower > 0.15f) { // Only shoot if power is significant enough
                // Prevent player from shooting if it's AI's turn calculation phase
                if (currentGameState != AI_THINKING) {
                    ApplyShot(shotPower, cueAngle, cueSpinX, cueSpinY);
                    currentGameState = SHOT_IN_PROGRESS;
                    foulCommitted = false; // Reset foul flag for the new shot
                    pocketedThisTurn.clear();
                }
            }
            // If shotPower is too low, reset state back to player's turn
            else if (currentGameState != AI_THINKING) {
                // If no power, revert state back without shooting
                if (currentGameState == BREAKING) {
                    // Still breaking state if power was too low
                }
                else {
                    // Revert to appropriate player turn state
                    currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                    // Clear pending AI turn flag if it somehow got set during a zero-power human shot attempt
                    if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = false;
                }
            }
            shotPower = 0; // Reset power indicator regardless of shot taken
        }

        // Logic for releasing cue ball after dragging (unchanged)
        if (isDraggingCueBall) {
            isDraggingCueBall = false;
            // After placing the ball, transition state if needed (state might already be set by click placement)
            if (currentGameState == PRE_BREAK_PLACEMENT) {
                currentGameState = BREAKING;
            }
            else if (currentGameState == BALL_IN_HAND_P1) {
                currentGameState = PLAYER1_TURN;
            }
            else if (currentGameState == BALL_IN_HAND_P2) {
                currentGameState = PLAYER2_TURN;
                // If AI placed, aiTurnPending should trigger AI on next GameUpdate
            }
        }
        // Logic for releasing english setting (unchanged)
        if (isSettingEnglish) {
            isSettingEnglish = false;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- Direct2D Resource Management ---

HRESULT CreateDeviceResources() {
    HRESULT hr = S_OK;

    // Create Direct2D Factory
    if (!pFactory) {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
        if (FAILED(hr)) return hr;
    }

    // Create DirectWrite Factory
    if (!pDWriteFactory) {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&pDWriteFactory)
        );
        if (FAILED(hr)) return hr;
    }

    // Create Text Formats
    if (!pTextFormat && pDWriteFactory) {
        hr = pDWriteFactory->CreateTextFormat(
            L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            16.0f, L"en-us", &pTextFormat
        );
        if (FAILED(hr)) return hr;
        // Center align text
        pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!pLargeTextFormat && pDWriteFactory) {
        hr = pDWriteFactory->CreateTextFormat(
            L"Impact", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            48.0f, L"en-us", &pLargeTextFormat
        );
        if (FAILED(hr)) return hr;
        pLargeTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); // Align left
        pLargeTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }


    // Create Render Target (needs valid hwnd)
    if (!pRenderTarget && hwndMain) {
        RECT rc;
        GetClientRect(hwndMain, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwndMain, size),
            &pRenderTarget
        );
        if (FAILED(hr)) {
            // If failed, release factories if they were created in this call
            SafeRelease(&pTextFormat);
            SafeRelease(&pLargeTextFormat);
            SafeRelease(&pDWriteFactory);
            SafeRelease(&pFactory);
            pRenderTarget = nullptr; // Ensure it's null on failure
            return hr;
        }
    }

    return hr;
}

void DiscardDeviceResources() {
    SafeRelease(&pRenderTarget);
    SafeRelease(&pTextFormat);
    SafeRelease(&pLargeTextFormat);
    SafeRelease(&pDWriteFactory);
    // Keep pFactory until application exit? Or release here too? Let's release.
    SafeRelease(&pFactory);
}

void OnResize(UINT width, UINT height) {
    if (pRenderTarget) {
        D2D1_SIZE_U size = D2D1::SizeU(width, height);
        pRenderTarget->Resize(size); // Ignore HRESULT for simplicity here
    }
}

// --- Game Initialization ---
void InitGame() {
    srand((unsigned int)time(NULL)); // Seed random number generator

    // --- Ensure pocketed list is clear from the absolute start ---
    pocketedThisTurn.clear();

    balls.clear(); // Clear existing balls

    // Reset Player Info (Names should be set by Dialog/wWinMain/ResetGame)
    player1Info.assignedType = BallType::NONE;
    player1Info.ballsPocketedCount = 0;
    // Player 1 Name usually remains "Player 1"
    player2Info.assignedType = BallType::NONE;
    player2Info.ballsPocketedCount = 0;
    // Player 2 Name is set based on gameMode in ShowNewGameDialog

    // Create Cue Ball (ID 0)
    // Initial position will be set during PRE_BREAK_PLACEMENT state
    balls.push_back({ 0, BallType::CUE_BALL, TABLE_LEFT + TABLE_WIDTH * 0.15f, RACK_POS_Y, 0, 0, CUE_BALL_COLOR, false });

    // --- Create Object Balls (Temporary List) ---
    std::vector<Ball> objectBalls;
    // Solids (1-7, Yellow)
    for (int i = 1; i <= 7; ++i) {
        objectBalls.push_back({ i, BallType::SOLID, 0, 0, 0, 0, SOLID_COLOR, false });
    }
    // Stripes (9-15, Red)
    for (int i = 9; i <= 15; ++i) {
        objectBalls.push_back({ i, BallType::STRIPE, 0, 0, 0, 0, STRIPE_COLOR, false });
    }
    // 8-Ball (ID 8) - Add it to the list to be placed
    objectBalls.push_back({ 8, BallType::EIGHT_BALL, 0, 0, 0, 0, EIGHT_BALL_COLOR, false });


    // --- Racking Logic (Improved) ---
    float spacingX = BALL_RADIUS * 2.0f * 0.866f; // cos(30) for horizontal spacing
    float spacingY = BALL_RADIUS * 2.0f * 1.0f;   // Vertical spacing

    // Define rack positions (0-14 indices corresponding to triangle spots)
    D2D1_POINT_2F rackPositions[15];
    int rackIndex = 0;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col <= row; ++col) {
            if (rackIndex >= 15) break;
            float x = RACK_POS_X + row * spacingX;
            float y = RACK_POS_Y + (col - row / 2.0f) * spacingY;
            rackPositions[rackIndex++] = D2D1::Point2F(x, y);
        }
    }

    // Separate 8-ball
    Ball eightBall;
    std::vector<Ball> otherBalls; // Solids and Stripes
    bool eightBallFound = false;
    for (const auto& ball : objectBalls) {
        if (ball.id == 8) {
            eightBall = ball;
            eightBallFound = true;
        }
        else {
            otherBalls.push_back(ball);
        }
    }
    // Ensure 8 ball was actually created (should always be true)
    if (!eightBallFound) {
        // Handle error - perhaps recreate it? For now, proceed.
        eightBall = { 8, BallType::EIGHT_BALL, 0, 0, 0, 0, EIGHT_BALL_COLOR, false };
    }


    // Shuffle the other 14 balls
    // Use std::shuffle if available (C++11 and later) for better randomness
    // std::random_device rd;
    // std::mt19937 g(rd());
    // std::shuffle(otherBalls.begin(), otherBalls.end(), g);
    std::random_shuffle(otherBalls.begin(), otherBalls.end()); // Using deprecated for now

    // --- Place balls into the main 'balls' vector in rack order ---
    // Important: Add the cue ball (already created) first.
    // (Cue ball added at the start of the function now)

    // 1. Place the 8-ball in its fixed position (index 4 for the 3rd row center)
    int eightBallRackIndex = 4;
    eightBall.x = rackPositions[eightBallRackIndex].x;
    eightBall.y = rackPositions[eightBallRackIndex].y;
    eightBall.vx = 0;
    eightBall.vy = 0;
    eightBall.isPocketed = false;
    balls.push_back(eightBall); // Add 8 ball to the main vector

    // 2. Place the shuffled Solids and Stripes in the remaining spots
    int otherBallIdx = 0;
    for (int i = 0; i < 15; ++i) {
        if (i == eightBallRackIndex) continue; // Skip the 8-ball spot

        if (otherBallIdx < otherBalls.size()) {
            Ball& ballToPlace = otherBalls[otherBallIdx++];
            ballToPlace.x = rackPositions[i].x;
            ballToPlace.y = rackPositions[i].y;
            ballToPlace.vx = 0;
            ballToPlace.vy = 0;
            ballToPlace.isPocketed = false;
            balls.push_back(ballToPlace); // Add to the main game vector
        }
    }
    // --- End Racking Logic ---


    // --- Determine Who Breaks and Initial State ---
    if (isPlayer2AI) {
        // AI Mode: Randomly decide who breaks
        if ((rand() % 2) == 0) {
            // AI (Player 2) breaks
            currentPlayer = 2;
            currentGameState = PRE_BREAK_PLACEMENT; // AI needs to place ball first
            aiTurnPending = true; // Trigger AI logic
        }
        else {
            // Player 1 (Human) breaks
            currentPlayer = 1;
            currentGameState = PRE_BREAK_PLACEMENT; // Human places cue ball
            aiTurnPending = false;
        }
    }
    else {
        // Human vs Human, Player 1 breaks
        currentPlayer = 1;
        currentGameState = PRE_BREAK_PLACEMENT;
        aiTurnPending = false; // No AI involved
    }

    // Reset other relevant game state variables
    foulCommitted = false;
    gameOverMessage = L"";
    firstBallPocketedAfterBreak = false;
    // pocketedThisTurn cleared at start
    // Reset shot parameters and input flags
    shotPower = 0.0f;
    cueSpinX = 0.0f;
    cueSpinY = 0.0f;
    isAiming = false;
    isDraggingCueBall = false;
    isSettingEnglish = false;
    cueAngle = 0.0f; // Reset aim angle
}


// --- Game Loop ---
void GameUpdate() {
    if (currentGameState == SHOT_IN_PROGRESS) {
        UpdatePhysics();
        CheckCollisions();
        bool pocketed = CheckPockets(); // Store if any ball was pocketed

        if (!AreBallsMoving()) {
            ProcessShotResults(); // Determine next state based on what happened
        }
    }

    // --- NEW: Check if AI needs to act ---
    else if (aiTurnPending && !AreBallsMoving()) {
        // Check if it's genuinely AI's turn state and not mid-shot etc.
        if (currentGameState == PLAYER2_TURN || currentGameState == BREAKING || currentGameState == PRE_BREAK_PLACEMENT) {
            // Only trigger if AI is P2, it's their turn/break, and balls stopped
            if (isPlayer2AI && currentPlayer == 2) {
                // Transition state to show AI is thinking
                currentGameState = AI_THINKING;
                aiTurnPending = false; // Acknowledge the pending flag

                // --- Trigger AI Decision Making ---
                // In a real game loop, you might start a timer here or background thread.
                // For simplicity here, we call it directly. This might pause rendering
                // briefly if AI calculation is slow.
                AIMakeDecision(); // AI calculates and applies shot

                // AIMakeDecision should end by calling ApplyShot, which sets
                // currentGameState = SHOT_IN_PROGRESS
                // If AI fails to find a shot, need to handle that (e.g., pass turn - should be rare)
            }
            else {
                aiTurnPending = false; // Clear flag if conditions not met (e.g. P1's turn somehow)
            }
        }
        else {
            aiTurnPending = false; // Clear flag if not in a state where AI should shoot
        }
    }

    // Other states (AIMING, BALL_IN_HAND, etc.) are handled by input messages
}

// --- Physics and Collision ---
void UpdatePhysics() {
    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& b = balls[i];
        if (!b.isPocketed) {
            b.x += b.vx;
            b.y += b.vy;

            // Apply friction
            b.vx *= FRICTION;
            b.vy *= FRICTION;

            // Stop balls if velocity is very low
            if (GetDistanceSq(b.vx, b.vy, 0, 0) < MIN_VELOCITY_SQ) {
                b.vx = 0;
                b.vy = 0;
            }
        }
    }
}

void CheckCollisions() {
    // --- Corrected Collision Boundaries ---
    // These now represent the actual edges of the playable table surface
    float left = TABLE_LEFT;
    float right = TABLE_RIGHT;
    float top = TABLE_TOP;
    float bottom = TABLE_BOTTOM;

    // Define a radius around pocket centers to check if a ball is near a pocket mouth
    // Use a value slightly larger than the pocket radius to prevent clipping the edge
    const float pocketMouthCheckRadiusSq = (POCKET_RADIUS + BALL_RADIUS) * (POCKET_RADIUS + BALL_RADIUS) * 1.1f; // Check slightly larger area

    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& b1 = balls[i];
        if (b1.isPocketed) continue; // Skip balls already pocketed

        // --- Pre-calculate proximity to pocket centers ---
        // This avoids recalculating distances multiple times for wall checks
        bool nearPocket[6];
        for (int p = 0; p < 6; ++p) {
            nearPocket[p] = GetDistanceSq(b1.x, b1.y, pocketPositions[p].x, pocketPositions[p].y) < pocketMouthCheckRadiusSq;
        }
        // Individual pocket proximity flags for clarity in wall checks
        bool nearTopLeftPocket = nearPocket[0];
        bool nearTopMidPocket = nearPocket[1];
        bool nearTopRightPocket = nearPocket[2];
        bool nearBottomLeftPocket = nearPocket[3];
        bool nearBottomMidPocket = nearPocket[4];
        bool nearBottomRightPocket = nearPocket[5];


        // --- Ball-Wall Collisions (with Pocket Avoidance) ---
        bool collidedWall = false; // Track if any wall collision happened for spin effects

        // Left Wall
        if (b1.x - BALL_RADIUS < left) {
            // Don't bounce if near top-left or bottom-left pocket mouths
            if (!nearTopLeftPocket && !nearBottomLeftPocket) {
                b1.x = left + BALL_RADIUS;
                b1.vx *= -1.0f;
                collidedWall = true;
            } // else: Allow ball to continue towards pocket
        }
        // Right Wall
        if (b1.x + BALL_RADIUS > right) {
            // Don't bounce if near top-right or bottom-right pocket mouths
            if (!nearTopRightPocket && !nearBottomRightPocket) {
                b1.x = right - BALL_RADIUS;
                b1.vx *= -1.0f;
                collidedWall = true;
            } // else: Allow ball to continue towards pocket
        }
        // Top Wall
        if (b1.y - BALL_RADIUS < top) {
            // Don't bounce if near top-left, top-mid, or top-right pocket mouths
            if (!nearTopLeftPocket && !nearTopMidPocket && !nearTopRightPocket) {
                b1.y = top + BALL_RADIUS;
                b1.vy *= -1.0f;
                collidedWall = true;
            } // else: Allow ball to continue towards pocket
        }
        // Bottom Wall
        if (b1.y + BALL_RADIUS > bottom) {
            // Don't bounce if near bottom-left, bottom-mid, or bottom-right pocket mouths
            if (!nearBottomLeftPocket && !nearBottomMidPocket && !nearBottomRightPocket) {
                b1.y = bottom - BALL_RADIUS;
                b1.vy *= -1.0f;
                collidedWall = true;
            } // else: Allow ball to continue towards pocket
        }

        // Optional: Apply simplified spin effect on wall collision IF a bounce occurred
        if (collidedWall) {
            // Simple spin damping/effect (can be refined)
            // Side spin affects vertical velocity on horizontal collision & vice-versa
            if (b1.x <= left + BALL_RADIUS || b1.x >= right - BALL_RADIUS) { // Hit L/R wall
                b1.vy += cueSpinX * b1.vx * 0.05f; // Apply small vertical impulse based on side spin and horizontal velocity
            }
            if (b1.y <= top + BALL_RADIUS || b1.y >= bottom - BALL_RADIUS) { // Hit T/B wall
                b1.vx -= cueSpinY * b1.vy * 0.05f; // Apply small horizontal impulse based on top/bottom spin and vertical velocity
            }
            // Dampen spin after wall hit
            cueSpinX *= 0.7f; // Increase damping maybe
            cueSpinY *= 0.7f;
        }


        // --- Ball-Ball Collisions ---
        for (size_t j = i + 1; j < balls.size(); ++j) {
            Ball& b2 = balls[j];
            if (b2.isPocketed) continue; // Skip pocketed balls

            float dx = b2.x - b1.x;
            float dy = b2.y - b1.y;
            float distSq = dx * dx + dy * dy;
            float minDist = BALL_RADIUS * 2.0f;

            if (distSq > 0 && distSq < minDist * minDist) { // Check distance squared first
                float dist = sqrtf(distSq);
                float overlap = minDist - dist;

                // Normalize collision vector
                float nx = dx / dist;
                float ny = dy / dist;

                // Separate balls to prevent sticking
                // Move each ball half the overlap distance along the collision normal
                b1.x -= overlap * 0.5f * nx;
                b1.y -= overlap * 0.5f * ny;
                b2.x += overlap * 0.5f * nx;
                b2.y += overlap * 0.5f * ny;

                // Relative velocity
                float rvx = b1.vx - b2.vx;
                float rvy = b1.vy - b2.vy;

                // Dot product of relative velocity and collision normal
                // This represents the component of relative velocity along the collision line
                float velAlongNormal = rvx * nx + rvy * ny;

                // Only resolve collision if balls are moving towards each other (dot product > 0)
                if (velAlongNormal > 0) {
                    // Calculate impulse scalar (simplified - assumes equal mass, perfect elasticity=1.0)
                   // For perfect elastic collision, the impulse magnitude needed is velAlongNormal.
                   // Each ball gets half the impulse if masses are equal, but since we apply to both in opposite directions along the normal,
                   // the change in velocity for each along the normal is 'velAlongNormal'.
                    float impulse = velAlongNormal; // Simplified impulse magnitude along normal

                    // Apply impulse to velocities along the collision normal
                    b1.vx -= impulse * nx;
                    b1.vy -= impulse * ny;
                    b2.vx += impulse * nx;
                    b2.vy += impulse * ny;

                    // Apply spin transfer/effect (Very simplified)
                    if (b1.id == 0 || b2.id == 0) { // If cue ball involved
                        float spinEffectFactor = 0.08f; // Reduced factor maybe
                        // Simple model: Apply a small velocity change perpendicular to the normal based on spin
                        b1.vx += (cueSpinY * ny - cueSpinX * nx) * spinEffectFactor; // Spin effect
                        b1.vy += (cueSpinY * nx + cueSpinX * ny) * spinEffectFactor; // Spin effect (check signs/logic)

                        b2.vx -= (cueSpinY * ny - cueSpinX * nx) * spinEffectFactor;
                        b2.vy -= (cueSpinY * nx + cueSpinX * ny) * spinEffectFactor;

                        // Dampen spin after transfer
                        cueSpinX *= 0.85f;
                        cueSpinY *= 0.85f;
                    }
                }
            }
        } // End ball-ball collision loop
    } // End loop through balls
} // End CheckCollisions


bool CheckPockets() {
    bool ballPocketed = false;
    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& b = balls[i];
        if (!b.isPocketed) {
            for (int p = 0; p < 6; ++p) {
                float distSq = GetDistanceSq(b.x, b.y, pocketPositions[p].x, pocketPositions[p].y);
                if (distSq < POCKET_RADIUS * POCKET_RADIUS) {
                    b.isPocketed = true;
                    b.vx = b.vy = 0;
                    pocketedThisTurn.push_back(b.id); // Record pocketed ball ID
                    ballPocketed = true;
                    break; // No need to check other pockets for this ball
                }
            }
        }
    }
    return ballPocketed;
}

bool AreBallsMoving() {
    for (size_t i = 0; i < balls.size(); ++i) {
        if (!balls[i].isPocketed && (balls[i].vx != 0 || balls[i].vy != 0)) {
            return true;
        }
    }
    return false;
}

void RespawnCueBall(bool behindHeadstring) { // 'behindHeadstring' only relevant for initial break placement
    Ball* cueBall = GetCueBall();
    if (cueBall) {
        // Reset position to a default (AI/Human might move it)
        cueBall->x = HEADSTRING_X * 0.5f;
        cueBall->y = TABLE_TOP + TABLE_HEIGHT / 2.0f;
        cueBall->vx = 0;
        cueBall->vy = 0;
        cueBall->isPocketed = false;

        // Set state based on who gets ball-in-hand
        if (currentPlayer == 1) { // Player 1 caused foul, Player 2 gets ball-in-hand
            if (isPlayer2AI) {
                // AI gets ball-in-hand. Set state and trigger AI.
                currentGameState = PLAYER2_TURN; // State remains P2 Turn
                aiTurnPending = true; // AI will handle placement in its logic
            }
            else {
                // Human Player 2 gets ball-in-hand
                currentGameState = BALL_IN_HAND_P2;
            }
        }
        else { // Player 2 caused foul, Player 1 gets ball-in-hand
            currentGameState = BALL_IN_HAND_P1;
            aiTurnPending = false; // Ensure AI flag off if P1 gets ball-in-hand
        }
    }
}


// --- Game Logic ---

void ApplyShot(float power, float angle, float spinX, float spinY) {
    Ball* cueBall = GetCueBall();
    if (cueBall) {
        cueBall->vx = cosf(angle) * power;
        cueBall->vy = sinf(angle) * power;

        // Apply English (Spin) - Simplified effect
        // Top/Bottom spin affects initial roll slightly
        cueBall->vx += sinf(angle) * spinY * 0.5f; // Small effect perpendicular to shot dir
        cueBall->vy -= cosf(angle) * spinY * 0.5f;
        // Side spin affects initial direction slightly
        cueBall->vx -= cosf(angle) * spinX * 0.5f;
        cueBall->vy -= sinf(angle) * spinX * 0.5f;

        // Store spin for later use in collisions/cushions (could decay over time too)
        cueSpinX = spinX;
        cueSpinY = spinY;
    }
}


void ProcessShotResults() {
    bool cueBallPocketed = false;
    bool eightBallPocketed = false;
    bool legalBallPocketed = false; // Player's own ball type
    bool opponentBallPocketed = false; // Opponent's ball type
    bool anyNonCueBallPocketed = false;
    BallType firstPocketedType = BallType::NONE; // Type of the first object ball pocketed
    int firstPocketedId = -1; // ID of the first object ball pocketed

    PlayerInfo& currentPlayerInfo = (currentPlayer == 1) ? player1Info : player2Info;
    PlayerInfo& opponentPlayerInfo = (currentPlayer == 1) ? player2Info : player1Info;

    // Analyze pocketed balls from this shot sequence
    for (int pocketedId : pocketedThisTurn) {
        Ball* b = GetBallById(pocketedId);
        if (!b) continue; // Should not happen

        if (b->id == 0) {
            cueBallPocketed = true;
        }
        else if (b->id == 8) {
            eightBallPocketed = true;
        }
        else {
            anyNonCueBallPocketed = true;
            // Record the FIRST object ball pocketed in this turn
            if (firstPocketedId == -1) {
                firstPocketedId = b->id;
                firstPocketedType = b->type;
            }

            // Check if ball matches player's assigned type (if already assigned)
            if (currentPlayerInfo.assignedType != BallType::NONE) {
                if (b->type == currentPlayerInfo.assignedType) {
                    legalBallPocketed = true;
                }
                else if (b->type == opponentPlayerInfo.assignedType) {
                    opponentBallPocketed = true; // Pocketed opponent's ball
                }
            }
        }
    }

    // --- Game Over Checks --- (Unchanged)
    if (eightBallPocketed) {
        CheckGameOverConditions(eightBallPocketed, cueBallPocketed);
        if (currentGameState == GAME_OVER) return; // Stop processing if game ended
    }

    // --- Foul Checks --- (Unchanged)
    bool turnFoul = false;
    if (cueBallPocketed) {
        foulCommitted = true;
        turnFoul = true;
    }
    // (Other foul checks like wrong ball first, no rail after contact, etc. could be added here)


    // --- State Transitions ---

    // 1. Break Shot Results (Assigning Colors)
    //    Condition: Colors not assigned AND at least one object ball pocketed AND no scratch
    if (player1Info.assignedType == BallType::NONE && anyNonCueBallPocketed && !cueBallPocketed)
    {
        // --- Added Safeguard ---
        // Ensure the recorded 'firstPocketedType' corresponds to an actual pocketed ball ID this turn.
        bool firstTypeVerified = false;
        for (int id : pocketedThisTurn) {
            if (id == firstPocketedId) {
                firstTypeVerified = true;
                break;
            }
        }

        // Only assign types if the first recorded pocketed ball type is valid and verified
        if (firstTypeVerified && (firstPocketedType == BallType::SOLID || firstPocketedType == BallType::STRIPE))
        {
            AssignPlayerBallTypes(firstPocketedType);

            // Update ball counts based on ALL balls pocketed this turn after assignment
            player1Info.ballsPocketedCount = 0;
            player2Info.ballsPocketedCount = 0;
            for (int id : pocketedThisTurn) {
                Ball* b = GetBallById(id);
                if (b && b->id != 0 && b->id != 8) { // Ignore cue and 8-ball for counts
                    if (b->type == player1Info.assignedType) player1Info.ballsPocketedCount++;
                    else if (b->type == player2Info.assignedType) player2Info.ballsPocketedCount++;
                }
            }

            // Determine if player continues turn: Did they pocket their *newly assigned* type?
            bool pocketedOwnAssignedType = false;
            for (int id : pocketedThisTurn) {
                Ball* b = GetBallById(id);
                if (b && b->id != 0 && b->id != 8 && b->type == currentPlayerInfo.assignedType) {
                    pocketedOwnAssignedType = true;
                    break;
                }
            }

            if (pocketedOwnAssignedType) {
                // Continue turn
                currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                // If AI's turn, ensure flag is set to trigger next move
                if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
            }
            else {
                // Switch turns if they didn't pocket their assigned type on the assigning shot
                SwitchTurns();
            }
        }
        else {
            // If only 8-ball was pocketed on break (and no scratch), or something went wrong.
            // Re-spot 8-ball was handled in CheckGameOverConditions.
            // Treat as end of turn, switch players.
            SwitchTurns();
        }

    }
    // 2. Normal Play Results (Colors already assigned)
    else {
        // Update pocketed counts for assigned types
        // (Do this even if foul, as balls are off the table)
        int p1NewlyPocketed = 0;
        int p2NewlyPocketed = 0;
        for (int id : pocketedThisTurn) {
            Ball* b = GetBallById(id);
            if (!b || b->id == 0 || b->id == 8) continue;
            if (b->type == player1Info.assignedType) p1NewlyPocketed++;
            else if (b->type == player2Info.assignedType) p2NewlyPocketed++;
        }
        // Only update counts if not already game over state (prevents double counting on winning 8ball shot)
        if (currentGameState != GAME_OVER) {
            player1Info.ballsPocketedCount += p1NewlyPocketed;
            player2Info.ballsPocketedCount += p2NewlyPocketed;
        }


        // Decide next turn based on foul or legal pocket
        if (turnFoul) {
            // Pass turn, give opponent ball-in-hand
            SwitchTurns();
            RespawnCueBall(false); // Ball in hand for opponent
        }
        else if (legalBallPocketed) {
            // Player legally pocketed their own ball, continue turn
            currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
            // If AI's turn, make sure it knows to go again
            if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
        }
        else {
            // No legal ball pocketed or only opponent ball pocketed without foul.
            SwitchTurns();
        }
    }

    // --- Cleanup for next shot ---
    // Clear the list of balls pocketed *in this specific shot sequence*
    pocketedThisTurn.clear();
}

void AssignPlayerBallTypes(BallType firstPocketedType) {
    if (firstPocketedType == BallType::SOLID || firstPocketedType == BallType::STRIPE) {
        if (currentPlayer == 1) {
            player1Info.assignedType = firstPocketedType;
            player2Info.assignedType = (firstPocketedType == BallType::SOLID) ? BallType::STRIPE : BallType::SOLID;
        }
        else {
            player2Info.assignedType = firstPocketedType;
            player1Info.assignedType = (firstPocketedType == BallType::SOLID) ? BallType::STRIPE : BallType::SOLID;
        }
    }
    // If 8-ball was first (illegal on break generally), rules vary.
    // Here, we might ignore assignment until a solid/stripe is pocketed legally.
    // Or assign based on what *else* was pocketed, if anything.
    // Simplification: Assignment only happens on SOLID or STRIPE first pocket.
}

void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed) {
    if (!eightBallPocketed) return; // Only proceed if 8-ball was pocketed

    PlayerInfo& currentPlayerInfo = (currentPlayer == 1) ? player1Info : player2Info;
    bool playerClearedBalls = (currentPlayerInfo.assignedType != BallType::NONE && currentPlayerInfo.ballsPocketedCount >= 7);

    // Loss Conditions:
    // 1. Pocket 8-ball AND scratch (pocket cue ball)
    // 2. Pocket 8-ball before clearing own color group
    if (cueBallPocketed || (!playerClearedBalls && currentPlayerInfo.assignedType != BallType::NONE)) {
        gameOverMessage = (currentPlayer == 1) ? L"Player 2 Wins! (Player 1 fouled on 8-ball)" : L"Player 1 Wins! (Player 2 fouled on 8-ball)";
        currentGameState = GAME_OVER;
    }
    // Win Condition:
    // 1. Pocket 8-ball legally after clearing own color group
    else if (playerClearedBalls) {
        gameOverMessage = (currentPlayer == 1) ? L"Player 1 Wins!" : L"Player 2 Wins!";
        currentGameState = GAME_OVER;
    }
    // Special case: 8 ball pocketed on break. Usually re-spot or re-rack.
    // Simple: If it happens during assignment phase, treat as foul, respawn 8ball.
    else if (player1Info.assignedType == BallType::NONE) {
        Ball* eightBall = GetBallById(8);
        if (eightBall) {
            eightBall->isPocketed = false;
            // Place 8-ball on foot spot (approx RACK_POS_X) or center if occupied
            eightBall->x = RACK_POS_X;
            eightBall->y = RACK_POS_Y;
            eightBall->vx = eightBall->vy = 0;
            // Check overlap and nudge if necessary (simplified)
        }
        // Apply foul rules if cue ball was also pocketed
        if (cueBallPocketed) {
            foulCommitted = true;
            // Don't switch turns on break scratch + 8ball pocket? Rules vary.
            // Let's make it a foul, switch turns, ball in hand.
            SwitchTurns();
            RespawnCueBall(false); // Ball in hand for opponent
        }
        else {
            // Just respawned 8ball, continue turn or switch based on other balls pocketed.
            // Let ProcessShotResults handle turn logic based on other pocketed balls.
        }
        // Prevent immediate game over message by returning here
        return;
    }


}


void SwitchTurns() {
    currentPlayer = (currentPlayer == 1) ? 2 : 1;
    // Reset aiming state for the new player
    isAiming = false;
    shotPower = 0;
    // Reset foul flag before new turn *really* starts (AI might take over)
    // Foul flag is mainly for display, gets cleared before human/AI shot
    // foulCommitted = false; // Probably better to clear before ApplyShot

    // Set the correct state based on who's turn it is
    if (currentPlayer == 1) {
        currentGameState = PLAYER1_TURN;
        aiTurnPending = false; // Ensure AI flag is off for P1
    }
    else { // Player 2's turn
        if (isPlayer2AI) {
            currentGameState = PLAYER2_TURN; // State indicates it's P2's turn
            aiTurnPending = true;           // Set flag for GameUpdate to trigger AI
            // AI will handle Ball-in-Hand logic if necessary within its decision making
        }
        else {
            currentGameState = PLAYER2_TURN; // Human P2
            aiTurnPending = false;
        }
    }
}

// --- Helper Functions ---

Ball* GetBallById(int id) {
    for (size_t i = 0; i < balls.size(); ++i) {
        if (balls[i].id == id) {
            return &balls[i];
        }
    }
    return nullptr;
}

Ball* GetCueBall() {
    return GetBallById(0);
}

float GetDistance(float x1, float y1, float x2, float y2) {
    return sqrtf(GetDistanceSq(x1, y1, x2, y2));
}

float GetDistanceSq(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

bool IsValidCueBallPosition(float x, float y, bool checkHeadstring) {
    // Basic bounds check (inside cushions)
    float left = TABLE_LEFT + CUSHION_THICKNESS + BALL_RADIUS;
    float right = TABLE_RIGHT - CUSHION_THICKNESS - BALL_RADIUS;
    float top = TABLE_TOP + CUSHION_THICKNESS + BALL_RADIUS;
    float bottom = TABLE_BOTTOM - CUSHION_THICKNESS - BALL_RADIUS;

    if (x < left || x > right || y < top || y > bottom) {
        return false;
    }

    // Check headstring restriction if needed
    if (checkHeadstring && x >= HEADSTRING_X) {
        return false;
    }

    // Check overlap with other balls
    for (size_t i = 0; i < balls.size(); ++i) {
        if (balls[i].id != 0 && !balls[i].isPocketed) { // Don't check against itself or pocketed balls
            if (GetDistanceSq(x, y, balls[i].x, balls[i].y) < (BALL_RADIUS * 2.0f) * (BALL_RADIUS * 2.0f)) {
                return false; // Overlapping another ball
            }
        }
    }

    return true;
}


template <typename T>
void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

// --- Helper Function for Line Segment Intersection ---
// Finds intersection point of line segment P1->P2 and line segment P3->P4
// Returns true if they intersect, false otherwise. Stores intersection point in 'intersection'.
bool LineSegmentIntersection(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_POINT_2F p3, D2D1_POINT_2F p4, D2D1_POINT_2F& intersection)
{
    float denominator = (p4.y - p3.y) * (p2.x - p1.x) - (p4.x - p3.x) * (p2.y - p1.y);

    // Check if lines are parallel or collinear
    if (fabs(denominator) < 1e-6) {
        return false;
    }

    float ua = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denominator;
    float ub = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denominator;

    // Check if intersection point lies on both segments
    if (ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f) {
        intersection.x = p1.x + ua * (p2.x - p1.x);
        intersection.y = p1.y + ua * (p2.y - p1.y);
        return true;
    }

    return false;
}

// --- NEW AI Implementation Functions ---

// Main entry point for AI turn
void AIMakeDecision() {
    Ball* cueBall = GetCueBall();
    if (!cueBall || !isPlayer2AI || currentPlayer != 2) return; // Safety checks

    // Handle Ball-in-Hand placement first if necessary
    if (currentGameState == PRE_BREAK_PLACEMENT || currentGameState == BALL_IN_HAND_P2) {
        AIPlaceCueBall();
        // After placement, state should transition to PLAYER2_TURN or BREAKING
        currentGameState = (player1Info.assignedType == BallType::NONE) ? BREAKING : PLAYER2_TURN;
    }

    // Now find the best shot from the current position
    AIShotInfo bestShot = AIFindBestShot();

    if (bestShot.possible) {
        // Add slight delay maybe? For now, shoot immediately.
        // Apply calculated shot
        ApplyShot(bestShot.power, bestShot.angle, 0.0f, 0.0f); // AI doesn't use spin yet

        // Set state to shot in progress (ApplyShot might do this already)
        currentGameState = SHOT_IN_PROGRESS;
        foulCommitted = false; // Reset foul flag for AI shot
        pocketedThisTurn.clear(); // Clear previous pockets
    }
    else {
        // AI couldn't find any shot (highly unlikely with simple logic, but possible)
        // Safety shot? Push cue ball gently? Forfeit turn?
        // Simplest: Just tap the cue ball gently forward as a safety/pass.
        ApplyShot(MAX_SHOT_POWER * 0.1f, 0.0f, 0.0f, 0.0f); // Gentle tap forward
        currentGameState = SHOT_IN_PROGRESS;
        foulCommitted = false;
        pocketedThisTurn.clear();
        // NOTE: This might cause a foul if no ball is hit. Harder AI would handle this better.
    }
    aiTurnPending = false; // Ensure flag is off after decision
}

// AI logic for placing cue ball during ball-in-hand
void AIPlaceCueBall() {
    Ball* cueBall = GetCueBall();
    if (!cueBall) return;

    // Simple Strategy: Find the easiest possible shot for the AI's ball type
    // Place the cue ball directly behind that target ball, aiming straight at a pocket.
    // (More advanced: find spot offering multiple options or safety)

    AIShotInfo bestPlacementShot = { false };
    D2D1_POINT_2F bestPlacePos = D2D1::Point2F(HEADSTRING_X * 0.5f, RACK_POS_Y); // Default placement

    BallType targetType = player2Info.assignedType;
    bool canTargetAnyPlacement = false; // Local scope variable for placement logic
    if (targetType == BallType::NONE) {
        canTargetAnyPlacement = true;
    }
    bool target8Ball = (!canTargetAnyPlacement && targetType != BallType::NONE && player2Info.ballsPocketedCount >= 7);
    if (target8Ball) targetType = BallType::EIGHT_BALL;


    for (auto& targetBall : balls) {
        if (targetBall.isPocketed || targetBall.id == 0) continue;

        // Determine if current ball is a valid target for placement consideration
        bool currentBallIsValidTarget = false;
        if (target8Ball && targetBall.id == 8) currentBallIsValidTarget = true;
        else if (canTargetAnyPlacement && targetBall.id != 8) currentBallIsValidTarget = true;
        else if (!canTargetAnyPlacement && !target8Ball && targetBall.type == targetType) currentBallIsValidTarget = true;

        if (!currentBallIsValidTarget) continue; // Skip if not a valid target

        for (int p = 0; p < 6; ++p) {
            // Calculate ideal cue ball position: straight line behind target ball aiming at pocket p
            float targetToPocketX = pocketPositions[p].x - targetBall.x;
            float targetToPocketY = pocketPositions[p].y - targetBall.y;
            float dist = sqrtf(targetToPocketX * targetToPocketX + targetToPocketY * targetToPocketY);
            if (dist < 1.0f) continue; // Avoid division by zero

            float idealAngle = atan2f(targetToPocketY, targetToPocketX);
            // Place cue ball slightly behind target ball along this line
            float placeDist = BALL_RADIUS * 3.0f; // Place a bit behind
            D2D1_POINT_2F potentialPlacePos = D2D1::Point2F( // Use factory function
                targetBall.x - cosf(idealAngle) * placeDist,
                targetBall.y - sinf(idealAngle) * placeDist
            );

            // Check if this placement is valid (on table, behind headstring if break, not overlapping)
            bool behindHeadstringRule = (currentGameState == PRE_BREAK_PLACEMENT);
            if (IsValidCueBallPosition(potentialPlacePos.x, potentialPlacePos.y, behindHeadstringRule)) {
                // Is path from potentialPlacePos to targetBall clear?
                // Use D2D1::Point2F() factory function here
                if (IsPathClear(potentialPlacePos, D2D1::Point2F(targetBall.x, targetBall.y), 0, targetBall.id)) {
                    // Is path from targetBall to pocket clear?
                    // Use D2D1::Point2F() factory function here
                    if (IsPathClear(D2D1::Point2F(targetBall.x, targetBall.y), pocketPositions[p], targetBall.id, -1)) {
                        // This seems like a good potential placement. Score it?
                        // Easy AI: Just take the first valid one found.
                        bestPlacePos = potentialPlacePos;
                        goto placement_found; // Use goto for simplicity in non-OOP structure
                    }
                }
            }
        }
    }

placement_found:
    // Place the cue ball at the best found position (or default if none found)
    cueBall->x = bestPlacePos.x;
    cueBall->y = bestPlacePos.y;
    cueBall->vx = 0;
    cueBall->vy = 0;
}


// AI finds the best shot available on the table
AIShotInfo AIFindBestShot() {
    AIShotInfo bestShotOverall = { false };
    Ball* cueBall = GetCueBall();
    if (!cueBall) return bestShotOverall;

    // Determine target ball type for AI (Player 2)
    BallType targetType = player2Info.assignedType;
    bool canTargetAny = false; // Can AI hit any ball (e.g., after break, before assignment)?
    if (targetType == BallType::NONE) {
        // If colors not assigned, AI aims to pocket *something* (usually lowest numbered ball legally)
        // Or, more simply, treat any ball as a potential target to make *a* pocket
        canTargetAny = true; // Simplification: allow targeting any non-8 ball.
        // A better rule is hit lowest numbered ball first on break follow-up.
    }

    // Check if AI needs to shoot the 8-ball
    bool target8Ball = (!canTargetAny && targetType != BallType::NONE && player2Info.ballsPocketedCount >= 7);


    // Iterate through all potential target balls
    for (auto& potentialTarget : balls) {
        if (potentialTarget.isPocketed || potentialTarget.id == 0) continue; // Skip pocketed and cue ball

        // Check if this ball is a valid target
        bool isValidTarget = false;
        if (target8Ball) {
            isValidTarget = (potentialTarget.id == 8);
        }
        else if (canTargetAny) {
            isValidTarget = (potentialTarget.id != 8); // Can hit any non-8 ball
        }
        else { // Colors assigned, not yet shooting 8-ball
            isValidTarget = (potentialTarget.type == targetType);
        }

        if (!isValidTarget) continue; // Skip if not a valid target for this turn

        // Now, check all pockets for this target ball
        for (int p = 0; p < 6; ++p) {
            AIShotInfo currentShot = EvaluateShot(&potentialTarget, p);
            currentShot.involves8Ball = (potentialTarget.id == 8);

            if (currentShot.possible) {
                // Compare scores to find the best shot
                if (!bestShotOverall.possible || currentShot.score > bestShotOverall.score) {
                    bestShotOverall = currentShot;
                }
            }
        }
    } // End loop through potential target balls

    // If targeting 8-ball and no shot found, or targeting own balls and no shot found,
    // need a safety strategy. Current simple AI just takes best found or taps cue ball.

    return bestShotOverall;
}


// Evaluate a potential shot at a specific target ball towards a specific pocket
AIShotInfo EvaluateShot(Ball* targetBall, int pocketIndex) {
    AIShotInfo shotInfo;
    shotInfo.possible = false; // Assume not possible initially
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;

    Ball* cueBall = GetCueBall();
    if (!cueBall || !targetBall) return shotInfo;

    // --- Define local state variables needed for legality checks ---
    BallType aiAssignedType = player2Info.assignedType;
    bool canTargetAny = (aiAssignedType == BallType::NONE); // Can AI hit any ball?
    bool mustTarget8Ball = (!canTargetAny && aiAssignedType != BallType::NONE && player2Info.ballsPocketedCount >= 7);
    // ---

    // 1. Calculate Ghost Ball position
    shotInfo.ghostBallPos = CalculateGhostBallPos(targetBall, pocketIndex);

    // 2. Calculate Angle from Cue Ball to Ghost Ball
    float dx = shotInfo.ghostBallPos.x - cueBall->x;
    float dy = shotInfo.ghostBallPos.y - cueBall->y;
    if (fabs(dx) < 0.01f && fabs(dy) < 0.01f) return shotInfo; // Avoid aiming at same spot
    shotInfo.angle = atan2f(dy, dx);

    // Basic angle validity check (optional)
    if (!IsValidAIAimAngle(shotInfo.angle)) {
        // Maybe log this or handle edge cases
    }

    // 3. Check Path: Cue Ball -> Ghost Ball Position
    // Use D2D1::Point2F() factory function here
    if (!IsPathClear(D2D1::Point2F(cueBall->x, cueBall->y), shotInfo.ghostBallPos, cueBall->id, targetBall->id)) {
        return shotInfo; // Path blocked
    }

    // 4. Check Path: Target Ball -> Pocket
    // Use D2D1::Point2F() factory function here
    if (!IsPathClear(D2D1::Point2F(targetBall->x, targetBall->y), pocketPositions[pocketIndex], targetBall->id, -1)) {
        return shotInfo; // Path blocked
    }

    // 5. Check First Ball Hit Legality
    float firstHitDistSq = -1.0f;
    // Use D2D1::Point2F() factory function here
    Ball* firstHit = FindFirstHitBall(D2D1::Point2F(cueBall->x, cueBall->y), shotInfo.angle, firstHitDistSq);

    if (!firstHit) {
        return shotInfo; // AI aims but doesn't hit anything? Impossible shot.
    }

    // Check if the first ball hit is the intended target ball
    if (firstHit->id != targetBall->id) {
        // Allow hitting slightly off target if it's very close to ghost ball pos
        float ghostDistSq = GetDistanceSq(shotInfo.ghostBallPos.x, shotInfo.ghostBallPos.y, firstHit->x, firstHit->y);
        // Allow a tolerance roughly half the ball radius squared
        if (ghostDistSq > (BALL_RADIUS * 0.7f) * (BALL_RADIUS * 0.7f)) {
            // First hit is significantly different from the target point.
            // This shot path leads to hitting the wrong ball first.
            return shotInfo; // Foul or unintended shot
        }
        // If first hit is not target, but very close, allow it for now (might still be foul based on type).
    }

    // Check legality of the *first ball actually hit* based on game rules
    if (!canTargetAny) { // Colors are assigned (or should be)
        if (mustTarget8Ball) { // Must hit 8-ball first
            if (firstHit->id != 8) {
                // return shotInfo; // FOUL - Hitting wrong ball when aiming for 8-ball
                // Keep shot possible for now, rely on AIFindBestShot to prioritize legal ones
            }
        }
        else { // Must hit own ball type first
            if (firstHit->type != aiAssignedType && firstHit->id != 8) { // Allow hitting 8-ball if own type blocked? No, standard rules usually require hitting own first.
                // return shotInfo; // FOUL - Hitting opponent ball or 8-ball when shouldn't
                // Keep shot possible for now, rely on AIFindBestShot to prioritize legal ones
            }
            else if (firstHit->id == 8) {
                // return shotInfo; // FOUL - Hitting 8-ball when shouldn't
                // Keep shot possible for now
            }
        }
    }
    // (If canTargetAny is true, hitting any ball except 8 first is legal - assuming not scratching)


    // 6. Calculate Score & Power (Difficulty affects this)
    shotInfo.possible = true; // If we got here, the shot is geometrically possible and likely legal enough for AI to consider

    float cueToGhostDist = GetDistance(cueBall->x, cueBall->y, shotInfo.ghostBallPos.x, shotInfo.ghostBallPos.y);
    float targetToPocketDist = GetDistance(targetBall->x, targetBall->y, pocketPositions[pocketIndex].x, pocketPositions[pocketIndex].y);

    // Simple Score: Shorter shots are better, straighter shots are slightly better.
    float distanceScore = 1000.0f / (1.0f + cueToGhostDist + targetToPocketDist);

    // Angle Score: Calculate cut angle
    // Vector Cue -> Ghost
    float v1x = shotInfo.ghostBallPos.x - cueBall->x;
    float v1y = shotInfo.ghostBallPos.y - cueBall->y;
    // Vector Target -> Pocket
    float v2x = pocketPositions[pocketIndex].x - targetBall->x;
    float v2y = pocketPositions[pocketIndex].y - targetBall->y;
    // Normalize vectors
    float mag1 = sqrtf(v1x * v1x + v1y * v1y);
    float mag2 = sqrtf(v2x * v2x + v2y * v2y);
    float angleScoreFactor = 0.5f; // Default if vectors are zero len
    if (mag1 > 0.1f && mag2 > 0.1f) {
        v1x /= mag1; v1y /= mag1;
        v2x /= mag2; v2y /= mag2;
        // Dot product gives cosine of angle between cue ball path and target ball path
        float dotProduct = v1x * v2x + v1y * v2y;
        // Straighter shot (dot product closer to 1) gets higher score
        angleScoreFactor = (1.0f + dotProduct) / 2.0f; // Map [-1, 1] to [0, 1]
    }
    angleScoreFactor = std::max(0.1f, angleScoreFactor); // Ensure some minimum score factor

    shotInfo.score = distanceScore * angleScoreFactor;

    // Bonus for pocketing 8-ball legally
    if (mustTarget8Ball && targetBall->id == 8) {
        shotInfo.score *= 10.0; // Strongly prefer the winning shot
    }

    // Penalty for difficult cuts? Already partially handled by angleScoreFactor.

    // 7. Calculate Power
    shotInfo.power = CalculateShotPower(cueToGhostDist, targetToPocketDist);

    // 8. Add Inaccuracy based on Difficulty (same as before)
    float angleError = 0.0f;
    float powerErrorFactor = 1.0f;

    switch (aiDifficulty) {
    case EASY:
        angleError = (float)(rand() % 100 - 50) / 1000.0f; // +/- ~3 deg
        powerErrorFactor = 0.8f + (float)(rand() % 40) / 100.0f; // 80-120%
        shotInfo.power *= 0.8f;
        break;
    case MEDIUM:
        angleError = (float)(rand() % 60 - 30) / 1000.0f; // +/- ~1.7 deg
        powerErrorFactor = 0.9f + (float)(rand() % 20) / 100.0f; // 90-110%
        break;
    case HARD:
        angleError = (float)(rand() % 10 - 5) / 1000.0f; // +/- ~0.3 deg
        powerErrorFactor = 0.98f + (float)(rand() % 4) / 100.0f; // 98-102%
        break;
    }
    shotInfo.angle += angleError;
    shotInfo.power *= powerErrorFactor;
    shotInfo.power = std::max(1.0f, std::min(shotInfo.power, MAX_SHOT_POWER)); // Clamp power

    return shotInfo;
}


// Calculates required power (simplified)
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist) {
    // Basic model: Power needed increases with total distance the balls need to travel.
    // Need enough power for cue ball to reach target AND target to reach pocket.
    float totalDist = cueToGhostDist + targetToPocketDist;

    // Map distance to power (needs tuning)
    // Let's say max power is needed for longest possible shot (e.g., corner to corner ~ 1000 units)
    float powerRatio = std::min(1.0f, totalDist / 800.0f); // Normalize based on estimated max distance

    float basePower = MAX_SHOT_POWER * 0.2f; // Minimum power to move balls reliably
    float variablePower = (MAX_SHOT_POWER * 0.8f) * powerRatio; // Scale remaining power range

    // Harder AI could adjust based on desired cue ball travel (more power for draw/follow)
    return std::min(MAX_SHOT_POWER, basePower + variablePower);
}

// Calculate the position the cue ball needs to hit for the target ball to go towards the pocket
D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, int pocketIndex) {
    float targetToPocketX = pocketPositions[pocketIndex].x - targetBall->x;
    float targetToPocketY = pocketPositions[pocketIndex].y - targetBall->y;
    float dist = sqrtf(targetToPocketX * targetToPocketX + targetToPocketY * targetToPocketY);

    if (dist < 1.0f) { // Target is basically in the pocket
        // Aim slightly off-center to avoid weird physics? Or directly at center?
        // For simplicity, return a point slightly behind center along the reverse line.
        return D2D1::Point2F(targetBall->x - targetToPocketX * 0.1f, targetBall->y - targetToPocketY * 0.1f);
    }

    // Normalize direction vector from target to pocket
    float nx = targetToPocketX / dist;
    float ny = targetToPocketY / dist;

    // Ghost ball position is diameter distance *behind* the target ball along this line
    float ghostX = targetBall->x - nx * (BALL_RADIUS * 2.0f);
    float ghostY = targetBall->y - ny * (BALL_RADIUS * 2.0f);

    return D2D1::Point2F(ghostX, ghostY);
}

// Checks if line segment is clear of obstructing balls
bool IsPathClear(D2D1_POINT_2F start, D2D1_POINT_2F end, int ignoredBallId1, int ignoredBallId2) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float segmentLenSq = dx * dx + dy * dy;

    if (segmentLenSq < 0.01f) return true; // Start and end are same point

    for (const auto& ball : balls) {
        if (ball.isPocketed) continue;
        if (ball.id == ignoredBallId1) continue;
        if (ball.id == ignoredBallId2) continue;

        // Check distance from ball center to the line segment
        float ballToStartX = ball.x - start.x;
        float ballToStartY = ball.y - start.y;

        // Project ball center onto the line defined by the segment
        float dot = (ballToStartX * dx + ballToStartY * dy) / segmentLenSq;

        D2D1_POINT_2F closestPointOnLine;
        if (dot < 0) { // Closest point is start point
            closestPointOnLine = start;
        }
        else if (dot > 1) { // Closest point is end point
            closestPointOnLine = end;
        }
        else { // Closest point is along the segment
            closestPointOnLine = D2D1::Point2F(start.x + dot * dx, start.y + dot * dy);
        }

        // Check if the closest point is within collision distance (ball radius + path radius)
        if (GetDistanceSq(ball.x, ball.y, closestPointOnLine.x, closestPointOnLine.y) < (BALL_RADIUS * BALL_RADIUS)) {
            // Consider slightly wider path check? Maybe BALL_RADIUS * 1.1f?
            // if (GetDistanceSq(ball.x, ball.y, closestPointOnLine.x, closestPointOnLine.y) < (BALL_RADIUS * 1.1f)*(BALL_RADIUS*1.1f)) {
            return false; // Path is blocked
        }
    }
    return true; // No obstructions found
}

// Finds the first ball hit along a path (simplified)
Ball* FindFirstHitBall(D2D1_POINT_2F start, float angle, float& hitDistSq) {
    Ball* hitBall = nullptr;
    hitDistSq = -1.0f; // Initialize hit distance squared
    float minCollisionDistSq = -1.0f;

    float cosA = cosf(angle);
    float sinA = sinf(angle);

    for (auto& ball : balls) {
        if (ball.isPocketed || ball.id == 0) continue; // Skip cue ball and pocketed

        float dx = ball.x - start.x;
        float dy = ball.y - start.y;

        // Project vector from start->ball onto the aim direction vector
        float dot = dx * cosA + dy * sinA;

        if (dot > 0) { // Ball is generally in front
            // Find closest point on aim line to the ball's center
            float closestPointX = start.x + dot * cosA;
            float closestPointY = start.y + dot * sinA;
            float distSq = GetDistanceSq(ball.x, ball.y, closestPointX, closestPointY);

            // Check if the aim line passes within the ball's radius
            if (distSq < (BALL_RADIUS * BALL_RADIUS)) {
                // Calculate distance from start to the collision point on the ball's circumference
                float backDist = sqrtf(std::max(0.f, BALL_RADIUS * BALL_RADIUS - distSq));
                float collisionDist = dot - backDist; // Distance along aim line to collision

                if (collisionDist > 0) { // Ensure collision is in front
                    float collisionDistSq = collisionDist * collisionDist;
                    if (hitBall == nullptr || collisionDistSq < minCollisionDistSq) {
                        minCollisionDistSq = collisionDistSq;
                        hitBall = &ball; // Found a closer hit ball
                    }
                }
            }
        }
    }
    hitDistSq = minCollisionDistSq; // Return distance squared to the first hit
    return hitBall;
}

// Basic check for reasonable AI aim angles (optional)
bool IsValidAIAimAngle(float angle) {
    // Placeholder - could check for NaN or infinity if calculations go wrong
    return isfinite(angle);
}

// --- Drawing Functions ---

void OnPaint() {
    HRESULT hr = CreateDeviceResources(); // Ensure resources are valid

    if (SUCCEEDED(hr)) {
        pRenderTarget->BeginDraw();
        DrawScene(pRenderTarget); // Pass render target
        hr = pRenderTarget->EndDraw();

        if (hr == D2DERR_RECREATE_TARGET) {
            DiscardDeviceResources();
            // Optionally request another paint message: InvalidateRect(hwndMain, NULL, FALSE);
            // But the timer loop will trigger redraw anyway.
        }
    }
    // If CreateDeviceResources failed, EndDraw might not be called.
    // Consider handling this more robustly if needed.
}

void DrawScene(ID2D1RenderTarget* pRT) {
    if (!pRT) return;

    pRT->Clear(D2D1::ColorF(D2D1::ColorF::LightGray)); // Background color

    DrawTable(pRT);
    DrawBalls(pRT);
    DrawAimingAids(pRT); // Includes cue stick if aiming
    DrawUI(pRT);
    DrawPowerMeter(pRT);
    DrawSpinIndicator(pRT);
    DrawPocketedBallsIndicator(pRT);
    DrawBallInHandIndicator(pRT); // Draw cue ball ghost if placing

     // Draw Game Over Message
    if (currentGameState == GAME_OVER && pTextFormat) {
        ID2D1SolidColorBrush* pBrush = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pBrush);
        if (pBrush) {
            D2D1_RECT_F layoutRect = D2D1::RectF(TABLE_LEFT, TABLE_TOP + TABLE_HEIGHT / 2 - 30, TABLE_RIGHT, TABLE_TOP + TABLE_HEIGHT / 2 + 30);
            pRT->DrawText(
                gameOverMessage.c_str(),
                (UINT32)gameOverMessage.length(),
                pTextFormat, // Use large format maybe?
                &layoutRect,
                pBrush
            );
            SafeRelease(&pBrush);
        }
    }

}

void DrawTable(ID2D1RenderTarget* pRT) {
    ID2D1SolidColorBrush* pBrush = nullptr;

    // Draw Table Bed (Green Felt)
    pRT->CreateSolidColorBrush(TABLE_COLOR, &pBrush);
    if (!pBrush) return;
    D2D1_RECT_F tableRect = D2D1::RectF(TABLE_LEFT, TABLE_TOP, TABLE_RIGHT, TABLE_BOTTOM);
    pRT->FillRectangle(&tableRect, pBrush);
    SafeRelease(&pBrush);

    // Draw Cushions (Red Border)
    pRT->CreateSolidColorBrush(CUSHION_COLOR, &pBrush);
    if (!pBrush) return;
    // Top Cushion (split by middle pocket)
    pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + HOLE_VISUAL_RADIUS, TABLE_TOP - CUSHION_THICKNESS, TABLE_LEFT + TABLE_WIDTH / 2.f - HOLE_VISUAL_RADIUS, TABLE_TOP), pBrush);
    pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + TABLE_WIDTH / 2.f + HOLE_VISUAL_RADIUS, TABLE_TOP - CUSHION_THICKNESS, TABLE_RIGHT - HOLE_VISUAL_RADIUS, TABLE_TOP), pBrush);
    // Bottom Cushion (split by middle pocket)
    pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + HOLE_VISUAL_RADIUS, TABLE_BOTTOM, TABLE_LEFT + TABLE_WIDTH / 2.f - HOLE_VISUAL_RADIUS, TABLE_BOTTOM + CUSHION_THICKNESS), pBrush);
    pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + TABLE_WIDTH / 2.f + HOLE_VISUAL_RADIUS, TABLE_BOTTOM, TABLE_RIGHT - HOLE_VISUAL_RADIUS, TABLE_BOTTOM + CUSHION_THICKNESS), pBrush);
    // Left Cushion
    pRT->FillRectangle(D2D1::RectF(TABLE_LEFT - CUSHION_THICKNESS, TABLE_TOP + HOLE_VISUAL_RADIUS, TABLE_LEFT, TABLE_BOTTOM - HOLE_VISUAL_RADIUS), pBrush);
    // Right Cushion
    pRT->FillRectangle(D2D1::RectF(TABLE_RIGHT, TABLE_TOP + HOLE_VISUAL_RADIUS, TABLE_RIGHT + CUSHION_THICKNESS, TABLE_BOTTOM - HOLE_VISUAL_RADIUS), pBrush);
    SafeRelease(&pBrush);


    // Draw Pockets (Black Circles)
    pRT->CreateSolidColorBrush(POCKET_COLOR, &pBrush);
    if (!pBrush) return;
    for (int i = 0; i < 6; ++i) {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(pocketPositions[i], HOLE_VISUAL_RADIUS, HOLE_VISUAL_RADIUS);
        pRT->FillEllipse(&ellipse, pBrush);
    }
    SafeRelease(&pBrush);

    // Draw Headstring Line (White)
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.5f), &pBrush);
    if (!pBrush) return;
    pRT->DrawLine(
        D2D1::Point2F(HEADSTRING_X, TABLE_TOP),
        D2D1::Point2F(HEADSTRING_X, TABLE_BOTTOM),
        pBrush,
        1.0f // Line thickness
    );
    SafeRelease(&pBrush);
}


void DrawBalls(ID2D1RenderTarget* pRT) {
    ID2D1SolidColorBrush* pBrush = nullptr;
    ID2D1SolidColorBrush* pStripeBrush = nullptr; // For stripe pattern

    pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &pBrush); // Placeholder
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pStripeBrush);

    if (!pBrush || !pStripeBrush) {
        SafeRelease(&pBrush);
        SafeRelease(&pStripeBrush);
        return;
    }


    for (size_t i = 0; i < balls.size(); ++i) {
        const Ball& b = balls[i];
        if (!b.isPocketed) {
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(b.x, b.y), BALL_RADIUS, BALL_RADIUS);

            // Set main ball color
            pBrush->SetColor(b.color);
            pRT->FillEllipse(&ellipse, pBrush);

            // Draw Stripe if applicable
            if (b.type == BallType::STRIPE) {
                // Draw a white band across the middle (simplified stripe)
                D2D1_RECT_F stripeRect = D2D1::RectF(b.x - BALL_RADIUS, b.y - BALL_RADIUS * 0.4f, b.x + BALL_RADIUS, b.y + BALL_RADIUS * 0.4f);
                // Need to clip this rectangle to the ellipse bounds - complex!
                // Alternative: Draw two colored arcs leaving a white band.
                // Simplest: Draw a white circle inside, slightly smaller.
                D2D1_ELLIPSE innerEllipse = D2D1::Ellipse(D2D1::Point2F(b.x, b.y), BALL_RADIUS * 0.6f, BALL_RADIUS * 0.6f);
                pRT->FillEllipse(innerEllipse, pStripeBrush); // White center part
                pBrush->SetColor(b.color); // Set back to stripe color
                pRT->FillEllipse(innerEllipse, pBrush); // Fill again, leaving a ring - No, this isn't right.

                // Let's try drawing a thick white line across
                // This doesn't look great. Just drawing solid red for stripes for now.
            }

            // Draw Number (Optional - requires more complex text layout or pre-rendered textures)
            // if (b.id != 0 && pTextFormat) {
            //     std::wstring numStr = std::to_wstring(b.id);
            //     D2D1_RECT_F textRect = D2D1::RectF(b.x - BALL_RADIUS, b.y - BALL_RADIUS, b.x + BALL_RADIUS, b.y + BALL_RADIUS);
            //     ID2D1SolidColorBrush* pNumBrush = nullptr;
            //     D2D1_COLOR_F numCol = (b.type == BallType::SOLID || b.id == 8) ? D2D1::ColorF(D2D1::ColorF::Black) : D2D1::ColorF(D2D1::ColorF::White);
            //     pRT->CreateSolidColorBrush(numCol, &pNumBrush);
            //     // Create a smaller text format...
            //     // pRT->DrawText(numStr.c_str(), numStr.length(), pSmallTextFormat, &textRect, pNumBrush);
            //     SafeRelease(&pNumBrush);
            // }
        }
    }

    SafeRelease(&pBrush);
    SafeRelease(&pStripeBrush);
}


void DrawAimingAids(ID2D1RenderTarget* pRT) {
    // Condition check at start (Unchanged)
    if (currentGameState != PLAYER1_TURN && currentGameState != PLAYER2_TURN &&
        currentGameState != BREAKING && currentGameState != AIMING)
    {
        return;
    }

    Ball* cueBall = GetCueBall();
    if (!cueBall || cueBall->isPocketed) return; // Don't draw if cue ball is gone

    ID2D1SolidColorBrush* pBrush = nullptr;
    ID2D1SolidColorBrush* pGhostBrush = nullptr;
    ID2D1StrokeStyle* pDashedStyle = nullptr;
    ID2D1SolidColorBrush* pCueBrush = nullptr;
    ID2D1SolidColorBrush* pReflectBrush = nullptr; // Brush for reflection line

    // Ensure render target is valid
    if (!pRT) return;

    // Create Brushes and Styles (check for failures)
    HRESULT hr;
    hr = pRT->CreateSolidColorBrush(AIM_LINE_COLOR, &pBrush);
    if FAILED(hr) { SafeRelease(&pBrush); return; }
    hr = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.5f), &pGhostBrush);
    if FAILED(hr) { SafeRelease(&pBrush); SafeRelease(&pGhostBrush); return; }
    hr = pRT->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.4f, 0.2f), &pCueBrush);
    if FAILED(hr) { SafeRelease(&pBrush); SafeRelease(&pGhostBrush); SafeRelease(&pCueBrush); return; }
    // Create reflection brush (e.g., lighter shade or different color)
    hr = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightCyan, 0.6f), &pReflectBrush);
    if FAILED(hr) { SafeRelease(&pBrush); SafeRelease(&pGhostBrush); SafeRelease(&pCueBrush); SafeRelease(&pReflectBrush); return; }

    if (pFactory) {
        D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties();
        strokeProps.dashStyle = D2D1_DASH_STYLE_DASH;
        hr = pFactory->CreateStrokeStyle(&strokeProps, nullptr, 0, &pDashedStyle);
        if FAILED(hr) { pDashedStyle = nullptr; }
    }


    // --- Cue Stick Drawing (Unchanged from previous fix) ---
    const float baseStickLength = 150.0f;
    const float baseStickThickness = 4.0f;
    float stickLength = baseStickLength * 1.4f;
    float stickThickness = baseStickThickness * 1.5f;
    float stickAngle = cueAngle + PI;
    float powerOffset = 0.0f;
    if (isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) {
        powerOffset = shotPower * 5.0f;
    }
    D2D1_POINT_2F cueStickEnd = D2D1::Point2F(cueBall->x + cosf(stickAngle) * (stickLength + powerOffset), cueBall->y + sinf(stickAngle) * (stickLength + powerOffset));
    D2D1_POINT_2F cueStickTip = D2D1::Point2F(cueBall->x + cosf(stickAngle) * (powerOffset + 5.0f), cueBall->y + sinf(stickAngle) * (powerOffset + 5.0f));
    pRT->DrawLine(cueStickTip, cueStickEnd, pCueBrush, stickThickness);


    // --- Projection Line Calculation ---
    float cosA = cosf(cueAngle);
    float sinA = sinf(cueAngle);
    float rayLength = TABLE_WIDTH + TABLE_HEIGHT; // Ensure ray is long enough
    D2D1_POINT_2F rayStart = D2D1::Point2F(cueBall->x, cueBall->y);
    D2D1_POINT_2F rayEnd = D2D1::Point2F(rayStart.x + cosA * rayLength, rayStart.y + sinA * rayLength);

    // Find the first ball hit by the aiming ray
    Ball* hitBall = nullptr;
    float firstHitDistSq = -1.0f;
    D2D1_POINT_2F ballCollisionPoint = { 0, 0 }; // Point on target ball circumference
    D2D1_POINT_2F ghostBallPosForHit = { 0, 0 }; // Ghost ball pos for the hit ball

    hitBall = FindFirstHitBall(rayStart, cueAngle, firstHitDistSq);
    if (hitBall) {
        // Calculate the point on the target ball's circumference
        float collisionDist = sqrtf(firstHitDistSq);
        ballCollisionPoint = D2D1::Point2F(rayStart.x + cosA * collisionDist, rayStart.y + sinA * collisionDist);
        // Calculate ghost ball position for this specific hit (used for projection consistency)
        ghostBallPosForHit = D2D1::Point2F(hitBall->x - cosA * BALL_RADIUS, hitBall->y - sinA * BALL_RADIUS); // Approx.
    }

    // Find the first rail hit by the aiming ray
    D2D1_POINT_2F railHitPoint = rayEnd; // Default to far end if no rail hit
    float minRailDistSq = rayLength * rayLength;
    int hitRailIndex = -1; // 0:Left, 1:Right, 2:Top, 3:Bottom

    // Define table edge segments for intersection checks
    D2D1_POINT_2F topLeft = D2D1::Point2F(TABLE_LEFT, TABLE_TOP);
    D2D1_POINT_2F topRight = D2D1::Point2F(TABLE_RIGHT, TABLE_TOP);
    D2D1_POINT_2F bottomLeft = D2D1::Point2F(TABLE_LEFT, TABLE_BOTTOM);
    D2D1_POINT_2F bottomRight = D2D1::Point2F(TABLE_RIGHT, TABLE_BOTTOM);

    D2D1_POINT_2F currentIntersection;

    // Check Left Rail
    if (LineSegmentIntersection(rayStart, rayEnd, topLeft, bottomLeft, currentIntersection)) {
        float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
        if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 0; }
    }
    // Check Right Rail
    if (LineSegmentIntersection(rayStart, rayEnd, topRight, bottomRight, currentIntersection)) {
        float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
        if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 1; }
    }
    // Check Top Rail
    if (LineSegmentIntersection(rayStart, rayEnd, topLeft, topRight, currentIntersection)) {
        float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
        if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 2; }
    }
    // Check Bottom Rail
    if (LineSegmentIntersection(rayStart, rayEnd, bottomLeft, bottomRight, currentIntersection)) {
        float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
        if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 3; }
    }


    // --- Determine final aim line end point ---
    D2D1_POINT_2F finalLineEnd = railHitPoint; // Assume rail hit first
    bool aimingAtRail = true;

    if (hitBall && firstHitDistSq < minRailDistSq) {
        // Ball collision is closer than rail collision
        finalLineEnd = ballCollisionPoint; // End line at the point of contact on the ball
        aimingAtRail = false;
    }

    // --- Draw Primary Aiming Line ---
    pRT->DrawLine(rayStart, finalLineEnd, pBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

    // --- Draw Target Circle/Indicator ---
    D2D1_ELLIPSE targetCircle = D2D1::Ellipse(finalLineEnd, BALL_RADIUS / 2.0f, BALL_RADIUS / 2.0f);
    pRT->DrawEllipse(&targetCircle, pBrush, 1.0f);

    // --- Draw Projection/Reflection Lines ---
    if (!aimingAtRail && hitBall) {
        // Aiming at a ball: Draw Ghost Cue Ball and Target Ball Projection
        D2D1_ELLIPSE ghostCue = D2D1::Ellipse(ballCollisionPoint, BALL_RADIUS, BALL_RADIUS); // Ghost ball at contact point
        pRT->DrawEllipse(ghostCue, pGhostBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

        // Calculate target ball projection based on impact line (cue collision point -> target center)
        float targetProjectionAngle = atan2f(hitBall->y - ballCollisionPoint.y, hitBall->x - ballCollisionPoint.x);
        // Clamp angle calculation if distance is tiny
        if (GetDistanceSq(hitBall->x, hitBall->y, ballCollisionPoint.x, ballCollisionPoint.y) < 1.0f) {
            targetProjectionAngle = cueAngle; // Fallback if overlapping
        }

        D2D1_POINT_2F targetStartPoint = D2D1::Point2F(hitBall->x, hitBall->y);
        D2D1_POINT_2F targetProjectionEnd = D2D1::Point2F(
            hitBall->x + cosf(targetProjectionAngle) * 50.0f, // Projection length 50 units
            hitBall->y + sinf(targetProjectionAngle) * 50.0f
        );
        // Draw solid line for target projection
        pRT->DrawLine(targetStartPoint, targetProjectionEnd, pBrush, 1.0f);

        // -- Cue Ball Path after collision (Optional, requires physics) --
        // Very simplified: Assume cue deflects, angle depends on cut angle.
        // float cutAngle = acosf(cosf(cueAngle - targetProjectionAngle)); // Angle between paths
        // float cueDeflectionAngle = ? // Depends on cutAngle, spin, etc. Hard to predict accurately.
        // D2D1_POINT_2F cueProjectionEnd = ...
        // pRT->DrawLine(ballCollisionPoint, cueProjectionEnd, pGhostBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

        // --- Accuracy Comment ---
        // Note: The visual accuracy of this projection, especially for cut shots (hitting the ball off-center)
        // or shots with spin, is limited by the simplified physics model. Real pool physics involves
        // collision-induced throw, spin transfer, and cue ball deflection not fully simulated here.
        // The ghost ball method shows the *ideal* line for a center-cue hit without spin.

    }
    else if (aimingAtRail && hitRailIndex != -1) {
        // Aiming at a rail: Draw reflection line
        float reflectAngle = cueAngle;
        // Reflect angle based on which rail was hit
        if (hitRailIndex == 0 || hitRailIndex == 1) { // Left or Right rail
            reflectAngle = PI - cueAngle; // Reflect horizontal component
        }
        else { // Top or Bottom rail
            reflectAngle = -cueAngle; // Reflect vertical component
        }
        // Normalize angle if needed (atan2 usually handles this)
        while (reflectAngle > PI) reflectAngle -= 2 * PI;
        while (reflectAngle <= -PI) reflectAngle += 2 * PI;


        float reflectionLength = 60.0f; // Length of the reflection line
        D2D1_POINT_2F reflectionEnd = D2D1::Point2F(
            finalLineEnd.x + cosf(reflectAngle) * reflectionLength,
            finalLineEnd.y + sinf(reflectAngle) * reflectionLength
        );

        // Draw the reflection line (e.g., using a different color/style)
        pRT->DrawLine(finalLineEnd, reflectionEnd, pReflectBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);
    }

    // Release resources
    SafeRelease(&pBrush);
    SafeRelease(&pGhostBrush);
    SafeRelease(&pCueBrush);
    SafeRelease(&pReflectBrush); // Release new brush
    SafeRelease(&pDashedStyle);
}

void DrawUI(ID2D1RenderTarget* pRT) {
    if (!pTextFormat || !pLargeTextFormat) return;

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(UI_TEXT_COLOR, &pBrush);
    if (!pBrush) return;

    // --- Player Info Area (Top Left/Right) --- (Unchanged)
    float uiTop = TABLE_TOP - 80;
    float uiHeight = 60;
    float p1Left = TABLE_LEFT;
    float p1Width = 150;
    float p2Left = TABLE_RIGHT - p1Width;
    D2D1_RECT_F p1Rect = D2D1::RectF(p1Left, uiTop, p1Left + p1Width, uiTop + uiHeight);
    D2D1_RECT_F p2Rect = D2D1::RectF(p2Left, uiTop, p2Left + p1Width, uiTop + uiHeight);

    // Player 1 Info Text (Unchanged)
    std::wostringstream oss1;
    oss1 << player1Info.name.c_str() << L"\n";
    if (player1Info.assignedType != BallType::NONE) {
        oss1 << ((player1Info.assignedType == BallType::SOLID) ? L"Solids (Yellow)" : L"Stripes (Red)");
        oss1 << L" [" << player1Info.ballsPocketedCount << L"/7]";
    }
    else {
        oss1 << L"(Undecided)";
    }
    pRT->DrawText(oss1.str().c_str(), (UINT32)oss1.str().length(), pTextFormat, &p1Rect, pBrush);

    // Player 2 Info Text (Unchanged)
    std::wostringstream oss2;
    oss2 << player2Info.name.c_str() << L"\n";
    if (player2Info.assignedType != BallType::NONE) {
        oss2 << ((player2Info.assignedType == BallType::SOLID) ? L"Solids (Yellow)" : L"Stripes (Red)");
        oss2 << L" [" << player2Info.ballsPocketedCount << L"/7]";
    }
    else {
        oss2 << L"(Undecided)";
    }
    pRT->DrawText(oss2.str().c_str(), (UINT32)oss2.str().length(), pTextFormat, &p2Rect, pBrush);

    // --- MODIFIED: Current Turn Arrow (Blue, Bigger, Beside Name) ---
    ID2D1SolidColorBrush* pArrowBrush = nullptr;
    pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrowBrush);
    if (pArrowBrush && currentGameState != GAME_OVER && currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
        float arrowSizeBase = 32.0f; // Base size for width/height offsets (4x original ~8)
        float arrowCenterY = p1Rect.top + uiHeight / 2.0f; // Center vertically with text box
        float arrowTipX, arrowBackX;

        if (currentPlayer == 1) {
            // Player 1: Arrow left of P1 box, pointing right
            arrowBackX = p1Rect.left - 15.0f; // Position left of the box
            arrowTipX = arrowBackX + arrowSizeBase * 0.75f; // Pointy end extends right
            // Define points for right-pointing arrow
            D2D1_POINT_2F pt1 = D2D1::Point2F(arrowTipX, arrowCenterY); // Tip
            D2D1_POINT_2F pt2 = D2D1::Point2F(arrowBackX, arrowCenterY - arrowSizeBase / 2.0f); // Top-Back
            D2D1_POINT_2F pt3 = D2D1::Point2F(arrowBackX, arrowCenterY + arrowSizeBase / 2.0f); // Bottom-Back

            ID2D1PathGeometry* pPath = nullptr;
            if (SUCCEEDED(pFactory->CreatePathGeometry(&pPath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pPath->Open(&pSink))) {
                    pSink->BeginFigure(pt1, D2D1_FIGURE_BEGIN_FILLED);
                    pSink->AddLine(pt2);
                    pSink->AddLine(pt3);
                    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    pSink->Close();
                    SafeRelease(&pSink);
                    pRT->FillGeometry(pPath, pArrowBrush);
                }
                SafeRelease(&pPath);
            }
        }
        else { // Player 2
         // Player 2: Arrow left of P2 box, pointing right (or right of P2 box pointing left?)
         // Let's keep it consistent: Arrow left of the active player's box, pointing right.
            arrowBackX = p2Rect.left - 15.0f; // Position left of the box
            arrowTipX = arrowBackX + arrowSizeBase * 0.75f; // Pointy end extends right
            // Define points for right-pointing arrow
            D2D1_POINT_2F pt1 = D2D1::Point2F(arrowTipX, arrowCenterY); // Tip
            D2D1_POINT_2F pt2 = D2D1::Point2F(arrowBackX, arrowCenterY - arrowSizeBase / 2.0f); // Top-Back
            D2D1_POINT_2F pt3 = D2D1::Point2F(arrowBackX, arrowCenterY + arrowSizeBase / 2.0f); // Bottom-Back

            ID2D1PathGeometry* pPath = nullptr;
            if (SUCCEEDED(pFactory->CreatePathGeometry(&pPath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pPath->Open(&pSink))) {
                    pSink->BeginFigure(pt1, D2D1_FIGURE_BEGIN_FILLED);
                    pSink->AddLine(pt2);
                    pSink->AddLine(pt3);
                    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    pSink->Close();
                    SafeRelease(&pSink);
                    pRT->FillGeometry(pPath, pArrowBrush);
                }
                SafeRelease(&pPath);
            }
        }
        SafeRelease(&pArrowBrush);
    }


    // --- MODIFIED: Foul Text (Large Red, Bottom Center) ---
    if (foulCommitted && currentGameState != SHOT_IN_PROGRESS) {
        ID2D1SolidColorBrush* pFoulBrush = nullptr;
        pRT->CreateSolidColorBrush(FOUL_TEXT_COLOR, &pFoulBrush);
        if (pFoulBrush && pLargeTextFormat) {
            // Calculate Rect for bottom-middle area
            float foulWidth = 200.0f; // Adjust width as needed
            float foulHeight = 60.0f;
            float foulLeft = TABLE_LEFT + (TABLE_WIDTH / 2.0f) - (foulWidth / 2.0f);
            // Position below the pocketed balls bar
            float foulTop = pocketedBallsBarRect.bottom + 10.0f;
            D2D1_RECT_F foulRect = D2D1::RectF(foulLeft, foulTop, foulLeft + foulWidth, foulTop + foulHeight);

            // --- Set text alignment to center for foul text ---
            pLargeTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pLargeTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            pRT->DrawText(L"FOUL!", 5, pLargeTextFormat, &foulRect, pFoulBrush);

            // --- Restore default alignment for large text if needed elsewhere ---
            // pLargeTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            // pLargeTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            SafeRelease(&pFoulBrush);
        }
    }

    // Show AI Thinking State (Unchanged from previous step)
    if (currentGameState == AI_THINKING && pTextFormat) {
        ID2D1SolidColorBrush* pThinkingBrush = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Orange), &pThinkingBrush);
        if (pThinkingBrush) {
            D2D1_RECT_F thinkingRect = p2Rect;
            thinkingRect.top += 20; // Offset within P2 box
            // Ensure default text alignment for this
            pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            pRT->DrawText(L"Thinking...", 11, pTextFormat, &thinkingRect, pThinkingBrush);
            SafeRelease(&pThinkingBrush);
        }
    }

    SafeRelease(&pBrush);
}

void DrawPowerMeter(ID2D1RenderTarget* pRT) {
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    ID2D1SolidColorBrush* pFillBrush = nullptr;

    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LimeGreen), &pFillBrush);

    if (!pBorderBrush || !pFillBrush) {
        SafeRelease(&pBorderBrush);
        SafeRelease(&pFillBrush);
        return;
    }

    // Draw Border
    pRT->DrawRectangle(&powerMeterRect, pBorderBrush, 1.0f);

    // Calculate Fill Height
    float fillRatio = 0;
    if (isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) {
        fillRatio = shotPower / MAX_SHOT_POWER;
    }
    float fillHeight = (powerMeterRect.bottom - powerMeterRect.top) * fillRatio;
    D2D1_RECT_F fillRect = D2D1::RectF(
        powerMeterRect.left, powerMeterRect.bottom - fillHeight,
        powerMeterRect.right, powerMeterRect.bottom
    );

    // Draw Fill
    pRT->FillRectangle(&fillRect, pFillBrush);

    SafeRelease(&pBorderBrush);
    SafeRelease(&pFillBrush);
}

void DrawSpinIndicator(ID2D1RenderTarget* pRT) {
    ID2D1SolidColorBrush* pWhiteBrush = nullptr;
    ID2D1SolidColorBrush* pRedBrush = nullptr;

    pRT->CreateSolidColorBrush(CUE_BALL_COLOR, &pWhiteBrush);
    pRT->CreateSolidColorBrush(ENGLISH_DOT_COLOR, &pRedBrush);

    if (!pWhiteBrush || !pRedBrush) {
        SafeRelease(&pWhiteBrush);
        SafeRelease(&pRedBrush);
        return;
    }

    // Draw White Ball Background
    D2D1_ELLIPSE bgEllipse = D2D1::Ellipse(spinIndicatorCenter, spinIndicatorRadius, spinIndicatorRadius);
    pRT->FillEllipse(&bgEllipse, pWhiteBrush);
    pRT->DrawEllipse(&bgEllipse, pRedBrush, 0.5f); // Thin red border


    // Draw Red Dot for Spin Position
    float dotRadius = 4.0f;
    float dotX = spinIndicatorCenter.x + cueSpinX * (spinIndicatorRadius - dotRadius); // Keep dot inside edge
    float dotY = spinIndicatorCenter.y + cueSpinY * (spinIndicatorRadius - dotRadius);
    D2D1_ELLIPSE dotEllipse = D2D1::Ellipse(D2D1::Point2F(dotX, dotY), dotRadius, dotRadius);
    pRT->FillEllipse(&dotEllipse, pRedBrush);

    SafeRelease(&pWhiteBrush);
    SafeRelease(&pRedBrush);
}


void DrawPocketedBallsIndicator(ID2D1RenderTarget* pRT) {
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    ID2D1SolidColorBrush* pBallBrush = nullptr;

    // Ensure render target is valid before proceeding
    if (!pRT) return;

    HRESULT hr = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.8f), &pBgBrush); // Semi-transparent black
    if (FAILED(hr)) { SafeRelease(&pBgBrush); return; } // Exit if brush creation fails

    hr = pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &pBallBrush); // Placeholder, color will be set per ball
    if (FAILED(hr)) {
        SafeRelease(&pBgBrush);
        SafeRelease(&pBallBrush);
        return; // Exit if brush creation fails
    }

    // Draw the background bar (rounded rect)
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(pocketedBallsBarRect, 10.0f, 10.0f); // Corner radius 10
    pRT->FillRoundedRectangle(&roundedRect, pBgBrush);

    // --- Draw small circles for pocketed balls inside the bar ---

    // Calculate dimensions based on the bar's height for better scaling
    float barHeight = pocketedBallsBarRect.bottom - pocketedBallsBarRect.top;
    float ballDisplayRadius = barHeight * 0.30f; // Make balls slightly smaller relative to bar height
    float spacing = ballDisplayRadius * 2.2f; // Adjust spacing slightly
    float padding = spacing * 0.75f; // Add padding from the edges
    float center_Y = pocketedBallsBarRect.top + barHeight / 2.0f; // Vertical center

    // Starting X positions with padding
    float currentX_P1 = pocketedBallsBarRect.left + padding;
    float currentX_P2 = pocketedBallsBarRect.right - padding; // Start from right edge minus padding

    int p1DrawnCount = 0;
    int p2DrawnCount = 0;
    const int maxBallsToShow = 7; // Max balls per player in the bar

    for (const auto& b : balls) {
        if (b.isPocketed) {
            // Skip cue ball and 8-ball in this indicator
            if (b.id == 0 || b.id == 8) continue;

            bool isPlayer1Ball = (player1Info.assignedType != BallType::NONE && b.type == player1Info.assignedType);
            bool isPlayer2Ball = (player2Info.assignedType != BallType::NONE && b.type == player2Info.assignedType);

            if (isPlayer1Ball && p1DrawnCount < maxBallsToShow) {
                pBallBrush->SetColor(b.color);
                // Draw P1 balls from left to right
                D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(D2D1::Point2F(currentX_P1 + p1DrawnCount * spacing, center_Y), ballDisplayRadius, ballDisplayRadius);
                pRT->FillEllipse(&ballEllipse, pBallBrush);
                p1DrawnCount++;
            }
            else if (isPlayer2Ball && p2DrawnCount < maxBallsToShow) {
                pBallBrush->SetColor(b.color);
                // Draw P2 balls from right to left
                D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(D2D1::Point2F(currentX_P2 - p2DrawnCount * spacing, center_Y), ballDisplayRadius, ballDisplayRadius);
                pRT->FillEllipse(&ballEllipse, pBallBrush);
                p2DrawnCount++;
            }
            // Note: Balls pocketed before assignment or opponent balls are intentionally not shown here.
            // You could add logic here to display them differently if needed (e.g., smaller, grayed out).
        }
    }

    SafeRelease(&pBgBrush);
    SafeRelease(&pBallBrush);
}

void DrawBallInHandIndicator(ID2D1RenderTarget* pRT) {
    if (!isDraggingCueBall && (currentGameState != BALL_IN_HAND_P1 && currentGameState != BALL_IN_HAND_P2 && currentGameState != PRE_BREAK_PLACEMENT)) {
        return; // Only show when placing/dragging
    }

    Ball* cueBall = GetCueBall();
    if (!cueBall) return;

    ID2D1SolidColorBrush* pGhostBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.6f), &pGhostBrush); // Semi-transparent white

    if (pGhostBrush) {
        D2D1_POINT_2F drawPos;
        if (isDraggingCueBall) {
            drawPos = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y);
        }
        else {
            // If not dragging but in placement state, show at current ball pos
            drawPos = D2D1::Point2F(cueBall->x, cueBall->y);
        }

        // Check if the placement is valid before drawing differently?
        bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
        bool isValid = IsValidCueBallPosition(drawPos.x, drawPos.y, behindHeadstring);

        if (!isValid) {
            // Maybe draw red outline if invalid placement?
            pGhostBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red, 0.6f));
        }


        D2D1_ELLIPSE ghostEllipse = D2D1::Ellipse(drawPos, BALL_RADIUS, BALL_RADIUS);
        pRT->FillEllipse(&ghostEllipse, pGhostBrush);
        pRT->DrawEllipse(&ghostEllipse, pGhostBrush, 1.0f); // Outline

        SafeRelease(&pGhostBrush);
    }
}