﻿#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <fstream> // For file I/O
#include <iostream> // For some basic I/O, though not strictly necessary for just file ops
#include <vector>
#include <cmath>
#include <string>
#include <sstream> // Required for wostringstream
#include <algorithm> // Required for std::max, std::min
#include <ctime>    // Required for srand, time
#include <cstdlib> // Required for srand, rand (often included by others, but good practice)
#include <commctrl.h> // Needed for radio buttons etc. in dialog (if using native controls)
#include <mmsystem.h> // For PlaySound
#include <tchar.h> //midi func
#include <thread>
#include <atomic>
#include "resource.h"

#ifndef HAS_STD_CLAMP
template <typename T>
T clamp(const T& v, const T& lo, const T& hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
namespace std { using ::clamp; }   // inject into std:: for seamless use
#define HAS_STD_CLAMP
#endif

#pragma comment(lib, "Comctl32.lib") // Link against common controls library
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Winmm.lib") // Link against Windows Multimedia library

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
const float POCKET_RADIUS = HOLE_VISUAL_RADIUS * 1.05f; // Make detection radius slightly larger // Make detection radius match visual size (or slightly larger)
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
    CHOOSING_POCKET_P1, // NEW: Player 1 needs to call a pocket for the 8-ball
    CHOOSING_POCKET_P2, // NEW: Player 2 needs to call a pocket for the 8-ball
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

enum OpeningBreakMode {
    CPU_BREAK,
    P1_BREAK,
    FLIP_COIN_BREAK
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
//ID2D1Factory* g_pD2DFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
IDWriteFactory* pDWriteFactory = nullptr;
IDWriteTextFormat* pTextFormat = nullptr;
IDWriteTextFormat* pLargeTextFormat = nullptr; // For "Foul!"

// Game State
HWND hwndMain = nullptr;
GameState currentGameState = SHOWING_DIALOG; // Start by showing dialog
std::vector<Ball> balls;
int currentPlayer = 1; // 1 or 2
PlayerInfo player1Info = { BallType::NONE, 0, L"Vince Woods"/*"Player 1"*/ };
PlayerInfo player2Info = { BallType::NONE, 0, L"Virtus Pro"/*"CPU"*/ }; // Default P2 name
bool foulCommitted = false;
std::wstring gameOverMessage = L"";
bool firstBallPocketedAfterBreak = false;
std::vector<int> pocketedThisTurn;
// --- NEW: 8-Ball Pocket Call Globals ---
int calledPocketP1 = -1; // Pocket index (0-5) called by Player 1 for the 8-ball. -1 means not called.
int calledPocketP2 = -1; // Pocket index (0-5) called by Player 2 for the 8-ball.
int currentlyHoveredPocket = -1; // For visual feedback on which pocket is being hovered
std::wstring pocketCallMessage = L""; // Message like "Choose a pocket..."
     // --- NEW: Remember which pocket the 8?ball actually went into last shot
int lastEightBallPocketIndex = -1;
//int lastPocketedIndex = -1; // pocket index (0–5) of the last ball pocketed
int called = -1;
bool cueBallPocketed = false;

// --- NEW: Foul Tracking Globals ---
int firstHitBallIdThisShot = -1;      // ID of the first object ball hit by cue ball (-1 if none)
bool cueHitObjectBallThisShot = false; // Did cue ball hit an object ball this shot?
bool railHitAfterContact = false;     // Did any ball hit a rail AFTER cue hit an object ball?
// --- End New Foul Tracking Globals ---

// NEW Game Mode/AI Globals
GameMode gameMode = HUMAN_VS_HUMAN; // Default mode
AIDifficulty aiDifficulty = MEDIUM; // Default difficulty
OpeningBreakMode openingBreakMode = CPU_BREAK; // Default opening break mode
bool isPlayer2AI = false;           // Is Player 2 controlled by AI?
bool aiTurnPending = false;         // Flag: AI needs to take its turn when possible
// bool aiIsThinking = false;       // Replaced by AI_THINKING game state
// NEW: Flag to indicate if the current shot is the opening break of the game
bool isOpeningBreakShot = false;

// NEW: For AI shot planning and visualization
struct AIPlannedShot {
    float angle;
    float power;
    float spinX;
    float spinY;
    bool isValid; // Is there a valid shot planned?
};
AIPlannedShot aiPlannedShotDetails; // Stores the AI's next shot
bool aiIsDisplayingAim = false;    // True when AI has decided a shot and is in "display aim" mode
int aiAimDisplayFramesLeft = 0;  // How many frames left to display AI aim
const int AI_AIM_DISPLAY_DURATION_FRAMES = 45; // Approx 0.75 seconds at 60 FPS, adjust as needed

// Input & Aiming
POINT ptMouse = { 0, 0 };
bool isAiming = false;
bool isDraggingCueBall = false;
// --- ENSURE THIS LINE EXISTS HERE ---
bool isDraggingStick = false; // True specifically when drag initiated on the stick graphic
// --- End Ensure ---
bool isSettingEnglish = false;
D2D1_POINT_2F aimStartPoint = { 0, 0 };
float cueAngle = 0.0f;
float shotPower = 0.0f;
float cueSpinX = 0.0f; // Range -1 to 1
float cueSpinY = 0.0f; // Range -1 to 1
float pocketFlashTimer = 0.0f;
bool cheatModeEnabled = false; // Cheat Mode toggle (G key)
int draggingBallId = -1;
bool keyboardAimingActive = false; // NEW FLAG: true when arrow keys modify aim/power
MCIDEVICEID midiDeviceID = 0; //midi func
std::atomic<bool> isMusicPlaying(false); //midi func
std::thread musicThread; //midi func
void StartMidi(HWND hwnd, const TCHAR* midiPath);
void StopMidi();

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
const D2D1_COLOR_F TABLE_COLOR = D2D1::ColorF(0.1608f, 0.4000f, 0.1765f); // Darker Green NEWCOLOR (0.0f, 0.5f, 0.1f) => (0.1608f, 0.4000f, 0.1765f)
//const D2D1_COLOR_F TABLE_COLOR = D2D1::ColorF(0.0f, 0.5f, 0.1f); // Darker Green NEWCOLOR (0.0f, 0.5f, 0.1f) => (0.1608f, 0.4000f, 0.1765f)
const D2D1_COLOR_F CUSHION_COLOR = D2D1::ColorF(D2D1::ColorF(0.3608f, 0.0275f, 0.0078f)); // NEWCOLOR ::Red => (0.3608f, 0.0275f, 0.0078f)
//const D2D1_COLOR_F CUSHION_COLOR = D2D1::ColorF(D2D1::ColorF::Red); // NEWCOLOR ::Red => (0.3608f, 0.0275f, 0.0078f)
const D2D1_COLOR_F POCKET_COLOR = D2D1::ColorF(D2D1::ColorF::Black);
const D2D1_COLOR_F CUE_BALL_COLOR = D2D1::ColorF(D2D1::ColorF::White);
const D2D1_COLOR_F EIGHT_BALL_COLOR = D2D1::ColorF(D2D1::ColorF::Black);
const D2D1_COLOR_F SOLID_COLOR = D2D1::ColorF(D2D1::ColorF::Yellow); // Solids = Yellow
const D2D1_COLOR_F STRIPE_COLOR = D2D1::ColorF(D2D1::ColorF::Red);   // Stripes = Red
const D2D1_COLOR_F AIM_LINE_COLOR = D2D1::ColorF(D2D1::ColorF::White, 0.7f); // Semi-transparent white
const D2D1_COLOR_F FOUL_TEXT_COLOR = D2D1::ColorF(D2D1::ColorF::Red);
const D2D1_COLOR_F TURN_ARROW_COLOR = D2D1::ColorF(0.1333f, 0.7294f, 0.7490f); //NEWCOLOR 0.1333f, 0.7294f, 0.7490f => ::Blue
//const D2D1_COLOR_F TURN_ARROW_COLOR = D2D1::ColorF(D2D1::ColorF::Blue);
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
//bool AssignPlayerBallTypes(BallType firstPocketedType);
bool AssignPlayerBallTypes(BallType firstPocketedType,
    bool creditShooter = true);
void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed);
Ball* GetBallById(int id);
Ball* GetCueBall();
//void PlayGameMusic(HWND hwnd); //midi func
void AIBreakShot();

// Drawing Functions
void DrawScene(ID2D1RenderTarget* pRT);
void DrawTable(ID2D1RenderTarget* pRT, ID2D1Factory* pFactory);
void DrawBalls(ID2D1RenderTarget* pRT);
void DrawCueStick(ID2D1RenderTarget* pRT);
void DrawAimingAids(ID2D1RenderTarget* pRT);
void DrawUI(ID2D1RenderTarget* pRT);
void DrawPowerMeter(ID2D1RenderTarget* pRT);
void DrawSpinIndicator(ID2D1RenderTarget* pRT);
void DrawPocketedBallsIndicator(ID2D1RenderTarget* pRT);
void DrawBallInHandIndicator(ID2D1RenderTarget* pRT);
// NEW
void DrawPocketSelectionIndicator(ID2D1RenderTarget* pRT);

// Helper Functions
float GetDistance(float x1, float y1, float x2, float y2);
float GetDistanceSq(float x1, float y1, float x2, float y2);
bool IsValidCueBallPosition(float x, float y, bool checkHeadstring);
template <typename T> void SafeRelease(T** ppT);
// --- NEW HELPER FORWARD DECLARATIONS ---
bool IsPlayerOnEightBall(int player);
void CheckAndTransitionToPocketChoice(int playerID);
// --- ADD FORWARD DECLARATION FOR NEW HELPER HERE ---
float PointToLineSegmentDistanceSq(D2D1_POINT_2F p, D2D1_POINT_2F a, D2D1_POINT_2F b);
// --- End Forward Declaration ---
bool LineSegmentIntersection(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_POINT_2F p3, D2D1_POINT_2F p4, D2D1_POINT_2F& intersection); // Keep this if present

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
void LoadSettings(); // For deserialization
void SaveSettings(); // For serialization
const std::wstring SETTINGS_FILE_NAME = L"Pool-Settings.txt";
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
    float spinX = 0.0f;
    float spinY = 0.0f;
};

/*
table = TABLE_COLOR new: #29662d (0.1608, 0.4000, 0.1765) => old: (0.0f, 0.5f, 0.1f)
rail CUSHION_COLOR = #5c0702 (0.3608, 0.0275, 0.0078) => ::Red
gap = #e99d33 (0.9157, 0.6157, 0.2000) => ::Orange
winbg = #5e8863 (0.3686, 0.5333, 0.3882) => 1.0f, 1.0f, 0.803f
headstring = #47742f (0.2784, 0.4549, 0.1843) => ::White
bluearrow = #08b0a5 (0.0314, 0.6902, 0.6471) *#22babf (0.1333,0.7294,0.7490) => ::Blue
*/

// --- NEW Settings Serialization Functions ---
void SaveSettings() {
    std::ofstream outFile(SETTINGS_FILE_NAME);
    if (outFile.is_open()) {
        outFile << static_cast<int>(gameMode) << std::endl;
        outFile << static_cast<int>(aiDifficulty) << std::endl;
        outFile << static_cast<int>(openingBreakMode) << std::endl;
        outFile.close();
    }
    // else: Handle error, e.g., log or silently fail
}

void LoadSettings() {
    std::ifstream inFile(SETTINGS_FILE_NAME);
    if (inFile.is_open()) {
        int gm, aid, obm;
        if (inFile >> gm) {
            gameMode = static_cast<GameMode>(gm);
        }
        if (inFile >> aid) {
            aiDifficulty = static_cast<AIDifficulty>(aid);
        }
        if (inFile >> obm) {
            openingBreakMode = static_cast<OpeningBreakMode>(obm);
        }
        inFile.close();

        // Validate loaded settings (optional, but good practice)
        if (gameMode < HUMAN_VS_HUMAN || gameMode > HUMAN_VS_AI) gameMode = HUMAN_VS_HUMAN; // Default
        if (aiDifficulty < EASY || aiDifficulty > HARD) aiDifficulty = MEDIUM; // Default
        if (openingBreakMode < CPU_BREAK || openingBreakMode > FLIP_COIN_BREAK) openingBreakMode = CPU_BREAK; // Default
    }
    // else: File doesn't exist or couldn't be opened, use defaults (already set in global vars)
}
// --- End Settings Serialization Functions ---

// --- NEW Dialog Procedure ---
INT_PTR CALLBACK NewGameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        // --- ACTION 4: Center Dialog Box ---
// Optional: Force centering if default isn't working
        RECT rcDlg, rcOwner, rcScreen;
        HWND hwndOwner = GetParent(hDlg); // GetParent(hDlg) might be better if hwndMain is passed
        if (hwndOwner == NULL) hwndOwner = GetDesktopWindow();

        GetWindowRect(hwndOwner, &rcOwner);
        GetWindowRect(hDlg, &rcDlg);
        CopyRect(&rcScreen, &rcOwner); // Use owner rect as reference bounds

        // Offset the owner rect relative to the screen if it's not the desktop
        if (GetParent(hDlg) != NULL) { // If parented to main window (passed to DialogBoxParam)
            OffsetRect(&rcOwner, -rcScreen.left, -rcScreen.top);
            OffsetRect(&rcDlg, -rcScreen.left, -rcScreen.top);
            OffsetRect(&rcScreen, -rcScreen.left, -rcScreen.top);
        }


        // Calculate centered position
        int x = rcOwner.left + (rcOwner.right - rcOwner.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcOwner.top + (rcOwner.bottom - rcOwner.top - (rcDlg.bottom - rcDlg.top)) / 2;

        // Ensure it stays within screen bounds (optional safety)
        x = std::max(static_cast<int>(rcScreen.left), x);
        y = std::max(static_cast<int>(rcScreen.top), y);
        if (x + (rcDlg.right - rcDlg.left) > rcScreen.right)
            x = rcScreen.right - (rcDlg.right - rcDlg.left);
        if (y + (rcDlg.bottom - rcDlg.top) > rcScreen.bottom)
            y = rcScreen.bottom - (rcDlg.bottom - rcDlg.top);


        // Set the dialog position
        SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        // --- End Centering Code ---

        // Set initial state based on current global settings (or defaults)
        CheckRadioButton(hDlg, IDC_RADIO_2P, IDC_RADIO_CPU, (gameMode == HUMAN_VS_HUMAN) ? IDC_RADIO_2P : IDC_RADIO_CPU);

        CheckRadioButton(hDlg, IDC_RADIO_EASY, IDC_RADIO_HARD,
            (aiDifficulty == EASY) ? IDC_RADIO_EASY : ((aiDifficulty == MEDIUM) ? IDC_RADIO_MEDIUM : IDC_RADIO_HARD));

        // Enable/Disable AI group based on initial mode
        EnableWindow(GetDlgItem(hDlg, IDC_GROUP_AI), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_EASY), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_MEDIUM), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_HARD), gameMode == HUMAN_VS_AI);
        // Set initial state for Opening Break Mode
        CheckRadioButton(hDlg, IDC_RADIO_CPU_BREAK, IDC_RADIO_FLIP_BREAK,
            (openingBreakMode == CPU_BREAK) ? IDC_RADIO_CPU_BREAK : ((openingBreakMode == P1_BREAK) ? IDC_RADIO_P1_BREAK : IDC_RADIO_FLIP_BREAK));
        // Enable/Disable Opening Break group based on initial mode
        EnableWindow(GetDlgItem(hDlg, IDC_GROUP_BREAK_MODE), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_CPU_BREAK), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_P1_BREAK), gameMode == HUMAN_VS_AI);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_FLIP_BREAK), gameMode == HUMAN_VS_AI);
    }
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
            // Also enable/disable Opening Break Mode group
            EnableWindow(GetDlgItem(hDlg, IDC_GROUP_BREAK_MODE), isCPU);
            EnableWindow(GetDlgItem(hDlg, IDC_RADIO_CPU_BREAK), isCPU);
            EnableWindow(GetDlgItem(hDlg, IDC_RADIO_P1_BREAK), isCPU);
            EnableWindow(GetDlgItem(hDlg, IDC_RADIO_FLIP_BREAK), isCPU);
        }
        return (INT_PTR)TRUE;

        case IDOK:
            // Retrieve selected options and store in global variables
            if (IsDlgButtonChecked(hDlg, IDC_RADIO_CPU) == BST_CHECKED) {
                gameMode = HUMAN_VS_AI;
                if (IsDlgButtonChecked(hDlg, IDC_RADIO_EASY) == BST_CHECKED) aiDifficulty = EASY;
                else if (IsDlgButtonChecked(hDlg, IDC_RADIO_MEDIUM) == BST_CHECKED) aiDifficulty = MEDIUM;
                else if (IsDlgButtonChecked(hDlg, IDC_RADIO_HARD) == BST_CHECKED) aiDifficulty = HARD;

                if (IsDlgButtonChecked(hDlg, IDC_RADIO_CPU_BREAK) == BST_CHECKED) openingBreakMode = CPU_BREAK;
                else if (IsDlgButtonChecked(hDlg, IDC_RADIO_P1_BREAK) == BST_CHECKED) openingBreakMode = P1_BREAK;
                else if (IsDlgButtonChecked(hDlg, IDC_RADIO_FLIP_BREAK) == BST_CHECKED) openingBreakMode = FLIP_COIN_BREAK;
            }
            else {
                gameMode = HUMAN_VS_HUMAN;
                // openingBreakMode doesn't apply to HvsH, can leave as is or reset
            }
            SaveSettings(); // Save settings when OK is pressed
            EndDialog(hDlg, IDOK); // Close dialog, return IDOK
            return (INT_PTR)TRUE;

        case IDCANCEL: // Handle Cancel or closing the dialog
            // Optionally, could reload settings here if you want cancel to revert to previously saved state
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
            case EASY: player2Info.name = L"Virtus Pro (Easy)"/*"CPU (Easy)"*/; break;
            case MEDIUM: player2Info.name = L"Virtus Pro (Medium)"/*"CPU (Medium)"*/; break;
            case HARD: player2Info.name = L"Virtus Pro (Hard)"/*"CPU (Hard)"*/; break;
            }
        }
        else {
            player2Info.name = L"Billy Ray Cyrus"/*"Player 2"*/;
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

    // --- NEW: Load settings at startup ---
    LoadSettings();

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
        case EASY: player2Info.name = L"Virtus Pro (Easy)"/*"CPU (Easy)"*/; break;
        case MEDIUM:player2Info.name = L"Virtus Pro (Medium)"/*"CPU (Medium)"*/; break;
        case HARD: player2Info.name = L"Virtus Pro (Hard)"/*"CPU (Hard)"*/; break;
        }
    }
    else {
        player2Info.name = L"Billy Ray Cyrus"/*"Player 2"*/;
    }
    // --- End of Dialog Logic ---


    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Direct2D_8BallPool";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); // Use your actual icon ID here

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Window Registration Failed.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return -1;
    }

    // --- ACTION 4: Calculate Centered Window Position ---
    const int WINDOW_WIDTH = 1000; // Define desired width
    const int WINDOW_HEIGHT = 700; // Define desired height
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowX = (screenWidth - WINDOW_WIDTH) / 2;
    int windowY = (screenHeight - WINDOW_HEIGHT) / 2;

    // --- Change Window Title based on mode ---
    std::wstring windowTitle = L"Direct2D 8-Ball Pool";
    if (gameMode == HUMAN_VS_HUMAN) windowTitle += L" (Human vs Human)";
    else windowTitle += L" (Human vs " + player2Info.name + L")";

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX; // No WS_THICKFRAME, No WS_MAXIMIZEBOX

    hwndMain = CreateWindowEx(
        0, L"Direct2D_8BallPool", windowTitle.c_str(), dwStyle,
        windowX, windowY, WINDOW_WIDTH, WINDOW_HEIGHT,
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
    Sleep(500); // Allow window to fully initialize before starting the countdown //midi func
    StartMidi(hwndMain, TEXT("BSQ.MID")); // Replace with your MIDI filename
    //PlayGameMusic(hwndMain); //midi func

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
    SaveSettings(); // Save settings on exit
    CoUninitialize();

    return (int)msg.wParam;
}

// --- WndProc ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Declare cueBall pointer once at the top, used in multiple cases
    // For clarity, often better to declare within each case where needed.
    Ball* cueBall = nullptr; // Initialize to nullptr
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
        // --- MODIFIED: Handle More Keys ---
    case WM_KEYDOWN:
    { // Add scope for variable declarations

        // --- FIX: Get Cue Ball pointer for this scope ---
        cueBall = GetCueBall();
        // We might allow some keys even if cue ball is gone (like F1/F2), but actions need it
        // --- End Fix ---

        // Check which player can interact via keyboard (Humans only)
        bool canPlayerControl = ((currentPlayer == 1 && (currentGameState == PLAYER1_TURN || currentGameState == AIMING || currentGameState == BREAKING || currentGameState == BALL_IN_HAND_P1 || currentGameState == PRE_BREAK_PLACEMENT)) ||
            (currentPlayer == 2 && !isPlayer2AI && (currentGameState == PLAYER2_TURN || currentGameState == AIMING || currentGameState == BREAKING || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT)));

        // --- F1 / F2 Keys (Always available) ---
        if (wParam == VK_F2) {
            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            ResetGame(hInstance); // Call reset function
            return 0; // Indicate key was processed
        }
        else if (wParam == VK_F1) {
            MessageBox(hwnd,
                L"Direct2D-based StickPool game made in C++ from scratch (2764+ lines of code)\n" // Update line count if needed
                L"First successful Clone in C++ (no other sites or projects were there to glean from.) Made /w AI assist\n"
                L"(others were in JS/ non-8-Ball in C# etc.) w/o OOP and Graphics Frameworks all in a Single file.\n"
                L"Copyright (C) 2025 Evans Thorpemorton, Entisoft Solutions.\n"
                L"Includes AI Difficulty Modes, Aim-Trajectory For Table Rails + Hard Angles TipShots. || F2=New Game",
                L"About This Game", MB_OK | MB_ICONINFORMATION);
            return 0; // Indicate key was processed
        }

        // Check for 'M' key (uppercase or lowercase)
            // Toggle music with "M"
        if (wParam == 'M' || wParam == 'm') {
            //static bool isMusicPlaying = false;
            if (isMusicPlaying) {
                // Stop the music
                StopMidi();
                isMusicPlaying = false;
            }
            else {
                // Build the MIDI file path
                TCHAR midiPath[MAX_PATH];
                GetModuleFileName(NULL, midiPath, MAX_PATH);
                // Keep only the directory part
                TCHAR* lastBackslash = _tcsrchr(midiPath, '\\');
                if (lastBackslash != NULL) {
                    *(lastBackslash + 1) = '\0';
                }
                // Append the MIDI filename
                _tcscat_s(midiPath, MAX_PATH, TEXT("BSQ.MID")); // Adjust filename if needed

                // Start playing MIDI
                StartMidi(hwndMain, midiPath);
                isMusicPlaying = true;
            }
        }


        // --- Player Interaction Keys (Only if allowed) ---
        if (canPlayerControl) {
            // --- Get Shift Key State ---
            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            float angleStep = shiftPressed ? 0.05f : 0.01f; // Base step / Faster step (Adjust as needed) // Multiplier was 5x
            float powerStep = 0.2f; // Power step (Adjust as needed)

            switch (wParam) {
            case VK_LEFT: // Rotate Cue Stick Counter-Clockwise
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    cueAngle -= angleStep;
                    // Normalize angle (keep between 0 and 2*PI)
                    if (cueAngle < 0) cueAngle += 2 * PI;
                    // Ensure state shows aiming visuals if turn just started
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = false; // Keyboard adjust doesn't use mouse aiming state
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;

            case VK_RIGHT: // Rotate Cue Stick Clockwise
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    cueAngle += angleStep;
                    // Normalize angle (keep between 0 and 2*PI)
                    if (cueAngle >= 2 * PI) cueAngle -= 2 * PI;
                    // Ensure state shows aiming visuals if turn just started
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = false;
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;

            case VK_UP: // Decrease Shot Power
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    shotPower -= powerStep;
                    if (shotPower < 0.0f) shotPower = 0.0f;
                    // Ensure state shows aiming visuals if turn just started
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = true; // Keyboard adjust doesn't use mouse aiming state
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;

            case VK_DOWN: // Increase Shot Power
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    shotPower += powerStep;
                    if (shotPower > MAX_SHOT_POWER) shotPower = MAX_SHOT_POWER;
                    // Ensure state shows aiming visuals if turn just started
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = true;
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;

            case VK_SPACE: // Trigger Shot
                if ((currentGameState == AIMING || currentGameState == BREAKING || currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN)
                    && currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING)
                {
                    if (shotPower > 0.15f) { // Use same threshold as mouse
                       // Reset foul flags BEFORE applying shot
                        firstHitBallIdThisShot = -1;
                        cueHitObjectBallThisShot = false;
                        railHitAfterContact = false;

                        // Play sound & Apply Shot
                        std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
                        ApplyShot(shotPower, cueAngle, cueSpinX, cueSpinY);

                        // Update State
                        currentGameState = SHOT_IN_PROGRESS;
                        foulCommitted = false;
                        pocketedThisTurn.clear();
                        shotPower = 0; // Reset power after shooting
                        isAiming = false; isDraggingStick = false; // Reset aiming flags
                        keyboardAimingActive = false;
                    }
                }
                break;

            case VK_ESCAPE: // Cancel Aim/Shot Setup
                if ((currentGameState == AIMING || currentGameState == BREAKING) || shotPower > 0)
                {
                    shotPower = 0.0f;
                    isAiming = false;
                    isDraggingStick = false;
                    keyboardAimingActive = false;
                    // Revert to basic turn state if not breaking
                    if (currentGameState != BREAKING) {
                        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                    }
                    //if (currentPlayer == 1) calledPocketP1 = -1;
                    //else                  calledPocketP2 = -1;
                }
                break;

            case 'G': // Toggle Cheat Mode
                cheatModeEnabled = !cheatModeEnabled;
                if (cheatModeEnabled)
                    MessageBeep(MB_ICONEXCLAMATION); // Play a beep when enabling
                else
                    MessageBeep(MB_OK); // Play a different beep when disabling
                break;

            default:
                // Allow default processing for other keys if needed
                // return DefWindowProc(hwnd, msg, wParam, lParam); // Usually not needed for WM_KEYDOWN
                break;
            } // End switch(wParam) for player controls
            return 0; // Indicate player control key was processed
        } // End if(canPlayerControl)
    } // End scope for WM_KEYDOWN case
    // If key wasn't F1/F2 and player couldn't control, maybe allow default processing?
    // return DefWindowProc(hwnd, msg, wParam, lParam); // Or just return 0
    return 0;

    case WM_MOUSEMOVE: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        // --- NEW LOGIC: Handle Pocket Hover ---
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
            (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {
            int oldHover = currentlyHoveredPocket;
            currentlyHoveredPocket = -1; // Reset
            for (int i = 0; i < 6; ++i) {
                if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < HOLE_VISUAL_RADIUS * HOLE_VISUAL_RADIUS * 2.25f) {
                    currentlyHoveredPocket = i;
                    break;
                }
            }
            if (oldHover != currentlyHoveredPocket) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            // Do NOT return 0 here, allow normal mouse angle update to continue
        }
        // --- END NEW LOGIC ---


        cueBall = GetCueBall(); // Declare and get cueBall pointer

        if (isDraggingCueBall && cheatModeEnabled && draggingBallId != -1) {
            Ball* ball = GetBallById(draggingBallId);
            if (ball) {
                ball->x = (float)ptMouse.x;
                ball->y = (float)ptMouse.y;
                ball->vx = ball->vy = 0.0f;
            }
            return 0;
        }

        if (!cueBall) return 0;

        // Update Aiming Logic (Check player turn)
        if (isDraggingCueBall &&
            ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                (!isPlayer2AI && currentPlayer == 2 && currentGameState == BALL_IN_HAND_P2) ||
                currentGameState == PRE_BREAK_PLACEMENT))
        {
            bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
            // Tentative position update
            cueBall->x = (float)ptMouse.x;
            cueBall->y = (float)ptMouse.y;
            cueBall->vx = cueBall->vy = 0;
        }
        else if ((isAiming || isDraggingStick) &&
            ((currentPlayer == 1 && (currentGameState == AIMING || currentGameState == BREAKING)) ||
                (!isPlayer2AI && currentPlayer == 2 && (currentGameState == AIMING || currentGameState == BREAKING))))
        {
            //NEW2 MOUSEBOUND CODE = START
                /*// Clamp mouse inside table bounds during aiming
                if (ptMouse.x < TABLE_LEFT) ptMouse.x = TABLE_LEFT;
            if (ptMouse.x > TABLE_RIGHT) ptMouse.x = TABLE_RIGHT;
            if (ptMouse.y < TABLE_TOP) ptMouse.y = TABLE_TOP;
            if (ptMouse.y > TABLE_BOTTOM) ptMouse.y = TABLE_BOTTOM;*/
            //NEW2 MOUSEBOUND CODE = END
            // Aiming drag updates angle and power
            float dx = (float)ptMouse.x - cueBall->x;
            float dy = (float)ptMouse.y - cueBall->y;
            if (dx != 0 || dy != 0) cueAngle = atan2f(dy, dx);
            //float pullDist = GetDistance((float)ptMouse.x, (float)ptMouse.y, aimStartPoint.x, aimStartPoint.y);
            //shotPower = std::min(pullDist / 10.0f, MAX_SHOT_POWER);
            if (!keyboardAimingActive) { // Only update shotPower if NOT keyboard aiming
                float pullDist = GetDistance((float)ptMouse.x, (float)ptMouse.y, aimStartPoint.x, aimStartPoint.y);
                shotPower = std::min(pullDist / 10.0f, MAX_SHOT_POWER);
            }
        }
        else if (isSettingEnglish &&
            ((currentPlayer == 1 && (currentGameState == PLAYER1_TURN || currentGameState == AIMING || currentGameState == BREAKING)) ||
                (!isPlayer2AI && currentPlayer == 2 && (currentGameState == PLAYER2_TURN || currentGameState == AIMING || currentGameState == BREAKING))))
        {
            // Setting English
            float dx = (float)ptMouse.x - spinIndicatorCenter.x;
            float dy = (float)ptMouse.y - spinIndicatorCenter.y;
            float dist = GetDistance(dx, dy, 0, 0);
            if (dist > spinIndicatorRadius) { dx *= spinIndicatorRadius / dist; dy *= spinIndicatorRadius / dist; }
            cueSpinX = dx / spinIndicatorRadius;
            cueSpinY = dy / spinIndicatorRadius;
        }
        else {
            //DISABLE PERM AIMING = START
            /*// Update visual angle even when not aiming/dragging (Check player turn)
            bool canUpdateVisualAngle = ((currentPlayer == 1 && (currentGameState == PLAYER1_TURN || currentGameState == BALL_IN_HAND_P1)) ||
                (currentPlayer == 2 && !isPlayer2AI && (currentGameState == PLAYER2_TURN || currentGameState == BALL_IN_HAND_P2)) ||
                currentGameState == PRE_BREAK_PLACEMENT || currentGameState == BREAKING || currentGameState == AIMING);

            if (canUpdateVisualAngle && !isDraggingCueBall && !isAiming && !isDraggingStick && !keyboardAimingActive) // NEW: Prevent mouse override if keyboard aiming
            {
                // NEW MOUSEBOUND CODE = START
                    // Only update cue angle if mouse is inside the playable table area
                if (ptMouse.x >= TABLE_LEFT && ptMouse.x <= TABLE_RIGHT &&
                    ptMouse.y >= TABLE_TOP && ptMouse.y <= TABLE_BOTTOM)
                {
                    // NEW MOUSEBOUND CODE = END
                    Ball* cb = cueBall; // Use function-scope cueBall // Already got cueBall above
                    if (cb) {
                        float dx = (float)ptMouse.x - cb->x;
                        float dy = (float)ptMouse.y - cb->y;
                        if (dx != 0 || dy != 0) cueAngle = atan2f(dy, dx);
                    }
                } //NEW MOUSEBOUND CODE LINE = DISABLE
            }*/
            //DISABLE PERM AIMING = END
        }
        return 0;
    } // End WM_MOUSEMOVE

    case WM_LBUTTONDOWN: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        // --- FOOLPROOF FIX: This block implements the two-stage pocket selection ---
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
            (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {

            int clickedPocketIndex = -1;
            // STAGE 1, STEP 1: Check if the click was on any of the 6 pockets
            for (int i = 0; i < 6; ++i) {
                if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < HOLE_VISUAL_RADIUS * HOLE_VISUAL_RADIUS * 2.25f) {
                    clickedPocketIndex = i;
                    break;
                }
            }

            if (clickedPocketIndex != -1) {
                // STAGE 1, STEP 2: Player clicked on a pocket. Update the choice.
                // We DO NOT change the game state here. This allows re-selection.
                if (currentPlayer == 1) calledPocketP1 = clickedPocketIndex;
                else calledPocketP2 = clickedPocketIndex;
                InvalidateRect(hwnd, NULL, FALSE); // Redraw to show the arrow has moved.
                return 0; // Consume the click and stay in CHOOSING_POCKET state.
            }

            // STAGE 2, STEP 1: Check if the player is clicking the cue ball to confirm.
            Ball* cueBall = GetCueBall();
            int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
            if (cueBall && calledPocket != -1 && GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y) < BALL_RADIUS * BALL_RADIUS * 25) {
                // STAGE 2, STEP 2: A pocket has been selected, and the player now clicks the cue ball.
                // NOW we transition to the normal aiming state.
                currentGameState = AIMING; // Go to a generic aiming state.
                pocketCallMessage = L""; // Clear the "Choose a pocket..." message.
                isAiming = true; // Prepare for aiming.
                aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y); // Use your existing aim start variable.
                return 0;
            }

            // If they click anywhere else (not a pocket, not the cue ball), do nothing.
            return 0;
        }

        /*// --- FOOLPROOF FIX: This block handles re-selectable pocket choice ---
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
            (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {

            int clickedPocketIndex = -1;
            // Check if the click was on any of the 6 pockets
            for (int i = 0; i < 6; ++i) {
                if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < HOLE_VISUAL_RADIUS * HOLE_VISUAL_RADIUS * 2.25f) {
                    clickedPocketIndex = i;
                    break;
                }
            }

            if (clickedPocketIndex != -1) { // Player clicked on a pocket
                // FIX: Update the called pocket, but DO NOT change the game state.
                // This allows the player to click another pocket to change their mind.
                if (currentPlayer == 1) calledPocketP1 = clickedPocketIndex;
                else calledPocketP2 = clickedPocketIndex;
                InvalidateRect(hwnd, NULL, FALSE); // Redraw to show updated arrow
                return 0; // Consume the click and stay in CHOOSING_POCKET state
            }

            // FIX: Add new logic to CONFIRM the choice by clicking the cue ball.
            Ball* cueBall = GetCueBall();
            int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
            if (cueBall && calledPocket != -1 && GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y) < BALL_RADIUS * BALL_RADIUS * 25) {
                // A pocket has been selected, and the player now clicks the cue ball.
                // NOW we transition to the normal aiming state.
                currentGameState = AIMING; // Go to aiming, not PLAYER1_TURN
                pocketCallMessage = L""; // Clear the "Choose a pocket..." message
                isAiming = true; // Prepare for aiming
                aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y);
                return 0;
            }

            // If they click anywhere else (not a pocket, not the cue ball), do nothing.
            return 0;
        }*/

        /*// --- handle pocket re-selection when choosing 8-ball pocket ---
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1)
            || (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI))
        {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            for (int i = 0; i < 6; ++i) {
                float dx = pt.x - pocketPositions[i].x;
                float dy = pt.y - pocketPositions[i].y;
                if (dx * dx + dy * dy <= POCKET_RADIUS * POCKET_RADIUS) {
                    // 1) Record the call
                    if (currentPlayer == 1) calledPocketP1 = i;
                    else                  calledPocketP2 = i;
                    // 2) Clear any prompt text
                    pocketCallMessage.clear();
                    // 3) Return to normal aiming state
                    currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                    // 4) Redraw (arrow stays because calledPocketP* >= 0)
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0; // consume click
                }
            }
            return 0; // clicked outside ? stay in pocket?call until a valid pocket is chosen
        }*/

        // … rest of your click?to?aim logic …

        //replaced /w new code
        /*
        // --- FIX: Add this entire block at the top of WM_LBUTTONDOWN ---
// This handles input specifically for the pocket selection state.
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
            (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {

            int clickedPocketIndex = -1;
            // Check if the click was on any of the 6 pockets
            for (int i = 0; i < 6; ++i) {
                if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < HOLE_VISUAL_RADIUS * HOLE_VISUAL_RADIUS * 2.25f) {
                    clickedPocketIndex = i;
                    break;
                }
            }

            if (clickedPocketIndex != -1) {
                // A pocket was clicked. Update the selection but STAY in the choosing state.
                // This allows the player to click another pocket to change their mind.
                if (currentPlayer == 1) calledPocketP1 = clickedPocketIndex;
                else calledPocketP2 = clickedPocketIndex;
                InvalidateRect(hwnd, NULL, FALSE); // Redraw to show the arrow has moved.
                return 0; // Consume the click and wait for the next action.
            }

            // If the player clicks the CUE BALL, that confirms their pocket selection.
            Ball* cueBall = GetCueBall();
            int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
            if (cueBall && calledPocket != -1 && GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y) < BALL_RADIUS * BALL_RADIUS * 25) {
                // A pocket has been selected, and the player now clicks the cue ball.
                // NOW we transition to the normal aiming state.
                currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                pocketCallMessage = L""; // Clear the "Choose a pocket..." message
                isAiming = true; // Prepare for aiming
                aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y); // Use your existing aim start variable
                return 0;
            }

            // If they click anywhere else (not a pocket, not the cue ball), do nothing.
            return 0;
        }
        // --- END OF THE NEW BLOCK ---
        */
        //new code ends here

        if (cheatModeEnabled) {
            // Allow dragging any ball freely
            for (Ball& ball : balls) {
                float distSq = GetDistanceSq(ball.x, ball.y, (float)ptMouse.x, (float)ptMouse.y);
                if (distSq <= BALL_RADIUS * BALL_RADIUS * 4) { // Click near ball
                    isDraggingCueBall = true;
                    draggingBallId = ball.id;
                    if (ball.id == 0) {
                        // If dragging cue ball manually, ensure we stay in Ball-In-Hand state
                        if (currentPlayer == 1)
                            currentGameState = BALL_IN_HAND_P1;
                        else if (currentPlayer == 2 && !isPlayer2AI)
                            currentGameState = BALL_IN_HAND_P2;
                    }
                    return 0;
                }
            }
        }

        Ball* cueBall = GetCueBall(); // Declare and get cueBall pointer            

        // Check which player is allowed to interact via mouse click
        bool canPlayerClickInteract = ((currentPlayer == 1) || (currentPlayer == 2 && !isPlayer2AI));
        // Define states where interaction is generally allowed
        bool canInteractState = (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN ||
            currentGameState == AIMING || currentGameState == BREAKING ||
            currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 ||
            currentGameState == PRE_BREAK_PLACEMENT);

        // Check Spin Indicator first (Allow if player's turn/aim phase)
        if (canPlayerClickInteract && canInteractState) {
            float spinDistSq = GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, spinIndicatorCenter.x, spinIndicatorCenter.y);
            if (spinDistSq < spinIndicatorRadius * spinIndicatorRadius * 1.2f) {
                isSettingEnglish = true;
                float dx = (float)ptMouse.x - spinIndicatorCenter.x;
                float dy = (float)ptMouse.y - spinIndicatorCenter.y;
                float dist = GetDistance(dx, dy, 0, 0);
                if (dist > spinIndicatorRadius) { dx *= spinIndicatorRadius / dist; dy *= spinIndicatorRadius / dist; }
                cueSpinX = dx / spinIndicatorRadius;
                cueSpinY = dy / spinIndicatorRadius;
                isAiming = false; isDraggingStick = false; isDraggingCueBall = false;
                return 0;
            }
        }

        if (!cueBall) return 0;

        // Check Ball-in-Hand placement/drag
        bool isPlacingBall = (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT);
        bool isPlayerAllowedToPlace = (isPlacingBall &&
            ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                (currentPlayer == 2 && !isPlayer2AI && currentGameState == BALL_IN_HAND_P2) ||
                (currentGameState == PRE_BREAK_PLACEMENT))); // Allow current player in break setup

        if (isPlayerAllowedToPlace) {
            float distSq = GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y);
            if (distSq < BALL_RADIUS * BALL_RADIUS * 9.0f) {
                isDraggingCueBall = true;
                isAiming = false; isDraggingStick = false;
            }
            else {
                bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
                if (IsValidCueBallPosition((float)ptMouse.x, (float)ptMouse.y, behindHeadstring)) {
                    cueBall->x = (float)ptMouse.x; cueBall->y = (float)ptMouse.y;
                    cueBall->vx = 0; cueBall->vy = 0;
                    isDraggingCueBall = false;
                    // Transition state
                    if (currentGameState == PRE_BREAK_PLACEMENT) currentGameState = BREAKING;
                    else if (currentGameState == BALL_IN_HAND_P1) currentGameState = PLAYER1_TURN;
                    else if (currentGameState == BALL_IN_HAND_P2) currentGameState = PLAYER2_TURN;
                    cueAngle = 0.0f;
                }
            }
            return 0;
        }

        // Check for starting Aim (Cue Ball OR Stick)
        bool canAim = ((currentPlayer == 1 && (currentGameState == PLAYER1_TURN || currentGameState == BREAKING)) ||
            (currentPlayer == 2 && !isPlayer2AI && (currentGameState == PLAYER2_TURN || currentGameState == BREAKING)));

        if (canAim) {
            const float stickDrawLength = 150.0f * 1.4f;
            float currentStickAngle = cueAngle + PI;
            D2D1_POINT_2F currentStickEnd = D2D1::Point2F(cueBall->x + cosf(currentStickAngle) * stickDrawLength, cueBall->y + sinf(currentStickAngle) * stickDrawLength);
            D2D1_POINT_2F currentStickTip = D2D1::Point2F(cueBall->x + cosf(currentStickAngle) * 5.0f, cueBall->y + sinf(currentStickAngle) * 5.0f);
            float distToStickSq = PointToLineSegmentDistanceSq(D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y), currentStickTip, currentStickEnd);
            float stickClickThresholdSq = 36.0f;
            float distToCueBallSq = GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y);
            float cueBallClickRadiusSq = BALL_RADIUS * BALL_RADIUS * 25;

            bool clickedStick = (distToStickSq < stickClickThresholdSq);
            bool clickedCueArea = (distToCueBallSq < cueBallClickRadiusSq);

            if (clickedStick || clickedCueArea) {
                isDraggingStick = clickedStick && !clickedCueArea;
                isAiming = clickedCueArea;
                aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y);
                shotPower = 0;
                float dx = (float)ptMouse.x - cueBall->x;
                float dy = (float)ptMouse.y - cueBall->y;
                if (dx != 0 || dy != 0) cueAngle = atan2f(dy, dx);
                if (currentGameState != BREAKING) currentGameState = AIMING;
            }
        }
        return 0;
    } // End WM_LBUTTONDOWN


    case WM_LBUTTONUP: {
        // --- FOOLPROOF FIX for Cheat Mode Scoring ---
        if (cheatModeEnabled && draggingBallId != -1) {
            Ball* b = GetBallById(draggingBallId);
            if (b) {
                for (int p = 0; p < 6; ++p) {
                    float dx = b->x - pocketPositions[p].x;
                    float dy = b->y - pocketPositions[p].y;
                    if (dx * dx + dy * dy <= POCKET_RADIUS * POCKET_RADIUS) {
                        // --- This is the new, "smarter" logic ---
                        b->isPocketed = true; // Pocket the ball visually.

                        // If the table is open, assign types based on this cheated ball.
                        if (player1Info.assignedType == BallType::NONE && b->id != 0 && b->id != 8) {
                            AssignPlayerBallTypes(b->type, false);
                        }

                        // Now, correctly update the score for the right player.
                        if (b->id != 0 && b->id != 8) {
                            if (b->type == player1Info.assignedType) {
                                player1Info.ballsPocketedCount++;
                            }
                            else if (b->type == player2Info.assignedType) {
                                player2Info.ballsPocketedCount++;
                            }
                        }
                        break; // Stop checking pockets.
                    }
                }
            }
        }

        /*if (cheatModeEnabled && draggingBallId != -1) {
            Ball* b = GetBallById(draggingBallId);
            if (b) {
                for (int p = 0; p < 6; ++p) {
                    float dx = b->x - pocketPositions[p].x;
                    float dy = b->y - pocketPositions[p].y;
                    if (dx * dx + dy * dy <= POCKET_RADIUS * POCKET_RADIUS) {
                        // --- Assign ball type on first cheat-pocket if table still open ---
                        if (player1Info.assignedType == BallType::NONE
                            && player2Info.assignedType == BallType::NONE
                            && (b->type == BallType::SOLID || b->type == BallType::STRIPE))
                        {
                            // In cheat mode, let's just assign to the current player
                            AssignPlayerBallTypes(b->type);
                        }
                        b->isPocketed = true;
                        pocketedThisTurn.push_back(b->id);

                        // --- FIX FOR CHEAT MODE SCORING ---
                        // Immediately increment the correct player's count based on ball type,
                        // not whose turn it is.
                        if (b->id != 0 && b->id != 8) {
                            if (b->type == player1Info.assignedType) {
                                player1Info.ballsPocketedCount++;
                            }
                            else if (b->type == player2Info.assignedType) {
                                player2Info.ballsPocketedCount++;
                            }
                        }
                        // --- END FIX ---
                        // --- NEW: If this was the 7th ball, trigger the arrow call UI ---
                        if (b->id != 8) {
                            PlayerInfo& shooter = (currentPlayer == 1 ? player1Info : player2Info);
                            if (shooter.ballsPocketedCount >= 7
                                && calledPocketP1 < 0
                                && calledPocketP2 < 0)
                            {
                                currentGameState = (currentPlayer == 1)
                                    ? CHOOSING_POCKET_P1
                                    : CHOOSING_POCKET_P2;
                            }
                            else {
                                // For any other cheat?pocket, keep the turn so you can continue aiming
                                currentGameState = (currentPlayer == 1)
                                    ? PLAYER1_TURN
                                    : PLAYER2_TURN;
                            }
                        }
                        // --- NEW: If it was the 8-Ball, award instant victory ---
                        else {
                            currentGameState = GAME_OVER;
                            gameOverMessage = (currentPlayer == 1 ? player1Info.name : player2Info.name)
                                + std::wstring(L" Wins!");
                        }
                        break;
                    }
                }
            }
        }*/

        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        Ball* cueBall = GetCueBall(); // Get cueBall pointer

        // Check for releasing aim drag (Stick OR Cue Ball)
        if ((isAiming || isDraggingStick) &&
            ((currentPlayer == 1 && (currentGameState == AIMING || currentGameState == BREAKING)) ||
                (!isPlayer2AI && currentPlayer == 2 && (currentGameState == AIMING || currentGameState == BREAKING))))
        {
            bool wasAiming = isAiming;
            bool wasDraggingStick = isDraggingStick;
            isAiming = false; isDraggingStick = false;

            if (shotPower > 0.15f) { // Check power threshold
                if (currentGameState != AI_THINKING) {
                    firstHitBallIdThisShot = -1; cueHitObjectBallThisShot = false; railHitAfterContact = false; // Reset foul flags
                    std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
                    ApplyShot(shotPower, cueAngle, cueSpinX, cueSpinY);
                    currentGameState = SHOT_IN_PROGRESS;
                    foulCommitted = false; pocketedThisTurn.clear();
                }
            }
            else if (currentGameState != AI_THINKING) { // Revert state if power too low
                if (currentGameState == BREAKING) { /* Still breaking */ }
                else {
                    currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                    if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = false;
                }
            }
            shotPower = 0; // Reset power indicator regardless
        }

        // Handle releasing cue ball drag (placement)
        if (isDraggingCueBall) {
            isDraggingCueBall = false;
            // Check player allowed to place
            bool isPlacingState = (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT);
            bool isPlayerAllowed = (isPlacingState &&
                ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                    (currentPlayer == 2 && !isPlayer2AI && currentGameState == BALL_IN_HAND_P2) ||
                    (currentGameState == PRE_BREAK_PLACEMENT)));

            if (isPlayerAllowed && cueBall) {
                bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
                if (IsValidCueBallPosition(cueBall->x, cueBall->y, behindHeadstring)) {
                    // Finalize position already set by mouse move
                    // Transition state
                    if (currentGameState == PRE_BREAK_PLACEMENT) currentGameState = BREAKING;
                    else if (currentGameState == BALL_IN_HAND_P1) currentGameState = PLAYER1_TURN;
                    else if (currentGameState == BALL_IN_HAND_P2) currentGameState = PLAYER2_TURN;
                    cueAngle = 0.0f;
                    /* ----------------------------------------------------
                    If the player who now has the turn is already on the
                    8-ball, immediately switch to pocket-selection state.
                    ---------------------------------------------------- */
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN)
                    {
                        CheckAndTransitionToPocketChoice(currentPlayer);
                    }
                }
                else { /* Stay in BALL_IN_HAND state if final pos invalid */ }
            }
        }

        // Handle releasing english setting
        if (isSettingEnglish) {
            isSettingEnglish = false;
        }
        return 0;
    } // End WM_LBUTTONUP

    case WM_DESTROY:
        isMusicPlaying = false;
        if (midiDeviceID != 0) {
            mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
            midiDeviceID = 0;
            SaveSettings(); // Save settings on exit
        }
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
    isOpeningBreakShot = true; // This is the start of a new game, so the next shot is an opening break.
    aiPlannedShotDetails.isValid = false; // Reset AI planned shot
    aiIsDisplayingAim = false;
    aiAimDisplayFramesLeft = 0;
    // ... (rest of InitGame())

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
        // --- Reset any 8?Ball call state on new game ---
    lastEightBallPocketIndex = -1;
    calledPocketP1 = -1;
    calledPocketP2 = -1;
    pocketCallMessage = L"";
    aiPlannedShotDetails.isValid = false; // THIS IS THE CRITICAL FIX: Reset the AI's plan.

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
    size_t otherBallIdx = 0;
    //int otherBallIdx = 0;
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
        /*// AI Mode: Randomly decide who breaks
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
            aiTurnPending = false;*/
        switch (openingBreakMode) {
        case CPU_BREAK:
            currentPlayer = 2; // AI breaks
            currentGameState = PRE_BREAK_PLACEMENT;
            aiTurnPending = true;
            break;
        case P1_BREAK:
            currentPlayer = 1; // Player 1 breaks
            currentGameState = PRE_BREAK_PLACEMENT;
            aiTurnPending = false;
            break;
        case FLIP_COIN_BREAK:
            if ((rand() % 2) == 0) { // 0 for AI, 1 for Player 1
                currentPlayer = 2; // AI breaks
                currentGameState = PRE_BREAK_PLACEMENT;
                aiTurnPending = true;
            }
            else {
                currentPlayer = 1; // Player 1 breaks
                currentGameState = PRE_BREAK_PLACEMENT;
                aiTurnPending = false;
            }
            break;
        default: // Fallback to CPU break
            currentPlayer = 2;
            currentGameState = PRE_BREAK_PLACEMENT;
            aiTurnPending = true;
            break;
        }
    }
    else {
        // Human vs Human, Player 1 always breaks (or could add a flip coin for HvsH too if desired)
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


// --------------------------------------------------------------------------------
// Full GameUpdate(): integrates AI call?pocket ? aim ? shoot (no omissions)
// --------------------------------------------------------------------------------
void GameUpdate() {
    // --- 1) Handle an in?flight shot ---
    if (currentGameState == SHOT_IN_PROGRESS) {
        UpdatePhysics();
        // ? clear old 8?ball pocket info before any new pocket checks
        //lastEightBallPocketIndex = -1;
        CheckCollisions();
        CheckPockets(); // FIX: This line was missing. It's essential to check for pocketed balls every frame.

        if (AreBallsMoving()) {
            isAiming = false;
            aiIsDisplayingAim = false;
        }

        if (!AreBallsMoving()) {
            ProcessShotResults();
        }
        return;
    }

    // --- 2) CPU’s turn (table is static) ---
    if (isPlayer2AI && currentPlayer == 2 && !AreBallsMoving()) {
        // ??? If we've just auto?entered AI_THINKING for the 8?ball call, actually make the decision ???
        if (currentGameState == AI_THINKING && aiTurnPending) {
            aiTurnPending = false;        // consume the pending flag
            AIMakeDecision();             // CPU calls its pocket or plans its shot
            return;                       // done this tick
        }

        // ??? Automate the AI pocket?selection click ???
        if (currentGameState == CHOOSING_POCKET_P2) {
            // AI immediately confirms its call and moves to thinking/shooting
            currentGameState = AI_THINKING;
            aiTurnPending = true;
            return; // process on next tick
        }
        // 2A) If AI is displaying its aim line, count down then shoot
        if (aiIsDisplayingAim) {
            aiAimDisplayFramesLeft--;
            if (aiAimDisplayFramesLeft <= 0) {
                aiIsDisplayingAim = false;
                if (aiPlannedShotDetails.isValid) {
                    firstHitBallIdThisShot = -1;
                    cueHitObjectBallThisShot = false;
                    railHitAfterContact = false;
                    std::thread([](const TCHAR* soundName) {
                        PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT);
                        }, TEXT("cue.wav")).detach();

                        ApplyShot(
                            aiPlannedShotDetails.power,
                            aiPlannedShotDetails.angle,
                            aiPlannedShotDetails.spinX,
                            aiPlannedShotDetails.spinY
                        );
                        aiPlannedShotDetails.isValid = false;
                }
                currentGameState = SHOT_IN_PROGRESS;
                foulCommitted = false;
                pocketedThisTurn.clear();
            }
            return;
        }

        // 2B) Immediately after calling pocket, transition into AI_THINKING
        if (currentGameState == CHOOSING_POCKET_P2 && aiTurnPending) {
            // Start thinking/shooting right away—no human click required
            currentGameState = AI_THINKING;
            aiTurnPending = false;
            AIMakeDecision();
            return;
        }

        // 2C) If AI has pending actions (break, ball?in?hand, or normal turn)
        if (aiTurnPending) {
            if (currentGameState == BALL_IN_HAND_P2) {
                AIPlaceCueBall();
                currentGameState = AI_THINKING;
                aiTurnPending = false;
                AIMakeDecision();
            }
            else if (isOpeningBreakShot && currentGameState == PRE_BREAK_PLACEMENT) {
                AIBreakShot();
            }
            else if (currentGameState == PLAYER2_TURN || currentGameState == BREAKING) {
                currentGameState = AI_THINKING;
                aiTurnPending = false;
                AIMakeDecision();
            }
            return;
        }
    }
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

            /* -----------------------------------------------------------------
   Additional clamp to guarantee the ball never escapes the table.
   The existing wall–collision code can momentarily disable the
   reflection test while the ball is close to a pocket mouth;
   that rare case allowed it to ‘slide’ through the cushion and
   leave the board.  We therefore enforce a final boundary check
   after the normal physics step.
   ----------------------------------------------------------------- */
            const float leftBound = TABLE_LEFT + BALL_RADIUS;
            const float rightBound = TABLE_RIGHT - BALL_RADIUS;
            const float topBound = TABLE_TOP + BALL_RADIUS;
            const float bottomBound = TABLE_BOTTOM - BALL_RADIUS;

            if (b.x < leftBound) { b.x = leftBound;   b.vx = fabsf(b.vx); }
            if (b.x > rightBound) { b.x = rightBound;  b.vx = -fabsf(b.vx); }
            if (b.y < topBound) { b.y = topBound;    b.vy = fabsf(b.vy); }
            if (b.y > bottomBound) { b.y = bottomBound; b.vy = -fabsf(b.vy); }
        }
    }
}

void CheckCollisions() {
    float left = TABLE_LEFT;
    float right = TABLE_RIGHT;
    float top = TABLE_TOP;
    float bottom = TABLE_BOTTOM;
    const float pocketMouthCheckRadiusSq = (POCKET_RADIUS + BALL_RADIUS) * (POCKET_RADIUS + BALL_RADIUS) * 1.1f;

    // --- Reset Per-Frame Sound Flags ---
    bool playedWallSoundThisFrame = false;
    bool playedCollideSoundThisFrame = false;
    // ---

    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& b1 = balls[i];
        if (b1.isPocketed) continue;

        bool nearPocket[6];
        for (int p = 0; p < 6; ++p) {
            nearPocket[p] = GetDistanceSq(b1.x, b1.y, pocketPositions[p].x, pocketPositions[p].y) < pocketMouthCheckRadiusSq;
        }
        bool nearTopLeftPocket = nearPocket[0];
        bool nearTopMidPocket = nearPocket[1];
        bool nearTopRightPocket = nearPocket[2];
        bool nearBottomLeftPocket = nearPocket[3];
        bool nearBottomMidPocket = nearPocket[4];
        bool nearBottomRightPocket = nearPocket[5];

        bool collidedWallThisBall = false;

        // --- Ball-Wall Collisions ---
        // (Check logic unchanged, added sound calls and railHitAfterContact update)
        // Left Wall
        if (b1.x - BALL_RADIUS < left) {
            if (!nearTopLeftPocket && !nearBottomLeftPocket) {
                b1.x = left + BALL_RADIUS; b1.vx *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true; // Track rail hit after contact
            }
        }
        // Right Wall
        if (b1.x + BALL_RADIUS > right) {
            if (!nearTopRightPocket && !nearBottomRightPocket) {
                b1.x = right - BALL_RADIUS; b1.vx *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true; // Track rail hit after contact
            }
        }
        // Top Wall
        if (b1.y - BALL_RADIUS < top) {
            if (!nearTopLeftPocket && !nearTopMidPocket && !nearTopRightPocket) {
                b1.y = top + BALL_RADIUS; b1.vy *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true; // Track rail hit after contact
            }
        }
        // Bottom Wall
        if (b1.y + BALL_RADIUS > bottom) {
            if (!nearBottomLeftPocket && !nearBottomMidPocket && !nearBottomRightPocket) {
                b1.y = bottom - BALL_RADIUS; b1.vy *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true; // Track rail hit after contact
            }
        }

        // Spin effect (Unchanged)
        if (collidedWallThisBall) {
            if (b1.x <= left + BALL_RADIUS || b1.x >= right - BALL_RADIUS) { b1.vy += cueSpinX * b1.vx * 0.05f; }
            if (b1.y <= top + BALL_RADIUS || b1.y >= bottom - BALL_RADIUS) { b1.vx -= cueSpinY * b1.vy * 0.05f; }
            cueSpinX *= 0.7f; cueSpinY *= 0.7f;
        }


        // --- Ball-Ball Collisions ---
        for (size_t j = i + 1; j < balls.size(); ++j) {
            Ball& b2 = balls[j];
            if (b2.isPocketed) continue;

            float dx = b2.x - b1.x; float dy = b2.y - b1.y;
            float distSq = dx * dx + dy * dy;
            float minDist = BALL_RADIUS * 2.0f;

            if (distSq > 1e-6 && distSq < minDist * minDist) {
                float dist = sqrtf(distSq);
                float overlap = minDist - dist;
                float nx = dx / dist; float ny = dy / dist;

                // Separation (Unchanged)
                b1.x -= overlap * 0.5f * nx; b1.y -= overlap * 0.5f * ny;
                b2.x += overlap * 0.5f * nx; b2.y += overlap * 0.5f * ny;

                float rvx = b1.vx - b2.vx; float rvy = b1.vy - b2.vy;
                float velAlongNormal = rvx * nx + rvy * ny;

                if (velAlongNormal > 0) { // Colliding
                    // --- Play Ball Collision Sound ---
                    if (!playedCollideSoundThisFrame) {
                        std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("poolballhit.wav")).detach();
                        playedCollideSoundThisFrame = true; // Set flag
                    }
                    // --- End Sound ---

                    // --- NEW: Track First Hit and Cue/Object Collision ---
                    if (firstHitBallIdThisShot == -1) { // If first hit hasn't been recorded yet
                        if (b1.id == 0) { // Cue ball hit b2 first
                            firstHitBallIdThisShot = b2.id;
                            cueHitObjectBallThisShot = true;
                        }
                        else if (b2.id == 0) { // Cue ball hit b1 first
                            firstHitBallIdThisShot = b1.id;
                            cueHitObjectBallThisShot = true;
                        }
                        // If neither is cue ball, doesn't count as first hit for foul purposes
                    }
                    else if (b1.id == 0 || b2.id == 0) {
                        // Track subsequent cue ball collisions with object balls
                        cueHitObjectBallThisShot = true;
                    }
                    // --- End First Hit Tracking ---


                    // Impulse (Unchanged)
                    float impulse = velAlongNormal;
                    b1.vx -= impulse * nx; b1.vy -= impulse * ny;
                    b2.vx += impulse * nx; b2.vy += impulse * ny;

                    // Spin Transfer (Unchanged)
                    if (b1.id == 0 || b2.id == 0) {
                        float spinEffectFactor = 0.08f;
                        b1.vx += (cueSpinY * ny - cueSpinX * nx) * spinEffectFactor;
                        b1.vy += (cueSpinY * nx + cueSpinX * ny) * spinEffectFactor;
                        b2.vx -= (cueSpinY * ny - cueSpinX * nx) * spinEffectFactor;
                        b2.vy -= (cueSpinY * nx + cueSpinX * ny) * spinEffectFactor;
                        cueSpinX *= 0.85f; cueSpinY *= 0.85f;
                    }
                }
            }
        } // End ball-ball loop
    } // End ball loop
} // End CheckCollisions


bool CheckPockets() {
    bool anyPocketed = false;
    // For each ball not already pocketed:
    for (auto& b : balls) {
        if (b.isPocketed)
            continue;

        // Check against each pocket
        for (int p = 0; p < 6; ++p) {
            float dx = b.x - pocketPositions[p].x;
            float dy = b.y - pocketPositions[p].y;
            if (dx * dx + dy * dy <= POCKET_RADIUS * POCKET_RADIUS) {
                // It's in the pocket—remove it from play
                // If it's the 8?ball, remember which pocket it went into
                if (b.id == 8) {
                    lastEightBallPocketIndex = p;   // <-- Must set here!
                }
                b.isPocketed = true;
                b.vx = b.vy = 0.0f;           // kill any movement
                pocketedThisTurn.push_back(b.id);
                anyPocketed = true;
                break;  // no need to check other pockets for this ball
            }
        }
    }
    return anyPocketed;
}

bool AreBallsMoving() {
    for (size_t i = 0; i < balls.size(); ++i) {
        if (!balls[i].isPocketed && (balls[i].vx != 0 || balls[i].vy != 0)) {
            return true;
        }
    }
    return false;
}

//Gemini New Fix Version #99
void RespawnCueBall(bool behindHeadstring) {
    Ball* cueBall = GetCueBall();
    if (cueBall) {
        // Determine the initial target position
        float targetX, targetY;
        if (behindHeadstring) {
            targetX = TABLE_LEFT + (HEADSTRING_X - TABLE_LEFT) * 0.5f;
            targetY = TABLE_TOP + TABLE_HEIGHT / 2.0f;
        }
        else {
            targetX = TABLE_LEFT + TABLE_WIDTH / 2.0f;
            targetY = TABLE_TOP + TABLE_HEIGHT / 2.0f;
        }

        // FOOLPROOF FIX: Check if the target spot is valid. If not, nudge it until it is.
        int attempts = 0;
        while (!IsValidCueBallPosition(targetX, targetY, behindHeadstring) && attempts < 100) {
            // If the spot is occupied, try nudging the ball slightly.
            targetX += (static_cast<float>(rand() % 100 - 50) / 50.0f) * BALL_RADIUS;
            targetY += (static_cast<float>(rand() % 100 - 50) / 50.0f) * BALL_RADIUS;
            // Clamp to stay within reasonable bounds
            targetX = std::max(TABLE_LEFT + BALL_RADIUS, std::min(targetX, TABLE_RIGHT - BALL_RADIUS));
            targetY = std::max(TABLE_TOP + BALL_RADIUS, std::min(targetY, TABLE_BOTTOM - BALL_RADIUS));
            attempts++;
        }

        // Set the final, valid position.
        cueBall->x = targetX;
        cueBall->y = targetY;
        cueBall->vx = 0;
        cueBall->vy = 0;
        cueBall->isPocketed = false;

        // Set the correct game state for ball-in-hand.
        if (currentPlayer == 1) {
            currentGameState = BALL_IN_HAND_P1;
            aiTurnPending = false;
        }
        else {
            currentGameState = BALL_IN_HAND_P2;
            if (isPlayer2AI) {
                aiTurnPending = true;
            }
        }
    }
}


// --- Game Logic ---

void ApplyShot(float power, float angle, float spinX, float spinY) {
    Ball* cueBall = GetCueBall();
    if (cueBall) {

        // --- Play Cue Strike Sound (Threaded) ---
        if (power > 0.1f) { // Only play if it's an audible shot
            std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
        }
        // --- End Sound ---

        cueBall->vx = cosf(angle) * power;
        cueBall->vy = sinf(angle) * power;

        // Apply English (Spin) - Simplified effect (Unchanged)
        cueBall->vx += sinf(angle) * spinY * 0.5f;
        cueBall->vy -= cosf(angle) * spinY * 0.5f;
        cueBall->vx -= cosf(angle) * spinX * 0.5f;
        cueBall->vy -= sinf(angle) * spinX * 0.5f;

        // Store spin (Unchanged)
        cueSpinX = spinX;
        cueSpinY = spinY;

        // --- Reset Foul Tracking flags for the new shot ---
        // (Also reset in LBUTTONUP, but good to ensure here too)
        firstHitBallIdThisShot = -1;      // No ball hit yet
        cueHitObjectBallThisShot = false; // Cue hasn't hit anything yet
        railHitAfterContact = false;     // No rail hit after contact yet
        // --- End Reset ---

                // If this was the opening break shot, clear the flag
        if (isOpeningBreakShot) {
            isOpeningBreakShot = false; // Mark opening break as taken
        }
    }
}


// ---------------------------------------------------------------------
//  ProcessShotResults()
// ---------------------------------------------------------------------
void ProcessShotResults() {
    bool cueBallPocketed = false;
    bool eightBallPocketed = false;
    bool playerContinuesTurn = false;

    // --- Step 1: Update Ball Counts FIRST (THE CRITICAL FIX) ---
    // We must update the score before any other game logic runs.
    PlayerInfo& shootingPlayer = (currentPlayer == 1) ? player1Info : player2Info;
    int ownBallsPocketedThisTurn = 0;

    for (int id : pocketedThisTurn) {
        Ball* b = GetBallById(id);
        if (!b) continue;

        if (b->id == 0) {
            cueBallPocketed = true;
        }
        else if (b->id == 8) {
            eightBallPocketed = true;
        }
        else {
            // This is a numbered ball. Update the pocketed count for the correct player.
            if (b->type == player1Info.assignedType && player1Info.assignedType != BallType::NONE) {
                player1Info.ballsPocketedCount++;
            }
            else if (b->type == player2Info.assignedType && player2Info.assignedType != BallType::NONE) {
                player2Info.ballsPocketedCount++;
            }

            if (b->type == shootingPlayer.assignedType) {
                ownBallsPocketedThisTurn++;
            }
        }
    }

    if (ownBallsPocketedThisTurn > 0) {
        playerContinuesTurn = true;
    }

    // --- Step 2: Handle Game-Ending 8-Ball Shot ---
    // Now that the score is updated, this check will have the correct information.
    if (eightBallPocketed) {
        CheckGameOverConditions(true, cueBallPocketed);
        if (currentGameState == GAME_OVER) {
            pocketedThisTurn.clear();
            return;
        }
    }

    // --- Step 3: Check for Fouls ---
    bool turnFoul = false;
    if (cueBallPocketed) {
        turnFoul = true;
    }
    else {
        Ball* firstHit = GetBallById(firstHitBallIdThisShot);
        if (!firstHit) { // Rule: Hitting nothing is a foul.
            turnFoul = true;
        }
        else { // Rule: Hitting the wrong ball type is a foul.
            if (player1Info.assignedType != BallType::NONE) { // Colors are assigned.
                // We check if the player WAS on the 8-ball BEFORE this shot.
                bool wasOnEightBall = (shootingPlayer.assignedType != BallType::NONE && (shootingPlayer.ballsPocketedCount - ownBallsPocketedThisTurn) >= 7);
                if (wasOnEightBall) {
                    if (firstHit->id != 8) turnFoul = true;
                }
                else {
                    if (firstHit->type != shootingPlayer.assignedType) turnFoul = true;
                }
            }
        }
    } //reenable below disabled for debugging
    //if (!turnFoul && cueHitObjectBallThisShot && !railHitAfterContact && pocketedThisTurn.empty()) {
        //turnFoul = true;
    //}
    foulCommitted = turnFoul;

    // --- Step 4: Final State Transition ---
    if (foulCommitted) {
        SwitchTurns();
        RespawnCueBall(false);
    }
    else if (player1Info.assignedType == BallType::NONE && !pocketedThisTurn.empty() && !cueBallPocketed) {
        // Assign types on the break.
        for (int id : pocketedThisTurn) {
            Ball* b = GetBallById(id);
            if (b && b->type != BallType::EIGHT_BALL) {
                AssignPlayerBallTypes(b->type);
                break;
            }
        }
        CheckAndTransitionToPocketChoice(currentPlayer);
    }
    else if (playerContinuesTurn) {
        // The player's turn continues. Now the check will work correctly.
        CheckAndTransitionToPocketChoice(currentPlayer);
    }
    else {
        SwitchTurns();
    }

    pocketedThisTurn.clear();
}

/*
// --- Step 3: Final State Transition ---
if (foulCommitted) {
    SwitchTurns();
    RespawnCueBall(false);
}
else if (playerContinuesTurn) {
    CheckAndTransitionToPocketChoice(currentPlayer);
}
else {
    SwitchTurns();
}

pocketedThisTurn.clear();
} */

//  Assign groups AND optionally give the shooter his first count.
bool AssignPlayerBallTypes(BallType firstPocketedType, bool creditShooter /*= true*/)
{
    if (firstPocketedType != SOLID && firstPocketedType != STRIPE)
        return false;                                 // safety

    /* ---------------------------------------------------------
       1.  Decide the groups
    --------------------------------------------------------- */
    if (currentPlayer == 1)
    {
        player1Info.assignedType = firstPocketedType;
        player2Info.assignedType =
            (firstPocketedType == SOLID) ? STRIPE : SOLID;
    }
    else
    {
        player2Info.assignedType = firstPocketedType;
        player1Info.assignedType =
            (firstPocketedType == SOLID) ? STRIPE : SOLID;
    }

    /* ---------------------------------------------------------
       2.  Count the very ball that made the assignment
    --------------------------------------------------------- */
    if (creditShooter)
    {
        if (currentPlayer == 1)
            ++player1Info.ballsPocketedCount;
        else
            ++player2Info.ballsPocketedCount;
    }
    return true;
}

/*bool AssignPlayerBallTypes(BallType firstPocketedType) {
    if (firstPocketedType == BallType::SOLID || firstPocketedType == BallType::STRIPE) {
        if (currentPlayer == 1) {
            player1Info.assignedType = firstPocketedType;
            player2Info.assignedType = (firstPocketedType == BallType::SOLID) ? BallType::STRIPE : BallType::SOLID;
        }
        else {
            player2Info.assignedType = firstPocketedType;
            player1Info.assignedType = (firstPocketedType == BallType::SOLID) ? BallType::STRIPE : BallType::SOLID;
        }
        return true; // Assignment was successful
    }
    return false; // No assignment made (e.g., 8-ball was pocketed on break)
}*/
// If 8-ball was first (illegal on break generally), rules vary.
// Here, we might ignore assignment until a solid/stripe is pocketed legally.
// Or assign based on what *else* was pocketed, if anything.
// Simplification: Assignment only happens on SOLID or STRIPE first pocket.


// --- Called in ProcessShotResults() after pocket detection ---
void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed)
{
    // Only care if the 8?ball really went in:
    if (!eightBallPocketed) return;

    // Who’s shooting now?
    PlayerInfo& shooter = (currentPlayer == 1) ? player1Info : player2Info;
    PlayerInfo& opponent = (currentPlayer == 1) ? player2Info : player1Info;

    // Which pocket did we CALL?
    int called = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    // Which pocket did it ACTUALLY fall into?
    int actual = lastEightBallPocketIndex;

    // Check legality: must have called a pocket ?0, must match actual,
    // must have pocketed all 7 of your balls first, and must not have scratched.
    bool legal = (called >= 0)
        && (called == actual)
        && (shooter.ballsPocketedCount >= 7)
        && (!cueBallPocketed);

    // Build a message that shows both values for debugging/tracing:
    if (legal) {
        gameOverMessage = shooter.name
            + L" Wins! "
            + L"(Called: " + std::to_wstring(called)
            + L", Actual: " + std::to_wstring(actual) + L")";
    }
    else {
        gameOverMessage = opponent.name
            + L" Wins! (Illegal 8?Ball) "
            + L"(Called: " + std::to_wstring(called)
            + L", Actual: " + std::to_wstring(actual) + L")";
    }

    currentGameState = GAME_OVER;
}



/*void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed) {
    if (!eightBallPocketed) return;

    PlayerInfo& shootingPlayer = (currentPlayer == 1) ? player1Info : player2Info;
    PlayerInfo& opponentPlayer = (currentPlayer == 1) ? player2Info : player1Info;

    // Handle 8-ball on break: re-spot and continue.
    if (player1Info.assignedType == BallType::NONE) {
        Ball* b = GetBallById(8);
        if (b) { b->isPocketed = false; b->x = RACK_POS_X; b->y = RACK_POS_Y; b->vx = b->vy = 0; }
        if (cueBallPocketed) foulCommitted = true;
        return;
    }

    // --- FOOLPROOF WIN/LOSS LOGIC ---
    bool wasOnEightBall = IsPlayerOnEightBall(currentPlayer);
    int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    int actualPocket = -1;

    // Find which pocket the 8-ball actually went into.
    for (int id : pocketedThisTurn) {
        if (id == 8) {
            Ball* b = GetBallById(8); // This ball is already marked as pocketed, but we need its last coords.
            if (b) {
                for (int p_idx = 0; p_idx < 6; ++p_idx) {
                    // Check last known position against pocket centers
                    if (GetDistanceSq(b->x, b->y, pocketPositions[p_idx].x, pocketPositions[p_idx].y) < POCKET_RADIUS * POCKET_RADIUS * 1.5f) {
                        actualPocket = p_idx;
                        break;
                    }
                }
            }
            break;
        }
    }

    // Evaluate win/loss based on a clear hierarchy of rules.
    if (!wasOnEightBall) {
        gameOverMessage = opponentPlayer.name + L" Wins! (8-Ball Pocketed Early)";
    }
    else if (cueBallPocketed) {
        gameOverMessage = opponentPlayer.name + L" Wins! (Scratched on 8-Ball)";
    }
    else if (calledPocket == -1) {
        gameOverMessage = opponentPlayer.name + L" Wins! (Pocket Not Called)";
    }
    else if (actualPocket != calledPocket) {
        gameOverMessage = opponentPlayer.name + L" Wins! (8-Ball in Wrong Pocket)";
    }
    else {
        // WIN! All loss conditions failed, this must be a legal win.
        gameOverMessage = shootingPlayer.name + L" Wins!";
    }

    currentGameState = GAME_OVER;
}*/

/*void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed)
{
    if (!eightBallPocketed) return;

    PlayerInfo& shooter = (currentPlayer == 1) ? player1Info : player2Info;
    PlayerInfo& opponent = (currentPlayer == 1) ? player2Info : player1Info;
    // Which pocket did we call?
    int called = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    // Which pocket did the ball really fall into?
    int actual = lastEightBallPocketIndex;

    // Legal victory only if:
    //  1) Shooter had already pocketed 7 of their object balls,
    //  2) They called a pocket,
    //  3) The 8?ball actually fell into that same pocket,
    //  4) They did not scratch on the 8?ball.
    bool legal =
        (shooter.ballsPocketedCount >= 7) &&
        (called >= 0) &&
        (called == actual) &&
        (!cueBallPocketed);

    if (legal) {
        gameOverMessage = shooter.name + L" Wins! "
            L"(called: " + std::to_wstring(called) +
            L", actual: " + std::to_wstring(actual) + L")";
    }
    else {
        gameOverMessage = opponent.name + L" Wins! (illegal 8-ball) "
        // For debugging you can append:
        + L" (called: " + std::to_wstring(called)
        + L", actual: " + std::to_wstring(actual) + L")";
    }

    currentGameState = GAME_OVER;
}*/

// ????????????????????????????????????????????????????????????????
//  CheckGameOverConditions()
//     – Called when the 8-ball has fallen.
//     – Decides who wins and builds the gameOverMessage.
// ????????????????????????????????????????????????????????????????
/*void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed)
{
    if (!eightBallPocketed) return;                     // safety

    PlayerInfo& shooter = (currentPlayer == 1) ? player1Info : player2Info;
    PlayerInfo& opponent = (currentPlayer == 1) ? player2Info : player1Info;

    int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    int actualPocket = lastEightBallPocketIndex;

    bool clearedSeven = (shooter.ballsPocketedCount >= 7);
    bool noScratch = !cueBallPocketed;
    bool callMade = (calledPocket >= 0);

    // helper ? turn “-1” into "None" for readability
    auto pocketToStr = [](int idx) -> std::wstring
    {
        return (idx >= 0) ? std::to_wstring(idx) : L"None";
    };

    if (clearedSeven && noScratch && callMade && actualPocket == calledPocket)
    {
        // legitimate win
        gameOverMessage =
            shooter.name +
            L" Wins! (Called pocket: " + pocketToStr(calledPocket) +
            L", Actual pocket: " + pocketToStr(actualPocket) + L")";
    }
    else
    {
        // wrong pocket, scratch, or early 8-ball
        gameOverMessage =
            opponent.name +
            L" Wins! (Called pocket: " + pocketToStr(calledPocket) +
            L", Actual pocket: " + pocketToStr(actualPocket) + L")";
    }

    currentGameState = GAME_OVER;
}*/

/* void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed) {
    if (!eightBallPocketed) return; // Only when 8-ball actually pocketed

    PlayerInfo& shooter = (currentPlayer == 1) ? player1Info : player2Info;
    PlayerInfo& opponent = (currentPlayer == 1) ? player2Info : player1Info;
    bool      onEightRoll = IsPlayerOnEightBall(currentPlayer);
    int       calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    int       actualPocket = -1;
    Ball* bEight = GetBallById(8);

    // locate which hole the 8-ball went into
    if (bEight) {
        for (int i = 0; i < 6; ++i) {
            if (GetDistanceSq(bEight->x, bEight->y,
                pocketPositions[i].x, pocketPositions[i].y)
                < POCKET_RADIUS * POCKET_RADIUS * 1.5f) {
                actualPocket = i; break;
            }
        }
    }

    // 1) On break / pre-assignment: re-spot & continue
    if (player1Info.assignedType == BallType::NONE) {
        if (bEight) {
            bEight->isPocketed = false;
            bEight->x = RACK_POS_X; bEight->y = RACK_POS_Y;
            bEight->vx = bEight->vy = 0;
        }
        if (cueBallPocketed) foulCommitted = true;
        return;
    }

    // 2) Loss if pocketed 8 early
    if (!onEightRoll) {
        gameOverMessage = opponent.name + L" Wins! (" + shooter.name + L" pocketed 8-ball early)";
    }
    // 3) Loss if scratched
    else if (cueBallPocketed) {
        gameOverMessage = opponent.name + L" Wins! (" + shooter.name + L" scratched on 8-ball)";
    }
    // 4) Loss if no pocket call
    else if (calledPocket < 0) {
        gameOverMessage = opponent.name + L" Wins! (" + shooter.name + L" did not call a pocket)";
    }
    // 5) Loss if in wrong pocket
    else if (actualPocket != calledPocket) {
        gameOverMessage = opponent.name + L" Wins! (" + shooter.name + L" 8-ball in wrong pocket)";
    }
    // 6) Otherwise, valid win
    else {
        gameOverMessage = shooter.name + L" Wins!";
    }

    currentGameState = GAME_OVER;
} */


// Switch the shooter, handle fouls and decide what state we go to next.
// ────────────────────────────────────────────────────────────────
//  SwitchTurns – final version (arrow–leak bug fixed)
// ────────────────────────────────────────────────────────────────
void SwitchTurns()
{
    /* --------------------------------------------------------- */
    /* 1.  Hand the table over to the other player               */
    /* --------------------------------------------------------- */
    currentPlayer = (currentPlayer == 1) ? 2 : 1;

    /* --------------------------------------------------------- */
    /* 2.  Generic per–turn resets                               */
    /* --------------------------------------------------------- */
    isAiming = false;
    shotPower = 0.0f;
    currentlyHoveredPocket = -1;

    /* --------------------------------------------------------- */
    /* 3.  Wipe every previous pocket call                       */
    /*    (the new shooter will choose again if needed)          */
    /* --------------------------------------------------------- */
    calledPocketP1 = -1;
    calledPocketP2 = -1;
    pocketCallMessage.clear();

    /* --------------------------------------------------------- */
    /* 4.  Handle fouls — cue-ball in hand overrides everything  */
    /* --------------------------------------------------------- */
    if (foulCommitted)
    {
        if (currentPlayer == 1)            // human
        {
            currentGameState = BALL_IN_HAND_P1;
            aiTurnPending = false;
        }
        else                               // P2
        {
            currentGameState = BALL_IN_HAND_P2;
            aiTurnPending = isPlayer2AI;   // AI will place cue-ball
        }

        foulCommitted = false;
        return;                            // we're done for this frame
    }

    /* --------------------------------------------------------- */
    /* 5.  Normal flow                                           */
    /*    Will put us in  ∘ PLAYER?_TURN                         */
    /*                    ∘ CHOOSING_POCKET_P?                   */
    /*                    ∘ AI_THINKING  (for CPU)               */
    /* --------------------------------------------------------- */
    CheckAndTransitionToPocketChoice(currentPlayer);
}


void AIBreakShot() {
    Ball* cueBall = GetCueBall();
    if (!cueBall) return;

    // This function is called when it's AI's turn for the opening break and state is PRE_BREAK_PLACEMENT.
    // AI will place the cue ball and then plan the shot.
    if (isOpeningBreakShot && currentGameState == PRE_BREAK_PLACEMENT) {
        // Place cue ball in the kitchen randomly
        /*float kitchenMinX = TABLE_LEFT + BALL_RADIUS; // [cite: 1071, 1072, 1587]
        float kitchenMaxX = HEADSTRING_X - BALL_RADIUS; // [cite: 1072, 1078, 1588]
        float kitchenMinY = TABLE_TOP + BALL_RADIUS; // [cite: 1071, 1072, 1588]
        float kitchenMaxY = TABLE_BOTTOM - BALL_RADIUS; // [cite: 1072, 1073, 1589]*/

        // --- AI Places Cue Ball for Break ---
// Decide if placing center or side. For simplicity, let's try placing slightly off-center
// towards one side for a more angled break, or center for direct apex hit.
// A common strategy is to hit the second ball of the rack.

        float placementY = RACK_POS_Y; // Align vertically with the rack center
        float placementX;

        // Randomly choose a side or center-ish placement for variation.
        int placementChoice = rand() % 3; // 0: Left-ish, 1: Center-ish, 2: Right-ish in kitchen

        if (placementChoice == 0) { // Left-ish
            placementX = HEADSTRING_X - (TABLE_WIDTH * 0.05f) - (BALL_RADIUS * (1 + (rand() % 3))); // Place slightly to the left within kitchen
        }
        else if (placementChoice == 2) { // Right-ish
            placementX = HEADSTRING_X - (TABLE_WIDTH * 0.05f) + (BALL_RADIUS * (1 + (rand() % 3))); // Place slightly to the right within kitchen
        }
        else { // Center-ish
            placementX = TABLE_LEFT + (HEADSTRING_X - TABLE_LEFT) * 0.5f; // Roughly center of kitchen
        }
        placementX = std::max(TABLE_LEFT + BALL_RADIUS + 1.0f, std::min(placementX, HEADSTRING_X - BALL_RADIUS - 1.0f)); // Clamp within kitchen X

        bool validPos = false;
        int attempts = 0;
        while (!validPos && attempts < 100) {
            /*cueBall->x = kitchenMinX + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (kitchenMaxX - kitchenMinX)); // [cite: 1589]
            cueBall->y = kitchenMinY + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (kitchenMaxY - kitchenMinY)); // [cite: 1590]
            if (IsValidCueBallPosition(cueBall->x, cueBall->y, true)) { // [cite: 1591]
                validPos = true; // [cite: 1591]*/
                // Try the chosen X, but vary Y slightly to find a clear spot
            cueBall->x = placementX;
            cueBall->y = placementY + (static_cast<float>(rand() % 100 - 50) / 100.0f) * BALL_RADIUS * 2.0f; // Vary Y a bit
            cueBall->y = std::max(TABLE_TOP + BALL_RADIUS + 1.0f, std::min(cueBall->y, TABLE_BOTTOM - BALL_RADIUS - 1.0f)); // Clamp Y

            if (IsValidCueBallPosition(cueBall->x, cueBall->y, true /* behind headstring */)) {
                validPos = true;
            }
            attempts++; // [cite: 1592]
        }
        if (!validPos) {
            // Fallback position
            /*cueBall->x = TABLE_LEFT + (HEADSTRING_X - TABLE_LEFT) * 0.5f; // [cite: 1071, 1078, 1593]
            cueBall->y = (TABLE_TOP + TABLE_BOTTOM) * 0.5f; // [cite: 1071, 1073, 1594]
            if (!IsValidCueBallPosition(cueBall->x, cueBall->y, true)) { // [cite: 1594]
                cueBall->x = HEADSTRING_X - BALL_RADIUS * 2; // [cite: 1072, 1078, 1594]
                cueBall->y = RACK_POS_Y; // [cite: 1080, 1595]
            }
        }
        cueBall->vx = 0; // [cite: 1595]
        cueBall->vy = 0; // [cite: 1596]

        // Plan a break shot: aim at the center of the rack (apex ball)
        float targetX = RACK_POS_X; // [cite: 1079] Aim for the apex ball X-coordinate
        float targetY = RACK_POS_Y; // [cite: 1080] Aim for the apex ball Y-coordinate

        float dx = targetX - cueBall->x; // [cite: 1599]
        float dy = targetY - cueBall->y; // [cite: 1600]
        float shotAngle = atan2f(dy, dx); // [cite: 1600]
        float shotPowerValue = MAX_SHOT_POWER; // [cite: 1076, 1600] Use MAX_SHOT_POWER*/

            cueBall->x = TABLE_LEFT + (HEADSTRING_X - TABLE_LEFT) * 0.75f; // A default safe spot in kitchen
            cueBall->y = RACK_POS_Y;
        }
        cueBall->vx = 0; cueBall->vy = 0;

        // --- AI Plans the Break Shot ---
        float targetX, targetY;
        // If cue ball is near center of kitchen width, aim for apex.
        // Otherwise, aim for the second ball on the side the cue ball is on (for a cut break).
        float kitchenCenterRegion = (HEADSTRING_X - TABLE_LEFT) * 0.3f; // Define a "center" region
        if (std::abs(cueBall->x - (TABLE_LEFT + (HEADSTRING_X - TABLE_LEFT) / 2.0f)) < kitchenCenterRegion / 2.0f) {
            // Center-ish placement: Aim for the apex ball (ball ID 1 or first ball in rack)
            targetX = RACK_POS_X; // Apex ball X
            targetY = RACK_POS_Y; // Apex ball Y
        }
        else {
            // Side placement: Aim to hit the "second" ball of the rack for a wider spread.
            // This is a simplification. A more robust way is to find the actual second ball.
            // For now, aim slightly off the apex towards the side the cue ball is on.
            targetX = RACK_POS_X + BALL_RADIUS * 2.0f * 0.866f; // X of the second row of balls
            targetY = RACK_POS_Y + ((cueBall->y > RACK_POS_Y) ? -BALL_RADIUS : BALL_RADIUS); // Aim at the upper or lower of the two second-row balls
        }

        float dx = targetX - cueBall->x;
        float dy = targetY - cueBall->y;
        float shotAngle = atan2f(dy, dx);
        float shotPowerValue = MAX_SHOT_POWER * (0.9f + (rand() % 11) / 100.0f); // Slightly vary max power

        // Store planned shot details for the AI
        /*aiPlannedShotDetails.angle = shotAngle; // [cite: 1102, 1601]
        aiPlannedShotDetails.power = shotPowerValue; // [cite: 1102, 1601]
        aiPlannedShotDetails.spinX = 0.0f; // [cite: 1102, 1601] No spin for a standard power break
        aiPlannedShotDetails.spinY = 0.0f; // [cite: 1103, 1602]
        aiPlannedShotDetails.isValid = true; // [cite: 1103, 1602]*/

        aiPlannedShotDetails.angle = shotAngle;
        aiPlannedShotDetails.power = shotPowerValue;
        aiPlannedShotDetails.spinX = 0.0f; // No spin for break usually
        aiPlannedShotDetails.spinY = 0.0f;
        aiPlannedShotDetails.isValid = true;

        // Update global cue parameters for immediate visual feedback if DrawAimingAids uses them
        /*::cueAngle = aiPlannedShotDetails.angle;      // [cite: 1109, 1603] Update global cueAngle
        ::shotPower = aiPlannedShotDetails.power;     // [cite: 1109, 1604] Update global shotPower
        ::cueSpinX = aiPlannedShotDetails.spinX;    // [cite: 1109]
        ::cueSpinY = aiPlannedShotDetails.spinY;    // [cite: 1110]*/

        ::cueAngle = aiPlannedShotDetails.angle;
        ::shotPower = aiPlannedShotDetails.power;
        ::cueSpinX = aiPlannedShotDetails.spinX;
        ::cueSpinY = aiPlannedShotDetails.spinY;

        // Set up for AI display via GameUpdate
        /*aiIsDisplayingAim = true;                   // [cite: 1104] Enable AI aiming visualization
        aiAimDisplayFramesLeft = AI_AIM_DISPLAY_DURATION_FRAMES; // [cite: 1105] Set duration for display

        currentGameState = AI_THINKING; // [cite: 1081] Transition to AI_THINKING state.
                                        // GameUpdate will handle the aiAimDisplayFramesLeft countdown
                                        // and then execute the shot using aiPlannedShotDetails.
                                        // isOpeningBreakShot will be set to false within ApplyShot.

        // No immediate ApplyShot or sound here; GameUpdate's AI execution logic will handle it.*/

        aiIsDisplayingAim = true;
        aiAimDisplayFramesLeft = AI_AIM_DISPLAY_DURATION_FRAMES;
        currentGameState = AI_THINKING; // State changes to AI_THINKING, GameUpdate will handle shot execution after display
        aiTurnPending = false;

        return; // The break shot is now planned and will be executed by GameUpdate
    }

    // 2. If not in PRE_BREAK_PLACEMENT (e.g., if this function were called at other times,
    //    though current game logic only calls it for PRE_BREAK_PLACEMENT)
    //    This part can be extended if AIBreakShot needs to handle other scenarios.
    //    For now, the primary logic is above.
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

// --- NEW HELPER FUNCTION IMPLEMENTATIONS ---

// Checks if a player has pocketed all their balls and is now on the 8-ball.
bool IsPlayerOnEightBall(int player) {
    PlayerInfo& playerInfo = (player == 1) ? player1Info : player2Info;
    if (playerInfo.assignedType != BallType::NONE && playerInfo.assignedType != BallType::EIGHT_BALL && playerInfo.ballsPocketedCount >= 7) {
        Ball* eightBall = GetBallById(8);
        return (eightBall && !eightBall->isPocketed);
    }
    return false;
}

// Centralized logic to enter the "choosing pocket" state. This fixes the indicator bugs.
void CheckAndTransitionToPocketChoice(int playerID) {
    // 1) Correct “on-8-ball” test
    PlayerInfo& info = (playerID == 1) ? player1Info : player2Info;
    bool needsToCall = (info.ballsPocketedCount >= 7);   // <- must be 7!
    //    bool needsToCall = IsPlayerOnEightBall(playerID);

    if (needsToCall) {
        if (playerID == 1) { // Human Player 1
            currentGameState = CHOOSING_POCKET_P1;
            pocketCallMessage = player1Info.name + L": Choose a pocket for the 8-Ball...";
            if (calledPocketP1 == -1) calledPocketP1 = 2; // Default to bottom-right
        }
        else { // Player 2
            if (isPlayer2AI) {
                // FOOLPROOF FIX: AI doesn't choose here. It transitions to a thinking state.
                // AIMakeDecision will handle the choice and the pocket call.
                currentGameState = AI_THINKING;
                aiTurnPending = true; // Signal the main loop to run AIMakeDecision
            }
            else { // Human Player 2
                currentGameState = CHOOSING_POCKET_P2;
                pocketCallMessage = player2Info.name + L": Choose a pocket for the 8-Ball...";
                if (calledPocketP2 == -1) calledPocketP2 = 2; // Default to bottom-right
            }
        }
    }
    else {
        pocketCallMessage = L"";

        // --- NEW: wipe any previous pocket call -----------------
        if (playerID == 1)
            calledPocketP1 = -1;
        else
            calledPocketP2 = -1;
        // ---------------------------------------------------------

        currentGameState = (playerID == 1) ? PLAYER1_TURN : PLAYER2_TURN;

        // let CPU start thinking during its normal turn
        if (playerID == 2 && isPlayer2AI)
            aiTurnPending = true;
        /*// Player does not need to call a pocket, proceed to normal turn.
        pocketCallMessage = L"";
        currentGameState = (playerID == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (playerID == 2 && isPlayer2AI) {
            aiTurnPending = true;
        }*/
    }
}

template <typename T>
void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

// --- CPU Ball?in?Hand Placement --------------------------------
// Moves the cue ball to a legal "ball in hand" position for the AI.
void AIPlaceCueBall() {
    Ball* cue = GetCueBall();
    if (!cue) return;

    // Simple strategy: place back behind the headstring at the standard break spot
    cue->x = TABLE_LEFT + TABLE_WIDTH * 0.15f;
    cue->y = RACK_POS_Y;
    cue->vx = cue->vy = 0.0f;
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

// --- INSERT NEW HELPER FUNCTION HERE ---
// Calculates the squared distance from point P to the line segment AB.
float PointToLineSegmentDistanceSq(D2D1_POINT_2F p, D2D1_POINT_2F a, D2D1_POINT_2F b) {
    float l2 = GetDistanceSq(a.x, a.y, b.x, b.y);
    if (l2 == 0.0f) return GetDistanceSq(p.x, p.y, a.x, a.y); // Segment is a point
    // Consider P projecting onto the line AB infinite line
    // t = [(P-A) . (B-A)] / |B-A|^2
    float t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2;
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp t to the segment [0, 1]
    // Projection falls on the segment
    D2D1_POINT_2F projection = D2D1::Point2F(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y));
    return GetDistanceSq(p.x, p.y, projection.x, projection.y);
}
// --- End New Helper ---

// --- NEW AI Implementation Functions ---

// --------------------------------------------------------------------------------
// Full AIMakeDecision(): CPU pocket?calling & shot planning (no omissions)
// --------------------------------------------------------------------------------
// ────────────────────────────────────────────────────────────────
//   3.  High-level brain – AIMakeDecision()
// ────────────────────────────────────────────────────────────────
void AIMakeDecision()
{
    /* Reset any previous plan / arrow */
    aiPlannedShotDetails.isValid = false;
    calledPocketP2 = -1;

    AIShotInfo best = AIFindBestShot();

    if (!best.possible)
    {
        // Should not happen because AIFindBestShot always returns a fallback
        aiPlannedShotDetails.isValid = false;
        return;
    }    

    /* Pocket call only when CPU really is on the 8-ball */
    if (IsPlayerOnEightBall(2) && best.pocketIndex >= 0)
        calledPocketP2 = best.pocketIndex;
    else
        calledPocketP2 = -1;

    /* Transfer parameters */
    aiPlannedShotDetails.angle = best.angle;
    aiPlannedShotDetails.power = best.power;
    aiPlannedShotDetails.spinX = best.spinX;
    aiPlannedShotDetails.spinY = best.spinY;
    aiPlannedShotDetails.isValid = true;

    // --- FOOLPROOF FIX: Trigger the Aim Display ---
    // If any valid plan was made, update the visuals and start the display pause.
    if (aiPlannedShotDetails.isValid) {

        // STEP 1: Copy the AI's plan into the global variables used for drawing.
        // This is the critical missing link.
        cueAngle = aiPlannedShotDetails.angle;
        shotPower = aiPlannedShotDetails.power;

        // STEP 2: Trigger the visual display pause.
        // These are the two lines you correctly identified.
        aiIsDisplayingAim = true;
        aiAimDisplayFramesLeft = AI_AIM_DISPLAY_DURATION_FRAMES;

    }

    /* Small pause so humans can see the aim before stroke */
    //aiIsDisplayingAim = true;
    //aiAimDisplayFramesLeft = AI_AIM_DISPLAY_DURATION_FRAMES;
}



AIShotInfo AIFindBestShot()
{
    AIShotInfo best;                       // .possible == false
    Ball* cue = GetCueBall();
    if (!cue) return best;

    const bool on8 = IsPlayerOnEightBall(2);
    const BallType wantType = player2Info.assignedType;

    for (Ball& b : balls)
    {
        if (b.isPocketed || b.id == 0) continue;

        // decide if this ball is a legal/interesting target
        bool ok =
            on8 ? (b.id == 8) :
            ((wantType == BallType::NONE) || (b.type == wantType));

        if (!ok) continue;

        for (int p = 0; p < 6; ++p)
        {
            AIShotInfo cand = EvaluateShot(&b, p);
            if (cand.possible &&
                (!best.possible || cand.score > best.score))
                best = cand;
        }
    }

    // fall-back: tap cue ball forward (safety) if no potting line exists
    if (!best.possible && cue)
    {
        best.possible = true;
        best.angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * PI;
        best.power = MAX_SHOT_POWER * 0.30f;
        best.spinX = best.spinY = 0.0f;
        best.targetBall = nullptr;
        best.score = -99999.0f;
        best.pocketIndex = -1;
    }
    return best;
}


// Evaluate a potential shot at a specific target ball towards a specific pocket
AIShotInfo EvaluateShot(Ball* targetBall, int pocketIndex) {
    AIShotInfo shotInfo; // Defaults to not possible
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;
    shotInfo.involves8Ball = (targetBall && targetBall->id == 8);

    Ball* cueBall = GetCueBall();
    if (!cueBall || !targetBall) return shotInfo;

    // 1. Calculate Ghost Ball position (where cue must hit target)
    shotInfo.ghostBallPos = CalculateGhostBallPos(targetBall, pocketIndex);

    // 2. Check Path: Cue Ball -> Ghost Ball Position
    if (!IsPathClear(D2D1::Point2F(cueBall->x, cueBall->y), shotInfo.ghostBallPos, cueBall->id, targetBall->id)) {
        return shotInfo; // Path blocked, shot is impossible.
    }

    // 3. Calculate Angle and Power
    float dx = shotInfo.ghostBallPos.x - cueBall->x;
    float dy = shotInfo.ghostBallPos.y - cueBall->y;
    shotInfo.angle = atan2f(dy, dx);

    float cueToGhostDist = GetDistance(cueBall->x, cueBall->y, shotInfo.ghostBallPos.x, shotInfo.ghostBallPos.y);
    float targetToPocketDist = GetDistance(targetBall->x, targetBall->y, pocketPositions[pocketIndex].x, pocketPositions[pocketIndex].y);
    shotInfo.power = CalculateShotPower(cueToGhostDist, targetToPocketDist);

    // 4. Score the shot (simple scoring: closer and straighter is better)
    shotInfo.score = 1000.0f - (cueToGhostDist + targetToPocketDist);

    // If we reached here, the shot is geometrically possible.
    shotInfo.possible = true;
    return shotInfo;
}


//  Estimate the power that will carry the cue-ball to the ghost position
//  *and* push the object-ball the remaining distance to the pocket.
//
//  • cueToGhostDist    – pixels from cue to ghost-ball centre
//  • targetToPocketDist– pixels from object-ball to chosen pocket
//
//  The function is fully deterministic (good for AI search) yet produces
//  human-looking power levels.
//
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist)
{
    // Total distance the *energy* must cover (cue path + object-ball path)
    float totalDist = cueToGhostDist + targetToPocketDist;

    // Typical diagonal of the playable area (approx.) – used for scaling
    constexpr float TABLE_DIAG = 900.0f;

    // 1.  Convert distance to a 0-1 number (0: tap-in, 1: table length)
    float norm = std::clamp(totalDist / TABLE_DIAG, 0.0f, 1.0f);

    // 2.  Ease-in curve (smoothstep) for nicer progression
    norm = norm * norm * (3.0f - 2.0f * norm);

    // 3.  Blend between a gentle minimum and the absolute maximum
    const float MIN_POWER = MAX_SHOT_POWER * 0.18f;     // just enough to move
    float power = MIN_POWER + norm * (MAX_SHOT_POWER - MIN_POWER);

    // 4.  Safety clamp (also screens out degenerate calls)
    power = std::clamp(power, 0.15f, MAX_SHOT_POWER);

    return power;
}

// ------------------------------------------------------------------
//  Return the ghost-ball centre needed for the target ball to roll
//  straight into the chosen pocket.
// ------------------------------------------------------------------
D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, int pocketIndex)
{
    if (!targetBall) return D2D1::Point2F(0, 0);

    D2D1_POINT_2F P = pocketPositions[pocketIndex];

    float vx = P.x - targetBall->x;
    float vy = P.y - targetBall->y;
    float L = sqrtf(vx * vx + vy * vy);
    if (L < 1.0f) L = 1.0f;                // safety

    vx /= L;   vy /= L;

    return D2D1::Point2F(
        targetBall->x - vx * (BALL_RADIUS * 2.0f),
        targetBall->y - vy * (BALL_RADIUS * 2.0f));
}

// Calculate the position the cue ball needs to hit for the target ball to go towards the pocket
// ────────────────────────────────────────────────────────────────
//   2.  Shot evaluation & search
// ────────────────────────────────────────────────────────────────

//  Calculate ghost-ball position so that cue hits target towards pocket
static inline D2D1_POINT_2F GhostPos(const Ball* tgt, int pocketIdx)
{
    D2D1_POINT_2F P = pocketPositions[pocketIdx];
    float vx = P.x - tgt->x;
    float vy = P.y - tgt->y;
    float L = sqrtf(vx * vx + vy * vy);
    vx /= L;  vy /= L;
    return D2D1::Point2F(tgt->x - vx * (BALL_RADIUS * 2.0f),
        tgt->y - vy * (BALL_RADIUS * 2.0f));
}

//  Heuristic: shorter + straighter + proper group = higher score
static inline float ScoreShot(float cue2Ghost,
    float tgt2Pocket,
    bool  correctGroup,
    bool  involves8)
{
    float base = 2000.0f - (cue2Ghost + tgt2Pocket);   // prefer close shots
    if (!correctGroup)  base -= 400.0f;                  // penalty
    if (involves8)      base += 150.0f;                  // a bit more desirable
    return base;
}

// Checks if line segment is clear of obstructing balls
// ────────────────────────────────────────────────────────────────
//   1.  Low-level helpers – IsPathClear & FindFirstHitBall
// ────────────────────────────────────────────────────────────────

//  Test if the capsule [ start … end ] (radius = BALL_RADIUS)
//  intersects any ball except the ids we want to ignore.
bool IsPathClear(D2D1_POINT_2F start,
    D2D1_POINT_2F end,
    int ignoredBallId1,
    int ignoredBallId2)
{
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-3f) return true;             // degenerate → treat as clear

    for (const Ball& b : balls)
    {
        if (b.isPocketed)      continue;
        if (b.id == ignoredBallId1 ||
            b.id == ignoredBallId2)             continue;

        // project ball centre onto the segment
        float t = ((b.x - start.x) * dx + (b.y - start.y) * dy) / lenSq;
        t = std::clamp(t, 0.0f, 1.0f);

        float cx = start.x + t * dx;
        float cy = start.y + t * dy;

        if (GetDistanceSq(b.x, b.y, cx, cy) < (BALL_RADIUS * BALL_RADIUS))
            return false;                       // blocked
    }
    return true;
}

//  Cast an (infinite) ray and return the first non-pocketed ball hit.
//  `hitDistSq` is distance² from the start point to the collision point.
Ball* FindFirstHitBall(D2D1_POINT_2F start,
    float        angle,
    float& hitDistSq)
{
    Ball* hitBall = nullptr;
    float  bestSq = std::numeric_limits<float>::max();
    float  cosA = cosf(angle);
    float  sinA = sinf(angle);

    for (Ball& b : balls)
    {
        if (b.id == 0 || b.isPocketed) continue;         // ignore cue & sunk balls

        float relX = b.x - start.x;
        float relY = b.y - start.y;
        float proj = relX * cosA + relY * sinA;          // distance along the ray

        if (proj <= 0) continue;                         // behind cue

        // closest approach of the ray to the sphere centre
        float closestX = start.x + proj * cosA;
        float closestY = start.y + proj * sinA;
        float dSq = GetDistanceSq(b.x, b.y, closestX, closestY);

        if (dSq <= BALL_RADIUS * BALL_RADIUS)            // intersection
        {
            float back = sqrtf(BALL_RADIUS * BALL_RADIUS - dSq);
            float collDist = proj - back;                // front surface
            float collSq = collDist * collDist;
            if (collSq < bestSq)
            {
                bestSq = collSq;
                hitBall = &b;
            }
        }
    }
    hitDistSq = bestSq;
    return hitBall;
}

// Basic check for reasonable AI aim angles (optional)
bool IsValidAIAimAngle(float angle) {
    // Placeholder - could check for NaN or infinity if calculations go wrong
    return isfinite(angle);
}

//midi func = start
void PlayMidiInBackground(HWND hwnd, const TCHAR* midiPath) {
    while (isMusicPlaying) {
        MCI_OPEN_PARMS mciOpen = { 0 };
        mciOpen.lpstrDeviceType = TEXT("sequencer");
        mciOpen.lpstrElementName = midiPath;

        if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)&mciOpen) == 0) {
            midiDeviceID = mciOpen.wDeviceID;

            MCI_PLAY_PARMS mciPlay = { 0 };
            mciSendCommand(midiDeviceID, MCI_PLAY, 0, (DWORD_PTR)&mciPlay);

            // Wait for playback to complete
            MCI_STATUS_PARMS mciStatus = { 0 };
            mciStatus.dwItem = MCI_STATUS_MODE;

            do {
                mciSendCommand(midiDeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&mciStatus);
                Sleep(100); // adjust as needed
            } while (mciStatus.dwReturn == MCI_MODE_PLAY && isMusicPlaying);

            mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
            midiDeviceID = 0;
        }
    }
}

void StartMidi(HWND hwnd, const TCHAR* midiPath) {
    if (isMusicPlaying) {
        StopMidi();
    }
    isMusicPlaying = true;
    musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);
}

void StopMidi() {
    if (isMusicPlaying) {
        isMusicPlaying = false;
        if (musicThread.joinable()) musicThread.join();
        if (midiDeviceID != 0) {
            mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
            midiDeviceID = 0;
        }
    }
}

/*void PlayGameMusic(HWND hwnd) {
    // Stop any existing playback
    if (isMusicPlaying) {
        isMusicPlaying = false;
        if (musicThread.joinable()) {
            musicThread.join();
        }
        if (midiDeviceID != 0) {
            mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
            midiDeviceID = 0;
        }
    }

    // Get the path of the executable
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    // Extract the directory path
    TCHAR* lastBackslash = _tcsrchr(exePath, '\\');
    if (lastBackslash != NULL) {
        *(lastBackslash + 1) = '\0';
    }

    // Construct the full path to the MIDI file
    static TCHAR midiPath[MAX_PATH];
    _tcscpy_s(midiPath, MAX_PATH, exePath);
    _tcscat_s(midiPath, MAX_PATH, TEXT("BSQ.MID"));

    // Start the background playback
    isMusicPlaying = true;
    musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);
}*/
//midi func = end

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

    //pRT->Clear(D2D1::ColorF(D2D1::ColorF::LightGray)); // Background color
    // Set background color to #ffffcd (RGB: 255, 255, 205)
    pRT->Clear(D2D1::ColorF(0.3686f, 0.5333f, 0.3882f)); // Clear with light yellow background NEWCOLOR 1.0f, 1.0f, 0.803f => (0.3686f, 0.5333f, 0.3882f)
    //pRT->Clear(D2D1::ColorF(1.0f, 1.0f, 0.803f)); // Clear with light yellow background NEWCOLOR 1.0f, 1.0f, 0.803f => (0.3686f, 0.5333f, 0.3882f)

    DrawTable(pRT, pFactory);
    DrawPocketSelectionIndicator(pRT); // Draw arrow over selected/called pocket
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

void DrawTable(ID2D1RenderTarget* pRT, ID2D1Factory* pFactory) {
    ID2D1SolidColorBrush* pBrush = nullptr;

    // === Draw Full Orange Frame (Table Border) ===
    ID2D1SolidColorBrush* pFrameBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.9157f, 0.6157f, 0.2000f), &pFrameBrush); //NEWCOLOR ::Orange (no brackets) => (0.9157, 0.6157, 0.2000)
    //pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Orange), &pFrameBrush); //NEWCOLOR ::Orange (no brackets) => (0.9157, 0.6157, 0.2000)
    if (pFrameBrush) {
        D2D1_RECT_F outerRect = D2D1::RectF(
            TABLE_LEFT - CUSHION_THICKNESS,
            TABLE_TOP - CUSHION_THICKNESS,
            TABLE_RIGHT + CUSHION_THICKNESS,
            TABLE_BOTTOM + CUSHION_THICKNESS
        );
        pRT->FillRectangle(&outerRect, pFrameBrush);
        SafeRelease(&pFrameBrush);
    }

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
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.4235f, 0.5647f, 0.1765f, 1.0f), &pBrush); // NEWCOLOR ::White => (0.2784, 0.4549, 0.1843)
    //pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.5f), &pBrush); // NEWCOLOR ::White => (0.2784, 0.4549, 0.1843)
    if (!pBrush) return;
    pRT->DrawLine(
        D2D1::Point2F(HEADSTRING_X, TABLE_TOP),
        D2D1::Point2F(HEADSTRING_X, TABLE_BOTTOM),
        pBrush,
        1.0f // Line thickness
    );
    SafeRelease(&pBrush);

    // Draw Semicircle facing West (flat side East)
    // Draw Semicircle facing East (curved side on the East, flat side on the West)
    ID2D1PathGeometry* pGeometry = nullptr;
    HRESULT hr = pFactory->CreatePathGeometry(&pGeometry);
    if (SUCCEEDED(hr) && pGeometry)
    {
        ID2D1GeometrySink* pSink = nullptr;
        hr = pGeometry->Open(&pSink);
        if (SUCCEEDED(hr) && pSink)
        {
            float radius = 60.0f; // Radius for the semicircle
            D2D1_POINT_2F center = D2D1::Point2F(HEADSTRING_X, (TABLE_TOP + TABLE_BOTTOM) / 2.0f);

            // For a semicircle facing East (curved side on the East), use the top and bottom points.
            D2D1_POINT_2F startPoint = D2D1::Point2F(center.x, center.y - radius); // Top point

            pSink->BeginFigure(startPoint, D2D1_FIGURE_BEGIN_HOLLOW);

            D2D1_ARC_SEGMENT arc = {};
            arc.point = D2D1::Point2F(center.x, center.y + radius); // Bottom point
            arc.size = D2D1::SizeF(radius, radius);
            arc.rotationAngle = 0.0f;
            // Use the correct identifier with the extra underscore:
            arc.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
            arc.arcSize = D2D1_ARC_SIZE_SMALL;

            pSink->AddArc(&arc);
            pSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pSink->Close();
            SafeRelease(&pSink);

            ID2D1SolidColorBrush* pArcBrush = nullptr;
            //pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.3f), &pArcBrush);
            pRT->CreateSolidColorBrush(D2D1::ColorF(0.4235f, 0.5647f, 0.1765f, 1.0f), &pArcBrush);
            if (pArcBrush)
            {
                pRT->DrawGeometry(pGeometry, pArcBrush, 1.5f);
                SafeRelease(&pArcBrush);
            }
        }
        SafeRelease(&pGeometry);
    }




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
    //if (currentGameState != PLAYER1_TURN && currentGameState != PLAYER2_TURN &&
        //currentGameState != BREAKING && currentGameState != AIMING)
    //{
        //return;
    //}
        // NEW Condition: Allow drawing if it's a human player's active turn/aiming/breaking,
    // OR if it's AI's turn and it's in AI_THINKING state (calculating) or BREAKING (aiming break).
    bool isHumanInteracting = (!isPlayer2AI || currentPlayer == 1) &&
        (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN ||
            currentGameState == BREAKING || currentGameState == AIMING);
    // AI_THINKING state is when AI calculates shot. AIMakeDecision sets cueAngle/shotPower.
    // Also include BREAKING state if it's AI's turn and isOpeningBreakShot for break aim visualization.
        // NEW Condition: AI is displaying its aim
    bool isAiVisualizingShot = (isPlayer2AI && currentPlayer == 2 &&
        currentGameState == AI_THINKING && aiIsDisplayingAim);

    if (!isHumanInteracting && !(isAiVisualizingShot || (currentGameState == AI_THINKING && aiIsDisplayingAim))) {
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
    // Create a Cyan brush for primary and secondary lines //orig(75.0f / 255.0f, 0.0f, 130.0f / 255.0f);indigoColor
    D2D1::ColorF cyanColor(0.0, 255.0, 255.0, 255.0f);
    ID2D1SolidColorBrush* pCyanBrush = nullptr;
    hr = pRT->CreateSolidColorBrush(cyanColor, &pCyanBrush);
    if (FAILED(hr)) {
        SafeRelease(&pCyanBrush);
        // handle error if needed
    }
    // Create a Purple brush for primary and secondary lines
    D2D1::ColorF purpleColor(255.0f, 0.0f, 255.0f, 255.0f);
    ID2D1SolidColorBrush* pPurpleBrush = nullptr;
    hr = pRT->CreateSolidColorBrush(purpleColor, &pPurpleBrush);
    if (FAILED(hr)) {
        SafeRelease(&pPurpleBrush);
        // handle error if needed
    }

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
    //if (isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) {
        // Show power offset if human is aiming/dragging, or if AI is preparing its shot (AI_THINKING or AI Break)
    if ((isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) || isAiVisualizingShot) { // Use the new condition
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
        //pRT->DrawLine(targetStartPoint, targetProjectionEnd, pBrush, 1.0f);

    //new code start

                // Dual trajectory with edge-aware contact simulation
        D2D1_POINT_2F dir = {
            targetProjectionEnd.x - targetStartPoint.x,
            targetProjectionEnd.y - targetStartPoint.y
        };
        float dirLen = sqrtf(dir.x * dir.x + dir.y * dir.y);
        dir.x /= dirLen;
        dir.y /= dirLen;

        D2D1_POINT_2F perp = { -dir.y, dir.x };

        // Approximate cue ball center by reversing from tip
        D2D1_POINT_2F cueBallCenterForGhostHit = { // Renamed for clarity if you use it elsewhere
            targetStartPoint.x - dir.x * BALL_RADIUS,
            targetStartPoint.y - dir.y * BALL_RADIUS
        };

        // REAL contact-ball center - use your physics object's center:
        // (replace 'objectBallPos' with whatever you actually call it)
        // (targetStartPoint is already hitBall->x, hitBall->y)
        D2D1_POINT_2F contactBallCenter = targetStartPoint; // Corrected: Use the object ball's actual center
        //D2D1_POINT_2F contactBallCenter = D2D1::Point2F(hitBall->x, hitBall->y);

       // The 'offset' calculation below uses 'cueBallCenterForGhostHit' (originally 'cueBallCenter').
       // This will result in 'offset' being 0 because 'cueBallCenterForGhostHit' is defined
       // such that (targetStartPoint - cueBallCenterForGhostHit) is parallel to 'dir',
       // and 'perp' is perpendicular to 'dir'.
       // Consider Change 2 if this 'offset' is not behaving as intended for the secondary line.
        /*float offset = ((targetStartPoint.x - cueBallCenterForGhostHit.x) * perp.x +
            (targetStartPoint.y - cueBallCenterForGhostHit.y) * perp.y);*/
            /*float offset = ((targetStartPoint.x - cueBallCenter.x) * perp.x +
                (targetStartPoint.y - cueBallCenter.y) * perp.y);
            float absOffset = fabsf(offset);
            float side = (offset >= 0 ? 1.0f : -1.0f);*/

            // Use actual cue ball center for offset calculation if 'offset' is meant to quantify the cut
        D2D1_POINT_2F actualCueBallPhysicalCenter = D2D1::Point2F(cueBall->x, cueBall->y); // This is also rayStart

        // Offset calculation based on actual cue ball position relative to the 'dir' line through targetStartPoint
        float offset = ((targetStartPoint.x - actualCueBallPhysicalCenter.x) * perp.x +
            (targetStartPoint.y - actualCueBallPhysicalCenter.y) * perp.y);
        float absOffset = fabsf(offset);
        float side = (offset >= 0 ? 1.0f : -1.0f);


        // Actual contact point on target ball edge
        D2D1_POINT_2F contactPoint = {
        contactBallCenter.x + perp.x * BALL_RADIUS * side,
        contactBallCenter.y + perp.y * BALL_RADIUS * side
        };

        // Tangent (cut shot) path from contact point
            // Tangent (cut shot) path: from contact point to contact ball center
        D2D1_POINT_2F objectBallDir = {
            contactBallCenter.x - contactPoint.x,
            contactBallCenter.y - contactPoint.y
        };
        float oLen = sqrtf(objectBallDir.x * objectBallDir.x + objectBallDir.y * objectBallDir.y);
        if (oLen != 0.0f) {
            objectBallDir.x /= oLen;
            objectBallDir.y /= oLen;
        }

        const float PRIMARY_LEN = 150.0f; //default=150.0f
        const float SECONDARY_LEN = 150.0f; //default=150.0f
        const float STRAIGHT_EPSILON = BALL_RADIUS * 0.05f;

        D2D1_POINT_2F primaryEnd = {
            targetStartPoint.x + dir.x * PRIMARY_LEN,
            targetStartPoint.y + dir.y * PRIMARY_LEN
        };

        // Secondary line starts from the contact ball's center
        D2D1_POINT_2F secondaryStart = contactBallCenter;
        D2D1_POINT_2F secondaryEnd = {
            secondaryStart.x + objectBallDir.x * SECONDARY_LEN,
            secondaryStart.y + objectBallDir.y * SECONDARY_LEN
        };

        if (absOffset < STRAIGHT_EPSILON)  // straight shot?
        {
            // Straight: secondary behind primary
                    // secondary behind primary {pDashedStyle param at end}
            pRT->DrawLine(secondaryStart, secondaryEnd, pPurpleBrush, 2.0f);
            //pRT->DrawLine(secondaryStart, secondaryEnd, pGhostBrush, 1.0f);
            pRT->DrawLine(targetStartPoint, primaryEnd, pCyanBrush, 2.0f);
            //pRT->DrawLine(targetStartPoint, primaryEnd, pBrush, 1.0f);
        }
        else
        {
            // Cut shot: both visible
                    // both visible for cut shot
            pRT->DrawLine(secondaryStart, secondaryEnd, pPurpleBrush, 2.0f);
            //pRT->DrawLine(secondaryStart, secondaryEnd, pGhostBrush, 1.0f);
            pRT->DrawLine(targetStartPoint, primaryEnd, pCyanBrush, 2.0f);
            //pRT->DrawLine(targetStartPoint, primaryEnd, pBrush, 1.0f);
        }
        // End improved trajectory logic

    //new code end

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
    SafeRelease(&pCyanBrush);
    SafeRelease(&pPurpleBrush);
    SafeRelease(&pDashedStyle);
}


void DrawUI(ID2D1RenderTarget* pRT) {
    if (!pTextFormat || !pLargeTextFormat) return;

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(UI_TEXT_COLOR, &pBrush);
    if (!pBrush) return;

    //new code
    // --- Always draw AI's 8?Ball call arrow when it's Player?2's turn and AI has called ---
    //if (isPlayer2AI && currentPlayer == 2 && calledPocketP2 >= 0) {
        // FIX: This condition correctly shows the AI's called pocket arrow.
    if (isPlayer2AI && IsPlayerOnEightBall(2) && calledPocketP2 >= 0) {
        // pocket index that AI called
        int idx = calledPocketP2;
        // draw large blue arrow
        ID2D1SolidColorBrush* pArrow = nullptr;
        pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrow);
        if (pArrow) {
            auto P = pocketPositions[idx];
            D2D1_POINT_2F tri[3] = {
                { P.x - 15.0f, P.y - 40.0f },
                { P.x + 15.0f, P.y - 40.0f },
                { P.x       , P.y - 10.0f }
            };
            ID2D1PathGeometry* geom = nullptr;
            pFactory->CreatePathGeometry(&geom);
            ID2D1GeometrySink* sink = nullptr;
            geom->Open(&sink);
            sink->BeginFigure(tri[0], D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLines(&tri[1], 2);
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            pRT->FillGeometry(geom, pArrow);
            SafeRelease(&sink);
            SafeRelease(&geom);
            SafeRelease(&pArrow);
        }
        // draw “Choose a pocket...” prompt
        D2D1_RECT_F txt = D2D1::RectF(
            TABLE_LEFT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 5.0f,
            TABLE_RIGHT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 30.0f
        );
        pRT->DrawText(
            L"AI has called this pocket",
            (UINT32)wcslen(L"AI has called this pocket"),
            pTextFormat,
            &txt,
            pBrush
        );
        // note: no return here — we still draw fouls/turn text underneath
    }
    //end new code

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
    // Draw Player 1 Side Ball
    if (player1Info.assignedType != BallType::NONE)
    {
        ID2D1SolidColorBrush* pBallBrush = nullptr;
        D2D1_COLOR_F ballColor = (player1Info.assignedType == BallType::SOLID) ?
            D2D1::ColorF(1.0f, 1.0f, 0.0f) : D2D1::ColorF(1.0f, 0.0f, 0.0f);
        pRT->CreateSolidColorBrush(ballColor, &pBallBrush);
        if (pBallBrush)
        {
            D2D1_POINT_2F ballCenter = D2D1::Point2F(p1Rect.right + 10.0f, p1Rect.top + 20.0f);
            float radius = 10.0f;
            D2D1_ELLIPSE ball = D2D1::Ellipse(ballCenter, radius, radius);
            pRT->FillEllipse(&ball, pBallBrush);
            SafeRelease(&pBallBrush);
            // Draw border around the ball
            ID2D1SolidColorBrush* pBorderBrush = nullptr;
            pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
            if (pBorderBrush)
            {
                pRT->DrawEllipse(&ball, pBorderBrush, 1.5f); // thin border
                SafeRelease(&pBorderBrush);
            }

            // If stripes, draw a stripe band
            if (player1Info.assignedType == BallType::STRIPE)
            {
                ID2D1SolidColorBrush* pStripeBrush = nullptr;
                pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pStripeBrush);
                if (pStripeBrush)
                {
                    D2D1_RECT_F stripeRect = D2D1::RectF(
                        ballCenter.x - radius,
                        ballCenter.y - 3.0f,
                        ballCenter.x + radius,
                        ballCenter.y + 3.0f
                    );
                    pRT->FillRectangle(&stripeRect, pStripeBrush);
                    SafeRelease(&pStripeBrush);
                }
            }
        }
    }


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
    // Draw Player 2 Side Ball
    if (player2Info.assignedType != BallType::NONE)
    {
        ID2D1SolidColorBrush* pBallBrush = nullptr;
        D2D1_COLOR_F ballColor = (player2Info.assignedType == BallType::SOLID) ?
            D2D1::ColorF(1.0f, 1.0f, 0.0f) : D2D1::ColorF(1.0f, 0.0f, 0.0f);
        pRT->CreateSolidColorBrush(ballColor, &pBallBrush);
        if (pBallBrush)
        {
            D2D1_POINT_2F ballCenter = D2D1::Point2F(p2Rect.right + 10.0f, p2Rect.top + 20.0f);
            float radius = 10.0f;
            D2D1_ELLIPSE ball = D2D1::Ellipse(ballCenter, radius, radius);
            pRT->FillEllipse(&ball, pBallBrush);
            SafeRelease(&pBallBrush);
            // Draw border around the ball
            ID2D1SolidColorBrush* pBorderBrush = nullptr;
            pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
            if (pBorderBrush)
            {
                pRT->DrawEllipse(&ball, pBorderBrush, 1.5f); // thin border
                SafeRelease(&pBorderBrush);
            }

            // If stripes, draw a stripe band
            if (player2Info.assignedType == BallType::STRIPE)
            {
                ID2D1SolidColorBrush* pStripeBrush = nullptr;
                pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pStripeBrush);
                if (pStripeBrush)
                {
                    D2D1_RECT_F stripeRect = D2D1::RectF(
                        ballCenter.x - radius,
                        ballCenter.y - 3.0f,
                        ballCenter.x + radius,
                        ballCenter.y + 3.0f
                    );
                    pRT->FillRectangle(&stripeRect, pStripeBrush);
                    SafeRelease(&pStripeBrush);
                }
            }
        }
    }

    // --- MODIFIED: Current Turn Arrow (Blue, Bigger, Beside Name) ---
    ID2D1SolidColorBrush* pArrowBrush = nullptr;
    pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrowBrush);
    if (pArrowBrush && currentGameState != GAME_OVER && currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
        float arrowSizeBase = 32.0f; // Base size for width/height offsets (4x original ~8)
        float arrowCenterY = p1Rect.top + uiHeight / 2.0f; // Center vertically with text box
        float arrowTipX, arrowBackX;

        D2D1_RECT_F playerBox = (currentPlayer == 1) ? p1Rect : p2Rect;
        arrowBackX = playerBox.left - 25.0f;
        arrowTipX = arrowBackX + arrowSizeBase * 0.75f;

        float notchDepth = 12.0f;  // Increased from 6.0f to make the rectangle longer
        float notchWidth = 10.0f;

        float cx = arrowBackX;
        float cy = arrowCenterY;

        // Define triangle + rectangle tail shape
        D2D1_POINT_2F tip = D2D1::Point2F(arrowTipX, cy);                           // tip
        D2D1_POINT_2F baseTop = D2D1::Point2F(cx, cy - arrowSizeBase / 2.0f);          // triangle top
        D2D1_POINT_2F baseBot = D2D1::Point2F(cx, cy + arrowSizeBase / 2.0f);          // triangle bottom

        // Rectangle coordinates for the tail portion:
        D2D1_POINT_2F r1 = D2D1::Point2F(cx - notchDepth, cy - notchWidth / 2.0f);   // rect top-left
        D2D1_POINT_2F r2 = D2D1::Point2F(cx, cy - notchWidth / 2.0f);                 // rect top-right
        D2D1_POINT_2F r3 = D2D1::Point2F(cx, cy + notchWidth / 2.0f);                 // rect bottom-right
        D2D1_POINT_2F r4 = D2D1::Point2F(cx - notchDepth, cy + notchWidth / 2.0f);    // rect bottom-left

        ID2D1PathGeometry* pPath = nullptr;
        if (SUCCEEDED(pFactory->CreatePathGeometry(&pPath))) {
            ID2D1GeometrySink* pSink = nullptr;
            if (SUCCEEDED(pPath->Open(&pSink))) {
                pSink->BeginFigure(tip, D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLine(baseTop);
                pSink->AddLine(r2); // transition from triangle into rectangle
                pSink->AddLine(r1);
                pSink->AddLine(r4);
                pSink->AddLine(r3);
                pSink->AddLine(baseBot);
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                SafeRelease(&pSink);
                pRT->FillGeometry(pPath, pArrowBrush);
            }
            SafeRelease(&pPath);
        }


        SafeRelease(&pArrowBrush);
    }

    //original
/*
    // --- MODIFIED: Current Turn Arrow (Blue, Bigger, Beside Name) ---
    ID2D1SolidColorBrush* pArrowBrush = nullptr;
    pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrowBrush);
    if (pArrowBrush && currentGameState != GAME_OVER && currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
        float arrowSizeBase = 32.0f; // Base size for width/height offsets (4x original ~8)
        float arrowCenterY = p1Rect.top + uiHeight / 2.0f; // Center vertically with text box
        float arrowTipX, arrowBackX;

        if (currentPlayer == 1) {
arrowBackX = p1Rect.left - 25.0f; // Position left of the box
            arrowTipX = arrowBackX + arrowSizeBase * 0.75f; // Pointy end extends right
            // Define points for right-pointing arrow
            //D2D1_POINT_2F pt1 = D2D1::Point2F(arrowTipX, arrowCenterY); // Tip
            //D2D1_POINT_2F pt2 = D2D1::Point2F(arrowBackX, arrowCenterY - arrowSizeBase / 2.0f); // Top-Back
            //D2D1_POINT_2F pt3 = D2D1::Point2F(arrowBackX, arrowCenterY + arrowSizeBase / 2.0f); // Bottom-Back
            // Enhanced arrow with base rectangle intersection
    float notchDepth = 6.0f; // Depth of square base "stem"
    float notchWidth = 4.0f; // Thickness of square part

    D2D1_POINT_2F pt1 = D2D1::Point2F(arrowTipX, arrowCenterY); // Tip
    D2D1_POINT_2F pt2 = D2D1::Point2F(arrowBackX, arrowCenterY - arrowSizeBase / 2.0f); // Top-Back
    D2D1_POINT_2F pt3 = D2D1::Point2F(arrowBackX - notchDepth, arrowCenterY - notchWidth / 2.0f); // Square Left-Top
    D2D1_POINT_2F pt4 = D2D1::Point2F(arrowBackX - notchDepth, arrowCenterY + notchWidth / 2.0f); // Square Left-Bottom
    D2D1_POINT_2F pt5 = D2D1::Point2F(arrowBackX, arrowCenterY + arrowSizeBase / 2.0f); // Bottom-Back


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


        //==================else player 2
        else { // Player 2
         // Player 2: Arrow left of P2 box, pointing right (or right of P2 box pointing left?)
         // Let's keep it consistent: Arrow left of the active player's box, pointing right.
// Let's keep it consistent: Arrow left of the active player's box, pointing right.
arrowBackX = p2Rect.left - 25.0f; // Position left of the box
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
        */


        // --- Persistent Blue 8?Ball Call Arrow & Prompt ---
    /*if (calledPocketP1 >= 0 || calledPocketP2 >= 0)
    {
        // determine index (default top?right)
        int idx = (currentPlayer == 1 ? calledPocketP1 : calledPocketP2);
        if (idx < 0) idx = (currentPlayer == 1 ? calledPocketP2 : calledPocketP1);
        if (idx < 0) idx = 2;

        // draw large blue arrow
        ID2D1SolidColorBrush* pArrow = nullptr;
        pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrow);
        if (pArrow) {
            auto P = pocketPositions[idx];
            D2D1_POINT_2F tri[3] = {
                {P.x - 15.0f, P.y - 40.0f},
                {P.x + 15.0f, P.y - 40.0f},
                {P.x       , P.y - 10.0f}
            };
            ID2D1PathGeometry* geom = nullptr;
            pFactory->CreatePathGeometry(&geom);
            ID2D1GeometrySink* sink = nullptr;
            geom->Open(&sink);
            sink->BeginFigure(tri[0], D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLines(&tri[1], 2);
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            pRT->FillGeometry(geom, pArrow);
            SafeRelease(&sink); SafeRelease(&geom); SafeRelease(&pArrow);
        }

        // draw prompt
        D2D1_RECT_F txt = D2D1::RectF(
            TABLE_LEFT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 5.0f,
            TABLE_RIGHT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 30.0f
        );
        pRT->DrawText(
            L"Choose a pocket...",
            (UINT32)wcslen(L"Choose a pocket..."),
            pTextFormat,
            &txt,
            pBrush
        );
    }*/

    // --- Persistent Blue 8?Ball Pocket Arrow & Prompt (once called) ---
/* if (calledPocketP1 >= 0 || calledPocketP2 >= 0)
{
    // 1) Determine pocket index
    int idx = (currentPlayer == 1 ? calledPocketP1 : calledPocketP2);
    // If the other player had called but it's now your turn, still show that call
    if (idx < 0) idx = (currentPlayer == 1 ? calledPocketP2 : calledPocketP1);
    if (idx < 0) idx = 2; // default to top?right if somehow still unset

    // 2) Draw large blue arrow
    ID2D1SolidColorBrush* pArrow = nullptr;
    pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrow);
    if (pArrow) {
        auto P = pocketPositions[idx];
        D2D1_POINT_2F tri[3] = {
            { P.x - 15.0f, P.y - 40.0f },
            { P.x + 15.0f, P.y - 40.0f },
            { P.x       , P.y - 10.0f }
        };
        ID2D1PathGeometry* geom = nullptr;
        pFactory->CreatePathGeometry(&geom);
        ID2D1GeometrySink* sink = nullptr;
        geom->Open(&sink);
        sink->BeginFigure(tri[0], D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLines(&tri[1], 2);
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();
        pRT->FillGeometry(geom, pArrow);
        SafeRelease(&sink);
        SafeRelease(&geom);
        SafeRelease(&pArrow);
    }

    // 3) Draw persistent prompt text
    D2D1_RECT_F txt = D2D1::RectF(
        TABLE_LEFT,
        TABLE_BOTTOM + CUSHION_THICKNESS + 5.0f,
        TABLE_RIGHT,
        TABLE_BOTTOM + CUSHION_THICKNESS + 30.0f
    );
    pRT->DrawText(
        L"Choose a pocket...",
        (UINT32)wcslen(L"Choose a pocket..."),
        pTextFormat,
        &txt,
        pBrush
    );
    // Note: no 'return'; allow foul/turn text to draw beneath if needed
} */

// new code ends here

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

    // --- Blue Arrow & Prompt for 8?Ball Call (while choosing or after called) ---
    if ((currentGameState == CHOOSING_POCKET_P1
        || currentGameState == CHOOSING_POCKET_P2)
        || (calledPocketP1 >= 0 || calledPocketP2 >= 0))
    {
        // determine index:
        //  - if a call exists, use it
        //  - if still choosing, use hover if any
        // determine index: use only the clicked call; default to top?right if unset
        int idx = (currentPlayer == 1 ? calledPocketP1 : calledPocketP2);
        if (idx < 0) idx = 2;

        // draw large blue arrow
        ID2D1SolidColorBrush* pArrow = nullptr;
        pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrow);
        if (pArrow) {
            auto P = pocketPositions[idx];
            D2D1_POINT_2F tri[3] = {
                {P.x - 15.0f, P.y - 40.0f},
                {P.x + 15.0f, P.y - 40.0f},
                {P.x       , P.y - 10.0f}
            };
            ID2D1PathGeometry* geom = nullptr;
            pFactory->CreatePathGeometry(&geom);
            ID2D1GeometrySink* sink = nullptr;
            geom->Open(&sink);
            sink->BeginFigure(tri[0], D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLines(&tri[1], 2);
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            pRT->FillGeometry(geom, pArrow);
            SafeRelease(&sink); SafeRelease(&geom); SafeRelease(&pArrow);
        }

        // draw prompt below pockets
        D2D1_RECT_F txt = D2D1::RectF(
            TABLE_LEFT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 5.0f,
            TABLE_RIGHT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 30.0f
        );
        pRT->DrawText(
            L"Choose a pocket...",
            (UINT32)wcslen(L"Choose a pocket..."),
            pTextFormat,
            &txt,
            pBrush
        );
        // do NOT return here; allow foul/turn text to display under the arrow
    }

    // Removed Obsolete
    /*
    // --- 8-Ball Pocket Selection Arrow & Prompt ---
    if (currentGameState == CHOOSING_POCKET_P1 || currentGameState == CHOOSING_POCKET_P2) {
        // Determine which pocket to highlight (default to Top-Right if unset)
        int idx = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
        if (idx < 0) idx = 2;

        // Draw the downward arrow
        ID2D1SolidColorBrush* pArrowBrush = nullptr;
        pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pArrowBrush);
        if (pArrowBrush) {
            D2D1_POINT_2F P = pocketPositions[idx];
            D2D1_POINT_2F tri[3] = {
                {P.x - 10.0f, P.y - 30.0f},
                {P.x + 10.0f, P.y - 30.0f},
                {P.x        , P.y - 10.0f}
            };
            ID2D1PathGeometry* geom = nullptr;
            pFactory->CreatePathGeometry(&geom);
            ID2D1GeometrySink* sink = nullptr;
            geom->Open(&sink);
            sink->BeginFigure(tri[0], D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLines(&tri[1], 2);
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            pRT->FillGeometry(geom, pArrowBrush);
            SafeRelease(&sink);
            SafeRelease(&geom);
            SafeRelease(&pArrowBrush);
        }

        // Draw “Choose a pocket...” text under the table
        D2D1_RECT_F prompt = D2D1::RectF(
            TABLE_LEFT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 5.0f,
            TABLE_RIGHT,
            TABLE_BOTTOM + CUSHION_THICKNESS + 30.0f
        );
        pRT->DrawText(
            L"Choose a pocket...",
            (UINT32)wcslen(L"Choose a pocket..."),
            pTextFormat,
            &prompt,
            pBrush
        );

        return; // Skip normal turn/foul text
    }
    */


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

    // --- Draw CHEAT MODE label if active ---
    if (cheatModeEnabled) {
        ID2D1SolidColorBrush* pCheatBrush = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &pCheatBrush);
        if (pCheatBrush && pTextFormat) {
            D2D1_RECT_F cheatTextRect = D2D1::RectF(
                TABLE_LEFT + 10.0f,
                TABLE_TOP + 10.0f,
                TABLE_LEFT + 200.0f,
                TABLE_TOP + 40.0f
            );
            pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            pRT->DrawText(L"CHEAT MODE ON", wcslen(L"CHEAT MODE ON"), pTextFormat, &cheatTextRect, pCheatBrush);
        }
        SafeRelease(&pCheatBrush);
    }
}

void DrawPowerMeter(ID2D1RenderTarget* pRT) {
    // Draw Border
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
    if (!pBorderBrush) return;
    pRT->DrawRectangle(&powerMeterRect, pBorderBrush, 2.0f);
    SafeRelease(&pBorderBrush);

    // Create Gradient Fill
    ID2D1GradientStopCollection* pGradientStops = nullptr;
    ID2D1LinearGradientBrush* pGradientBrush = nullptr;
    D2D1_GRADIENT_STOP gradientStops[4];
    gradientStops[0].position = 0.0f;
    gradientStops[0].color = D2D1::ColorF(D2D1::ColorF::Green);
    gradientStops[1].position = 0.45f;
    gradientStops[1].color = D2D1::ColorF(D2D1::ColorF::Yellow);
    gradientStops[2].position = 0.7f;
    gradientStops[2].color = D2D1::ColorF(D2D1::ColorF::Orange);
    gradientStops[3].position = 1.0f;
    gradientStops[3].color = D2D1::ColorF(D2D1::ColorF::Red);

    pRT->CreateGradientStopCollection(gradientStops, 4, &pGradientStops);
    if (pGradientStops) {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
        props.startPoint = D2D1::Point2F(powerMeterRect.left, powerMeterRect.bottom);
        props.endPoint = D2D1::Point2F(powerMeterRect.left, powerMeterRect.top);
        pRT->CreateLinearGradientBrush(props, pGradientStops, &pGradientBrush);
        SafeRelease(&pGradientStops);
    }

    // Calculate Fill Height
    float fillRatio = 0;
    //if (isAiming && (currentGameState == AIMING || currentGameState == BREAKING)) {
        // Determine if power meter should reflect shot power (human aiming or AI preparing)
    bool humanIsAimingPower = isAiming && (currentGameState == AIMING || currentGameState == BREAKING);
    // NEW Condition: AI is displaying its aim, so show its chosen power
    bool aiIsVisualizingPower = (isPlayer2AI && currentPlayer == 2 &&
        currentGameState == AI_THINKING && aiIsDisplayingAim);

    if (humanIsAimingPower || aiIsVisualizingPower) { // Use the new condition
        fillRatio = shotPower / MAX_SHOT_POWER;
    }
    float fillHeight = (powerMeterRect.bottom - powerMeterRect.top) * fillRatio;
    D2D1_RECT_F fillRect = D2D1::RectF(
        powerMeterRect.left,
        powerMeterRect.bottom - fillHeight,
        powerMeterRect.right,
        powerMeterRect.bottom
    );

    if (pGradientBrush) {
        pRT->FillRectangle(&fillRect, pGradientBrush);
        SafeRelease(&pGradientBrush);
    }

    // Draw scale notches
    ID2D1SolidColorBrush* pNotchBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pNotchBrush);
    if (pNotchBrush) {
        for (int i = 0; i <= 8; ++i) {
            float y = powerMeterRect.top + (powerMeterRect.bottom - powerMeterRect.top) * (i / 8.0f);
            pRT->DrawLine(
                D2D1::Point2F(powerMeterRect.right + 2.0f, y),
                D2D1::Point2F(powerMeterRect.right + 8.0f, y),
                pNotchBrush,
                1.5f
            );
        }
        SafeRelease(&pNotchBrush);
    }

    // Draw "Power" Label Below Meter
    if (pTextFormat) {
        ID2D1SolidColorBrush* pTextBrush = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pTextBrush);
        if (pTextBrush) {
            D2D1_RECT_F textRect = D2D1::RectF(
                powerMeterRect.left - 20.0f,
                powerMeterRect.bottom + 8.0f,
                powerMeterRect.right + 20.0f,
                powerMeterRect.bottom + 38.0f
            );
            pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            pRT->DrawText(L"Power", 5, pTextFormat, &textRect, pTextBrush);
            SafeRelease(&pTextBrush);
        }
    }

    // Draw Glow Effect if fully charged or fading out
    static float glowPulse = 0.0f;
    static bool glowIncreasing = true;
    static float glowFadeOut = 0.0f; // NEW: tracks fading out

    if (shotPower >= MAX_SHOT_POWER * 0.99f) {
        // While fully charged, keep pulsing normally
        if (glowIncreasing) {
            glowPulse += 0.02f;
            if (glowPulse >= 1.0f) glowIncreasing = false;
        }
        else {
            glowPulse -= 0.02f;
            if (glowPulse <= 0.0f) glowIncreasing = true;
        }
        glowFadeOut = 1.0f; // Reset fade out to full
    }
    else if (glowFadeOut > 0.0f) {
        // If shot fired, gradually fade out
        glowFadeOut -= 0.02f;
        if (glowFadeOut < 0.0f) glowFadeOut = 0.0f;
    }

    if (glowFadeOut > 0.0f) {
        ID2D1SolidColorBrush* pGlowBrush = nullptr;
        float effectiveOpacity = (0.3f + 0.7f * glowPulse) * glowFadeOut;
        pRT->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Red, effectiveOpacity),
            &pGlowBrush
        );
        if (pGlowBrush) {
            float glowCenterX = (powerMeterRect.left + powerMeterRect.right) / 2.0f;
            float glowCenterY = powerMeterRect.top;
            D2D1_ELLIPSE glowEllipse = D2D1::Ellipse(
                D2D1::Point2F(glowCenterX, glowCenterY - 10.0f),
                12.0f + 3.0f * glowPulse,
                6.0f + 2.0f * glowPulse
            );
            pRT->FillEllipse(&glowEllipse, pGlowBrush);
            SafeRelease(&pGlowBrush);
        }
    }
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
    float baseAlpha = 0.8f;
    float flashBoost = pocketFlashTimer * 0.5f; // Make flash effect boost alpha slightly
    float finalAlpha = std::min(1.0f, baseAlpha + flashBoost);
    pBgBrush->SetOpacity(finalAlpha);
    pRT->FillRoundedRectangle(&roundedRect, pBgBrush);
    pBgBrush->SetOpacity(1.0f); // Reset opacity after drawing

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

void DrawPocketSelectionIndicator(ID2D1RenderTarget* pRT) {
    /*  Never show the arrow while the player is still placing the
    cue-ball (ball-in-hand) – it otherwise hides behind the
    ghost-ball and can lock the UI.                               */

    /* Still skip the opening-break placement,
   but show the arrow during BALL-IN-HAND */
   // ? skip when no active call for the CURRENT shooter
    if ((currentPlayer == 1 && calledPocketP1 < 0) ||
        (currentPlayer == 2 && calledPocketP2 < 0))    return;
    /*if (currentGameState == PRE_BREAK_PLACEMENT)
        return;*/ //new ai-asked-to-disable
        /*if (currentGameState == BALL_IN_HAND_P1 ||
            currentGameState == BALL_IN_HAND_P2 ||
            currentGameState == PRE_BREAK_PLACEMENT)
        {
            return;
        }*/

    int pocketToIndicate = -1;
    // Whenever EITHER player has pocketed their first 7 and has called (human or AI),
    // we forcibly show their arrow—regardless of currentGameState.
    if ((currentPlayer == 1 && player1Info.ballsPocketedCount >= 7 && calledPocketP1 >= 0) ||
        (currentPlayer == 2 && player2Info.ballsPocketedCount >= 7 && calledPocketP2 >= 0))
    {
        pocketToIndicate = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    }
    /*// A human player is actively choosing if they are in the CHOOSING_POCKET state.
    bool isHumanChoosing = (currentGameState == CHOOSING_POCKET_P1 || (currentGameState == CHOOSING_POCKET_P2 && !isPlayer2AI));

    if (isHumanChoosing) {
        // When choosing, show the currently selected pocket (which has a default).
        pocketToIndicate = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    }
    else if (IsPlayerOnEightBall(currentPlayer)) {
        // If it's a normal turn but the player is on the 8-ball, show their called pocket as a reminder.
        pocketToIndicate = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
    }*/

    if (pocketToIndicate < 0 || pocketToIndicate > 5) {
        return; // Don't draw if no pocket is selected or relevant.
    }

    ID2D1SolidColorBrush* pArrowBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, 0.9f), &pArrowBrush);
    if (!pArrowBrush) return;

    // ... The rest of your arrow drawing geometry logic remains exactly the same ...
    // (No changes needed to the points/path drawing, only the logic above)
    D2D1_POINT_2F targetPocketCenter = pocketPositions[pocketToIndicate];
    float arrowHeadSize = HOLE_VISUAL_RADIUS * 0.5f;
    float arrowShaftLength = HOLE_VISUAL_RADIUS * 0.3f;
    float arrowShaftWidth = arrowHeadSize * 0.4f;
    float verticalOffsetFromPocketCenter = HOLE_VISUAL_RADIUS * 1.6f;
    D2D1_POINT_2F tip, baseLeft, baseRight, shaftTopLeft, shaftTopRight, shaftBottomLeft, shaftBottomRight;

    if (targetPocketCenter.y == TABLE_TOP) {
        tip = D2D1::Point2F(targetPocketCenter.x, targetPocketCenter.y + verticalOffsetFromPocketCenter + arrowHeadSize);
        baseLeft = D2D1::Point2F(targetPocketCenter.x - arrowHeadSize / 2.0f, targetPocketCenter.y + verticalOffsetFromPocketCenter);
        baseRight = D2D1::Point2F(targetPocketCenter.x + arrowHeadSize / 2.0f, targetPocketCenter.y + verticalOffsetFromPocketCenter);
        shaftTopLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y);
        shaftTopRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y);
        shaftBottomLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y - arrowShaftLength);
        shaftBottomRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y - arrowShaftLength);
    }
    else {
        tip = D2D1::Point2F(targetPocketCenter.x, targetPocketCenter.y - verticalOffsetFromPocketCenter - arrowHeadSize);
        baseLeft = D2D1::Point2F(targetPocketCenter.x - arrowHeadSize / 2.0f, targetPocketCenter.y - verticalOffsetFromPocketCenter);
        baseRight = D2D1::Point2F(targetPocketCenter.x + arrowHeadSize / 2.0f, targetPocketCenter.y - verticalOffsetFromPocketCenter);
        shaftTopLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y + arrowShaftLength);
        shaftTopRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y + arrowShaftLength);
        shaftBottomLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y);
        shaftBottomRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y);
    }

    ID2D1PathGeometry* pPath = nullptr;
    if (SUCCEEDED(pFactory->CreatePathGeometry(&pPath))) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pPath->Open(&pSink))) {
            pSink->BeginFigure(tip, D2D1_FIGURE_BEGIN_FILLED);
            pSink->AddLine(baseLeft); pSink->AddLine(shaftBottomLeft); pSink->AddLine(shaftTopLeft);
            pSink->AddLine(shaftTopRight); pSink->AddLine(shaftBottomRight); pSink->AddLine(baseRight);
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            SafeRelease(&pSink);
            pRT->FillGeometry(pPath, pArrowBrush);
        }
        SafeRelease(&pPath);
    }
    SafeRelease(&pArrowBrush);
}