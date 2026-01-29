#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING // [+] C++20 Fix: Silence codecvt deprecation errors
#include <windows.h>
#include <d2d1_1.h> // For even-odd fill mode
#include <d2d1.h>
#include <dwrite.h>
#include <fstream> // For file I/O
#include <iostream> // For some basic I/O, though not strictly necessary for just file ops
#include <array>
#include <vector>
#include <cmath>
#include <string>
#include <sstream> // Required for wostringstream
#include <algorithm> // Required for std::max, std::min
// --- Add these lines before GDI+ ---
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
// [+] NEW: Need GDI+ header for loading JPG resources
#include <gdiplus.h>
// --- Remove the macros after GDI+ ---
#undef min
#undef max
#include <ctime>    // Required for srand, time
#include <cstdlib> // Required for srand, rand (often included by others, but good practice)
#include <commctrl.h> // Needed for radio buttons etc. in dialog (if using native controls)
#include <mmsystem.h> // For PlaySound
#include <tchar.h> //midi func
#include <thread>
#include <atomic>
#include <wincodec.h>   // WIC
#include <locale>       // Add this header
#include <codecvt>      // Add this header
#include <map>          // NEW: Required for Trajectory recording
#include <random>       // [+] C++20 Migration: Required for std::shuffle
#include <numbers>      // [+] C++20 Migration: Standard math constants
#include "resource.h"

/*#ifndef HAS_STD_CLAMP
template <typename T>
T clamp(const T& v, const T& lo, const T& hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
namespace std { using ::clamp; }   // inject into std:: for seamless use
#define HAS_STD_CLAMP
#endif*/

#pragma comment(lib, "Comctl32.lib") // Link against common controls library
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment (lib, "Gdiplus.lib")
#pragma comment(lib, "Winmm.lib") // Link against Windows Multimedia library
#pragma comment(lib, "windowscodecs.lib")

// --- Constants ---
const float PI = std::numbers::pi_v<float>; // [+] C++20: Standard constant
const float BALL_RADIUS = 10.0f;
const float TABLE_LEFT = 80.0f;  // Adjusted for new window size
const float TABLE_TOP = 80.0f;   // Adjusted for new window size
const float TABLE_WIDTH = 840.0f;  // Increased by 20%
const float TABLE_HEIGHT = 420.0f; // Increased by 20%
const float TABLE_RIGHT = TABLE_LEFT + TABLE_WIDTH;
const float TABLE_BOTTOM = TABLE_TOP + TABLE_HEIGHT;
//const float CUSHION_THICKNESS = 20.0f;
const float WOODEN_RAIL_THICKNESS = 20.0f; // This was formerly CUSHION_THICKNESS
const float INNER_RIM_THICKNESS = 15.0f;    // This was formerly a local 'rimWidth'. Reduced from 15.0f to ~1/3rd. 
// --- WITH these new constants ---
const float CORNER_HOLE_VISUAL_RADIUS = 26.0f; // (22.0f)
const float MIDDLE_HOLE_VISUAL_RADIUS = 26.0f; // Middle pockets are now ~18% bigger (26.0f)
//const float HOLE_VISUAL_RADIUS = 22.0f; // Visual size of the hole
//const float POCKET_RADIUS = HOLE_VISUAL_RADIUS * 1.05f; // Make detection radius slightly larger // Make detection radius match visual size (or slightly larger)
const float MAX_SHOT_POWER = 15.0f;
const float FRICTION = 0.985f; // Friction factor per frame
const float MIN_VELOCITY_SQ = 0.01f * 0.01f; // Stop balls below this squared velocity
const float HEADSTRING_X = TABLE_LEFT + TABLE_WIDTH * 0.30f; // 30% line
const float RACK_POS_X = TABLE_LEFT + TABLE_WIDTH * 0.65f; // 65% line for rack apex
const float RACK_POS_Y = TABLE_TOP + TABLE_HEIGHT / 2.0f;
const UINT ID_TIMER = 1;
const int TARGET_FPS = 60; // Target frames per second for timer
//const float V_CUT_DEPTH = 32.0f; // How deep the V-cut goes into the rim
//const float V_CUT_WIDTH = 48.0f; // How wide the V-cut is at the rim edge
//const float RIM_WIDTH = 25.0f;   // Width of the chamfered inner rim 32.0f=>10=>25(both)
//const float rimWidth = 25.0f; // must match rimWidth used elsewhere in the project 15.0f=>10
//const float chamferSize = 25.0f;    // Defines the size of the 45-degree V-cut 25.0f
const float CHAMFER_SIZE = 25.0f;          // This was formerly a local 'chamferSize'.

// --- Enums ---
// Add GameType near other enums
enum GameType {
    EIGHT_BALL_MODE, // RENAMED
    NINE_BALL,
    STRAIGHT_POOL
};
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

// NEW: Enum and arrays for table color options
enum TableColor {
    INDIGO,
    TAN,
    TEAL,
    GREEN,
    RED
};

// --- Structs ---
struct Ball {
    int id;             // 0=Cue, 1-7=Solid, 8=Eight, 9-15=Stripe
    BallType type;
    float x, y;
    float vx, vy;
    D2D1_COLOR_F color;
    bool isPocketed;
    float rotationAngle; // radians, used for drawing rotation (visual spin)
    float rotationAxis;  // direction (radians) perpendicular to travel - used to offset highlight
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
IDWriteTextFormat* pTextFormatBold = nullptr;
IDWriteTextFormat* pLargeTextFormat = nullptr; // For "Foul!"
IDWriteTextFormat* pBallNumFormat = nullptr;
// -------------------- Globals for mask creation --------------------
static IWICImagingFactory* g_pWICFactory_masks = nullptr;
static std::vector<std::vector<uint8_t>> pocketMasks; // 6 masks, each maskWidth*maskHeight bytes (0/1)
static int pocketMaskWidth = 0;
static int pocketMaskHeight = 0;
static float pocketMaskOriginX = TABLE_LEFT;
static float pocketMaskOriginY = TABLE_TOP;
static float pocketMaskScale = 1.0f; // pixels per world unit
static const uint8_t POCKET_ALPHA_THRESHOLD = 48; // same idea as earlier

// Game State
HWND hwndMain = nullptr;
GameState currentGameState = SHOWING_DIALOG; // Start by showing dialog
std::vector<Ball> balls;
int currentPlayer = 1; // 1 or 2
PlayerInfo player1Info = { BallType::NONE, 0, L"Anti-Deluvian™"/*"Vince Woods"*//*"Player 1"*/ };
PlayerInfo player2Info = { BallType::NONE, 0, L"Virtus Pro"/*"CPU"*/ }; // Default P2 name
 // --- NEW: Global strings for customizable names ---
std::wstring g_player1Name = L"Anti-Deluvian™";
std::wstring g_player2Name = L"NetBurst-Hex™";
std::wstring g_aiName = L"Virtus Pro™";
// --- End New Globals ---
 // --- NEW: Struct and global variable for score statistics ---
struct PlayerStats {
    int p1_wins = 0;
    int p1_losses = 0;
    int ai_wins = 0; // From AI's perspective
    int human_vs_human_games = 0;
};
PlayerStats g_stats;
// --- End New Globals ---

// NEW Game Type and Straight Pool Specific Globals
GameType currentGameType = GameType::EIGHT_BALL_MODE; // Default to 8-Ball
int targetScoreStraightPool = 25;      // Winning score for Straight Pool (default, will be loaded)
int player1StraightPoolScore = 0;
int player2StraightPoolScore = 0;
int ballsOnTableCount = 15;            // Track balls on table for re-rack logic
const WCHAR* gameTypeNames[] = { L"8-Ball Billiards", L"Nine Ball", L"Straight Pool" }; // For ComboBox

// NEW 9-Ball Specific Globals
int lowestBallOnTable = 1;
int player1ConsecutiveFouls = 0;
int player2ConsecutiveFouls = 0;
bool isPushOutAvailable = false;
bool isPushOutShot = false; // True if the current shot IS a push-out

bool g_soundEffectsMuted = false; // NEW: Global flag to mute all sound effects
//bool shouldMusicBePlaying = true;
bool g_isPracticeMode = false; // NEW: Global flag for Practice Mode
// --- NEW: Debug Mode Flag ---
bool g_debugMode = false;
// [+] NEW: Paths Feature Globals
bool g_tracePaths = false;       // Toggle for ball trails
std::map<int, std::vector<D2D1_POINT_2F>> g_lastShotTrails; // Trails from the previous shot
std::map<int, std::vector<D2D1_POINT_2F>> g_tempShotTrails; // Trails being recorded for the current shot
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
// How long to display the FOUL! HUD after a foul is detected (frames)
int foulDisplayCounter = 0;           // counts down each frame; >0 => show FOUL!
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
// --- NEW: Audio Buffers for Instant Playback ---
// We preload wav files into these vectors to avoid disk I/O lag during gameplay.
std::vector<char> g_soundBufferCue;
std::vector<char> g_soundBufferPocket;
std::vector<char> g_soundBufferWall;
std::vector<char> g_soundBufferHit;
std::vector<char> g_soundBufferRack;
// [+] NEW: Buffers for Game Over and Foul sounds
std::vector<char> g_soundBufferWon;
std::vector<char> g_soundBufferLoss;
std::vector<char> g_soundBufferFoul;

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

// [+] NEW: Retry/Undo System State
struct GameStateBackup {
    std::vector<Ball> balls;
    GameState gameState;
    int currentPlayer;
    PlayerInfo p1Info;
    PlayerInfo p2Info;
    int p1ScoreStraight;
    int p2ScoreStraight;
    int ballsOnTable;
    int lowestBall;
    int p1Fouls;
    int p2Fouls;
    bool pushOut;
    int calledP1;
    int calledP2;
    std::wstring pocketMsg;
    bool foul;
    bool openingBreak;
    bool aiTurnPending;
};

GameStateBackup g_undoBackup;
bool g_hasUndo = false; // Is there a valid state to restore?

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
void StartMidi(HWND hwnd, const std::wstring& midiPath);
void StopMidi();

// NEW: Global for selected table color and definitions
TableColor selectedTableColor = INDIGO; // Default color
const WCHAR* tableColorNames[] = { L"Indigo", L"Tan", L"Teal", L"Green", L"Red" };
const D2D1_COLOR_F tableColorValues[] = {
D2D1::ColorF(0.05f, 0.09f, 0.28f),       // Indigo
D2D1::ColorF(0.3529f, 0.3137f, 0.2196f), // Tan
D2D1::ColorF(0.00392f, 0.467f, 0.439f)/*(0.0f, 0.545f, 0.541f)*//*(0.0f, 0.5f, 0.5f)*//*(0.0f, 0.4f, 0.4392f)*/,       // Teal default=(0.0f, 0.5f, 0.5f)
D2D1::ColorF(0.1608f, 0.4000f, 0.1765f), // Green
D2D1::ColorF(0.3882f, 0.1059f, 0.10196f) // Red
};

// UI Element Positions
D2D1_RECT_F powerMeterRect = { TABLE_RIGHT + WOODEN_RAIL_THICKNESS + 20, TABLE_TOP, TABLE_RIGHT + WOODEN_RAIL_THICKNESS + 50, TABLE_BOTTOM };
D2D1_RECT_F spinIndicatorRect = { TABLE_LEFT - WOODEN_RAIL_THICKNESS - 60, TABLE_TOP + 20, TABLE_LEFT - WOODEN_RAIL_THICKNESS - 20, TABLE_TOP + 60 }; // Circle area
D2D1_POINT_2F spinIndicatorCenter = { spinIndicatorRect.left + (spinIndicatorRect.right - spinIndicatorRect.left) / 2.0f, spinIndicatorRect.top + (spinIndicatorRect.bottom - spinIndicatorRect.top) / 2.0f };
float spinIndicatorRadius = (spinIndicatorRect.right - spinIndicatorRect.left) / 2.0f;
D2D1_RECT_F pocketedBallsBarRect = { TABLE_LEFT, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS + 30, TABLE_RIGHT, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS + 70 };

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
//const D2D1_COLOR_F TABLE_COLOR = D2D1::ColorF(0.0f, 0.5f, 0.5f); // Darker Green NEWCOLOR (0.0f, 0.5f, 0.1f) => (0.1608f, 0.4000f, 0.1765f) (uncomment this l8r if needbe)
// This is now a variable that can be changed, not a constant.
D2D1_COLOR_F TABLE_COLOR = D2D1::ColorF(0.05f, 0.09f, 0.28f); // Default to Indigo
//const D2D1_COLOR_F TABLE_COLOR = D2D1::ColorF(0.0f, 0.5f, 0.1f); // Darker Green NEWCOLOR (0.0f, 0.5f, 0.1f) => (0.1608f, 0.4000f, 0.1765f)
/*
MPool: *Darker tan= (0.3529f, 0.3137f, 0.2196f) *Indigo= (0.05f, 0.09f, 0.28f) xPurple= (0.1922f, 0.2941f, 0.4118f); xTan= (0.5333f, 0.4706f, 0.3569f); (Red= (0.3882f, 0.1059f, 0.10196f); *Green(default)= (0.1608f, 0.4000f, 0.1765f); *Teal= (0.0f, 0.4f, 0.4392f) + Teal40%reduction= (0.0f, 0.3333f, 0.4235f);
Hex: Teal=#00abc2 Tan=#7c6d53 Teal2=#03adc2 xPurple=#314b69 ?Tan=#88785b Red=#631b1a
*/
const D2D1_COLOR_F CUSHION_COLOR = D2D1::ColorF(D2D1::ColorF(0.3608f, 0.0275f, 0.0078f)); // NEWCOLOR ::Red => (0.3608f, 0.0275f, 0.0078f)
//const D2D1_COLOR_F CUSHION_COLOR = D2D1::ColorF(D2D1::ColorF::Red); // NEWCOLOR ::Red => (0.3608f, 0.0275f, 0.0078f)
const D2D1_COLOR_F POCKET_COLOR = D2D1::ColorF(D2D1::ColorF::Black);
const D2D1_COLOR_F CUE_BALL_COLOR = D2D1::ColorF(D2D1::ColorF::White);
const D2D1_COLOR_F EIGHT_BALL_COLOR = D2D1::ColorF(D2D1::ColorF::Black);
const D2D1_COLOR_F SOLID_COLOR = D2D1::ColorF(D2D1::ColorF::Goldenrod); // Solids = Yellow Goldenrod
const D2D1_COLOR_F STRIPE_COLOR = D2D1::ColorF(D2D1::ColorF::DarkOrchid);   // Stripes = Red DarkOrchid
const D2D1_COLOR_F AIM_LINE_COLOR = D2D1::ColorF(D2D1::ColorF::White, 0.7f); // Semi-transparent white
const D2D1_COLOR_F FOUL_TEXT_COLOR = D2D1::ColorF(D2D1::ColorF::Red);
const D2D1_COLOR_F TURN_ARROW_COLOR = D2D1::ColorF(0.1333f, 0.7294f, 0.7490f); //NEWCOLOR 0.1333f, 0.7294f, 0.7490f => ::Blue
//const D2D1_COLOR_F TURN_ARROW_COLOR = D2D1::ColorF(D2D1::ColorF::Blue);
const D2D1_COLOR_F ENGLISH_DOT_COLOR = D2D1::ColorF(D2D1::ColorF::Red);
const D2D1_COLOR_F UI_TEXT_COLOR = D2D1::ColorF(D2D1::ColorF::Black);

// --------------------------------------------------------------------
//  Realistic colours for each id (0-15)
//  0 = cue-ball (white) | 1-7 solids | 8 = eight-ball | 9-15 stripes
// --------------------------------------------------------------------
static const D2D1_COLOR_F BALL_COLORS[16] =
{
    D2D1::ColorF(D2D1::ColorF::White),          // 0  cue
    D2D1::ColorF(1.00f, 0.85f, 0.00f),          // 1  yellow
    D2D1::ColorF(0.05f, 0.30f, 1.00f),          // 2  blue
    D2D1::ColorF(0.90f, 0.10f, 0.10f),          // 3  red
    D2D1::ColorF(0.55f, 0.25f, 0.85f),          // 4  purple
    D2D1::ColorF(1.00f, 0.55f, 0.00f),          // 5  orange
    D2D1::ColorF(0.00f, 0.60f, 0.30f),          // 6  green
    D2D1::ColorF(0.50f, 0.05f, 0.05f),          // 7  maroon / burgundy
    D2D1::ColorF(D2D1::ColorF::Black),          // 8  black
    D2D1::ColorF(1.00f, 0.85f, 0.00f),          // 9  (yellow stripe)
    D2D1::ColorF(0.05f, 0.30f, 1.00f),          // 10 blue stripe
    D2D1::ColorF(0.90f, 0.10f, 0.10f),          // 11 red stripe
    D2D1::ColorF(0.55f, 0.25f, 0.85f),          // 12 purple stripe
    D2D1::ColorF(1.00f, 0.55f, 0.00f),          // 13 orange stripe
    D2D1::ColorF(0.00f, 0.60f, 0.30f),          // 14 green stripe
    D2D1::ColorF(0.50f, 0.05f, 0.05f)           // 15 maroon stripe
};

// [+] NEW: Debug Undo System Globals
struct PocketEvent {
    int ballId;
    int playerId; // Who pocketed it? (For correcting Straight Pool scores)
    bool wasOn8Ball; // Was the player "On 8-Ball" before this?
};
std::vector<PocketEvent> g_pocketHistory; // Stack LIFO to track order
std::atomic<bool> g_debugActionLock(false); // Semaphore for atomic operations

// Quick helper
inline D2D1_COLOR_F GetBallColor(int id)
{
    return (id >= 0 && id < 16) ? BALL_COLORS[id]
        : D2D1::ColorF(D2D1::ColorF::White);
}

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
void FinalizeGame(int winner, int loser, const std::wstring& reason); // <<< ADD THIS NEW LINE
void ReRackPracticeMode(); // NEW: For re-racking in practice mode
void ReRackStraightPool(); // Forward declaration for your existing function
void HandleCheatDropPocket(Ball* b, int p); // Atomic cheat drop handler
void SkipCurrentShot(); // [+] NEW: Fast-forward function
Ball* GetBallById(int id);
Ball* GetCueBall();
//void PlayGameMusic(HWND hwnd); //midi func
void AIBreakShot();

// [+] NEW: Forward Declarations for Undo System
void SaveStateForUndo();
void RestoreStateFromUndo();

// Drawing Functions
void DrawScene(ID2D1RenderTarget* pRT);
void DrawTable(ID2D1RenderTarget* pRT, ID2D1Factory* pFactory);
void DrawBalls(ID2D1RenderTarget* pRT);
void DrawCueStick(ID2D1RenderTarget* pRT);
void DrawAimingAids(ID2D1RenderTarget* pRT);
void DrawUI(ID2D1RenderTarget* pRT);
void DrawPowerMeter(ID2D1RenderTarget* pRT);
void DrawSpinIndicator(ID2D1RenderTarget* pRT);
void RespawnBall(int ballId); // NEW Helper
void DrawPocketedBallsIndicator(ID2D1RenderTarget* pRT);
void DrawBallInHandIndicator(ID2D1RenderTarget* pRT);
// NEW
void DrawPocketSelectionIndicator(ID2D1RenderTarget* pRT);
void DrawBallTrails(ID2D1RenderTarget* pRT, ID2D1Factory* pFactory);
// [+] NEW: Forward declaration for the About Dialog procedure
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Helper Functions
float GetDistance(float x1, float y1, float x2, float y2);
float GetDistanceSq(float x1, float y1, float x2, float y2);
bool IsValidCueBallPosition(float x, float y, bool checkHeadstring);
//template <typename T> void SafeRelease(T** ppT);
 // --- Single, canonical SafeRelease helper (place once, top-level) ---
template <typename T>
static inline void SafeRelease(T** ppT) {
    if (ppT && *ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}
/*template <typename T>
void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}*/
static D2D1_COLOR_F Lighten(const D2D1_COLOR_F& c, float factor);
static bool IsBallInPocketBlackArea(const Ball& b, int p);
static std::vector<std::vector<std::array<D2D1_POINT_2F, 4>>> g_pocketBezierCache;
static bool g_pocketBezierCacheInitialized = false;

// --- NEW HELPER FORWARD DECLARATIONS ---
bool IsPlayerOnEightBall(int player);
void CheckAndTransitionToPocketChoice(int playerID);
void UpdateLowestBall(); // NEW 9-Ball Helper
// --- ADD FORWARD DECLARATION FOR NEW HELPER HERE ---
float PointToLineSegmentDistanceSq(D2D1_POINT_2F p, D2D1_POINT_2F a, D2D1_POINT_2F b);
// --- End Forward Declaration ---
bool LineSegmentIntersection(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_POINT_2F p3, D2D1_POINT_2F p4, D2D1_POINT_2F& intersection); // Keep this if present
bool PointInTriangle(const D2D1_POINT_2F& p, const D2D1_POINT_2F& a, const D2D1_POINT_2F& b, const D2D1_POINT_2F& c);

// Helper to load a file into a buffer
bool LoadWavFileToMemory(const std::wstring& filename, std::vector<char>& buffer) {
    // Construct absolute path based on executable location to ensure we find the files
    TCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    TCHAR* lastBackslash = wcsrchr(exePath, L'\\');
    if (lastBackslash) *(lastBackslash + 1) = L'\0';
    std::wstring fullPath = std::wstring(exePath) + filename;

    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(size);
    if (file.read(buffer.data(), size)) return true;
    return false;
}

// Helper to play sound from memory
void PlayGameSound(const std::vector<char>& buffer) {
    if (g_soundEffectsMuted || buffer.empty()) return;
    // SND_MEMORY: Play from buffer
    // SND_ASYNC: Don't block the game loop
    // SND_NODEFAULT: Don't play system beep if fail
    PlaySound((LPCWSTR)buffer.data(), NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
// --- End Audio Buffers ---

// Return the visual pocket radius for pocket index p
inline float GetPocketVisualRadius(int p)
{
    // Middle pockets are larger; corners use the corner radius
    return (p == 1 || p == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
}
// -----------------------------
// Pocket / capsule collision helper
// -----------------------------
static inline float Dot(const D2D1_POINT_2F& a, const D2D1_POINT_2F& b) {
    return a.x * b.x + a.y * b.y;
}
static inline float Clampf(float v, float a, float b) {
    return (v < a) ? a : (v > b ? b : v);
}

// Ensure WIC factory (call once)
bool EnsureWICFactory_Masks()
{
    if (g_pWICFactory_masks) return true;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&g_pWICFactory_masks));
    return SUCCEEDED(hr);
}

// Cleanup
void ShutdownPocketMasks_D2D()
{
    pocketMasks.clear();
    pocketMaskWidth = pocketMaskHeight = 0;
    pocketMaskOriginX = pocketMaskOriginY = 0.0f;
    pocketMaskScale = 1.0f;
    if (g_pWICFactory_masks) { g_pWICFactory_masks->Release(); g_pWICFactory_masks = nullptr; }
}

// ----------------------------------------------------------------------
// CreatePocketMasks_D2D  (uses your pocket path geometry)
// ----------------------------------------------------------------------
void CreatePocketMasks_D2D(int pixelWidth, int pixelHeight, float originX, float originY, float scale)
{
    if (!EnsureWICFactory_Masks()) return;
    if (!pFactory) return;

    pocketMaskWidth = pixelWidth;
    pocketMaskHeight = pixelHeight;
    pocketMaskOriginX = originX;
    pocketMaskOriginY = originY;
    pocketMaskScale = scale;

    pocketMasks.clear();
    pocketMasks.resize(6);

    // Create WIC bitmap + D2D render target
    IWICBitmap* pWicBitmap = nullptr;
    HRESULT hr = g_pWICFactory_masks->CreateBitmap(
        pocketMaskWidth, pocketMaskHeight,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &pWicBitmap);
    if (FAILED(hr) || !pWicBitmap) {
        SafeRelease(&pWicBitmap);
        return;
    }

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.dpiX = 96.0f;
    rtProps.dpiY = 96.0f;

    ID2D1RenderTarget* pWicRT = nullptr;
    hr = pFactory->CreateWicBitmapRenderTarget(pWicBitmap, rtProps, &pWicRT);
    if (FAILED(hr) || !pWicRT) {
        SafeRelease(&pWicBitmap);
        return;
    }

    ID2D1SolidColorBrush* pPocketBrush = nullptr;
    pWicRT->CreateSolidColorBrush(POCKET_COLOR, &pPocketBrush);

    // For each pocket: clear, draw the exact keyhole shape, EndDraw, lock & read pixels
    for (int i = 0; i < 6; ++i) {
        pWicRT->BeginDraw();
        pWicRT->Clear(D2D1::ColorF(0, 0.0f)); // fully transparent background

        if (pPocketBrush) {
            // --- FOOLPROOF FIX: Create a unified "keyhole" mask for each pocket ---
            // This ensures the mask perfectly matches the visual opening, preventing rim bounces.

            // 1. Draw the main circular hole. We draw it in the center of our local bitmap.
            D2D1_POINT_2F maskCenter = { pocketMaskWidth / 2.0f, pocketMaskHeight / 2.0f };
            float radius = GetPocketVisualRadius(i) * pocketMaskScale; // Scale radius to pixels
            D2D1_ELLIPSE pocketEllipse = D2D1::Ellipse(maskCenter, radius, radius);
            pWicRT->FillEllipse(&pocketEllipse, pPocketBrush);

            // 2. Define and draw the V-shaped cutout that leads into the hole.
            // The coordinates are calculated in the local space of the mask bitmap.
            const float rimWidthPixels = INNER_RIM_THICKNESS * pocketMaskScale;
            const float vCutDepthPixels = CHAMFER_SIZE * pocketMaskScale;

            D2D1_POINT_2F v_tip, v_base1, v_base2;

            // The geometry is defined for each pocket's orientation
            if (i == 1) { // Top-Middle (V opens down)
                v_tip = { maskCenter.x, maskCenter.y + vCutDepthPixels };
                v_base1 = { maskCenter.x - vCutDepthPixels, maskCenter.y };
                v_base2 = { maskCenter.x + vCutDepthPixels, maskCenter.y };
            }
            else if (i == 4) { // Bottom-Middle (V opens up)
                v_tip = { maskCenter.x, maskCenter.y - vCutDepthPixels };
                v_base1 = { maskCenter.x + vCutDepthPixels, maskCenter.y };
                v_base2 = { maskCenter.x - vCutDepthPixels, maskCenter.y };
            }
            else if (i == 0) { // Top-Left (V opens down-right)
                v_tip = { maskCenter.x + vCutDepthPixels, maskCenter.y + vCutDepthPixels };
                v_base1 = { maskCenter.x, maskCenter.y + vCutDepthPixels };
                v_base2 = { maskCenter.x + vCutDepthPixels, maskCenter.y };
            }
            else if (i == 2) { // Top-Right (V opens down-left)
                v_tip = { maskCenter.x - vCutDepthPixels, maskCenter.y + vCutDepthPixels };
                v_base1 = { maskCenter.x - vCutDepthPixels, maskCenter.y };
                v_base2 = { maskCenter.x, maskCenter.y + vCutDepthPixels };
            }
            else if (i == 3) { // Bottom-Left (V opens up-right)
                v_tip = { maskCenter.x + vCutDepthPixels, maskCenter.y - vCutDepthPixels };
                v_base1 = { maskCenter.x + vCutDepthPixels, maskCenter.y };
                v_base2 = { maskCenter.x, maskCenter.y - vCutDepthPixels };
            }
            else { // i == 5, Bottom-Right (V opens up-left)
                v_tip = { maskCenter.x - vCutDepthPixels, maskCenter.y - vCutDepthPixels };
                v_base1 = { maskCenter.x, maskCenter.y - vCutDepthPixels };
                v_base2 = { maskCenter.x - vCutDepthPixels, maskCenter.y };
            }

            // Create and fill a path geometry for the triangle
            ID2D1PathGeometry* pTriPath = nullptr;
            pFactory->CreatePathGeometry(&pTriPath);
            if (pTriPath) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pTriPath->Open(&pSink))) {
                    pSink->BeginFigure(v_tip, D2D1_FIGURE_BEGIN_FILLED);
                    pSink->AddLine(v_base1);
                    pSink->AddLine(v_base2);
                    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    pSink->Close();
                }
                SafeRelease(&pSink);
                // *** THIS IS THE CORRECTED LINE ***
                pWicRT->FillGeometry(pTriPath, pPocketBrush); //
                SafeRelease(&pTriPath);
            }
        }

        hr = pWicRT->EndDraw();
        (void)hr; // continue even if EndDraw fails

        // Lock the WIC bitmap and extract alpha => build mask
        WICRect lockRect = { 0, 0, pocketMaskWidth, pocketMaskHeight };
        IWICBitmapLock* pLock = nullptr;
        hr = pWicBitmap->Lock(&lockRect, WICBitmapLockRead, &pLock);
        if (SUCCEEDED(hr) && pLock) {
            UINT cbStride = 0;
            UINT cbBufferSize = 0;
            BYTE* pv = nullptr;
            hr = pLock->GetDataPointer(&cbBufferSize, &pv);
            pLock->GetStride(&cbStride);

            if (SUCCEEDED(hr) && pv) {
                pocketMasks[i].assign(pocketMaskWidth * pocketMaskHeight, 0);
                for (int y = 0; y < pocketMaskHeight; ++y) {
                    BYTE* row = pv + y * cbStride;
                    int base = y * pocketMaskWidth;
                    for (int x = 0; x < pocketMaskWidth; ++x) {
                        BYTE a = row[x * 4 + 3]; // premultiplied BGRA alpha at offset 3
                        pocketMasks[i][base + x] = (a > POCKET_ALPHA_THRESHOLD) ? 1 : 0;
                    }
                }
            }
            pLock->Release();
        }
    } // end for pockets

    // cleanup
    SafeRelease(&pPocketBrush);
    SafeRelease(&pWicRT);
    SafeRelease(&pWicBitmap);
}

// Create six per-pocket masks by rendering each pocket individually into a WIC bitmap.
// pixelWidth/Height = resolution for the masks
// originX/originY = world coordinate mapping for mask pixel (0,0) -> these world coords
// scale = pixels per world unit (1.0f = 1 pixel per D2D unit)
/*void CreatePocketMasks_D2D(int pixelWidth, int pixelHeight, float originX, float originY, float scale)
{
    if (!EnsureWICFactory_Masks()) return;
    if (!pFactory) return; // requires your ID2D1Factory*

    pocketMaskWidth = pixelWidth;
    pocketMaskHeight = pixelHeight;
    pocketMaskOriginX = originX;
    pocketMaskOriginY = originY;
    pocketMaskScale = scale;

    pocketMasks.clear();
    pocketMasks.resize(6); // six pockets

    // Create a WIC bitmap and a D2D render-target that draws into it.
    IWICBitmap* pWicBitmap = nullptr;
    HRESULT hr = g_pWICFactory_masks->CreateBitmap(
        pocketMaskWidth, pocketMaskHeight,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &pWicBitmap);
    if (FAILED(hr) || !pWicBitmap) {
        SafeRelease(&pWicBitmap);
        return;
    }

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.dpiX = 96.0f;
    rtProps.dpiY = 96.0f;

    ID2D1RenderTarget* pWicRT = nullptr;
    hr = pFactory->CreateWicBitmapRenderTarget(pWicBitmap, rtProps, &pWicRT);
    if (FAILED(hr) || !pWicRT) {
        SafeRelease(&pWicBitmap);
        return;
    }

    // Create a brush for drawing the pocket shape (solid black)
    ID2D1SolidColorBrush* pBrush = nullptr;
    pWicRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &pBrush);

    // Set transform: world -> pixel = (world - origin) * scale
    D2D1::Matrix3x2F transform = D2D1::Matrix3x2F::Translation(-pocketMaskOriginX, -pocketMaskOriginY) *
        D2D1::Matrix3x2F::Scale(pocketMaskScale, pocketMaskScale);
    pWicRT->SetTransform(transform);

    // For each pocket: clear, draw that pocket's black area, EndDraw, lock & read pixels into pocketMasks[i]
    for (int p = 0; p < 6; ++p) {
        pWicRT->BeginDraw();
        // Clear to transparent
        pWicRT->Clear(D2D1::ColorF(0, 0.0f));

        // Draw only the pocket p — use the same geometry you use for on-screen pockets.
        // Example draws a filled ellipse of the visual pocket radius; if you draw semicircles
        // or a path on screen, replicate that exact path here for pixel-perfect masks.
        float visualRadius = GetPocketVisualRadius(p); // uses your helper (MIDDLE vs CORNER)
        D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(pocketPositions[p].x, pocketPositions[p].y), visualRadius, visualRadius);
        pWicRT->FillEllipse(&e, pBrush);
*/
/*ID2D1SolidColorBrush* pPocketBrush = nullptr;
ID2D1SolidColorBrush* pRimBrush = nullptr;
pWicRT->CreateSolidColorBrush(POCKET_COLOR, &pPocketBrush);
pWicRT->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f), &pRimBrush);
if (pPocketBrush && pRimBrush) {
    for (int i = 0; i < 6; ++i) {
        ID2D1PathGeometry* pPath = nullptr;
        pFactory->CreatePathGeometry(&pPath);
        if (pPath) {
            ID2D1GeometrySink* pSink = nullptr;
            if (SUCCEEDED(pPath->Open(&pSink))) {
                float currentVisualRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
                float mouthRadius = currentVisualRadius * 1.5f;
                float backRadius = currentVisualRadius * 1.1f;
                D2D1_POINT_2F center = pocketPositions[i];
                D2D1_POINT_2F p1, p2;
                D2D1_SWEEP_DIRECTION sweep;
                if (i == 0) {
                    p1 = D2D1::Point2F(center.x + mouthRadius, center.y);
                    p2 = D2D1::Point2F(center.x, center.y + mouthRadius);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 2) {
                    p1 = D2D1::Point2F(center.x, center.y + mouthRadius);
                    p2 = D2D1::Point2F(center.x - mouthRadius, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 3) {
                    p1 = D2D1::Point2F(center.x, center.y - mouthRadius);
                    p2 = D2D1::Point2F(center.x + mouthRadius, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 5) {
                    p1 = D2D1::Point2F(center.x - mouthRadius, center.y);
                    p2 = D2D1::Point2F(center.x, center.y - mouthRadius);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 1) {
                    p1 = D2D1::Point2F(center.x - mouthRadius / 1.5f, center.y);
                    p2 = D2D1::Point2F(center.x + mouthRadius / 1.5f, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                }
                else {
                    p1 = D2D1::Point2F(center.x + mouthRadius / 1.5f, center.y);
                    p2 = D2D1::Point2F(center.x - mouthRadius / 1.5f, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                }
                pSink->BeginFigure(center, D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLine(p1);
                pSink->AddArc(D2D1::ArcSegment(p2, D2D1::SizeF(backRadius, backRadius), 0.0f, sweep, D2D1_ARC_SIZE_SMALL));
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                SafeRelease(&pSink);
                pWicRT->FillGeometry(pPath, pPocketBrush);
                // Draw the inner rim for a beveled effect (Start)
                D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(0.8f, 0.8f, center);
                ID2D1TransformedGeometry* pTransformedGeo = nullptr;
                pFactory->CreateTransformedGeometry(pPath, &scale, &pTransformedGeo);
                if (pTransformedGeo) {
                    pWicRT->FillGeometry(pTransformedGeo, pRimBrush);
                    SafeRelease(&pTransformedGeo);
                } //End
            }
            SafeRelease(&pPath);
        }
    }
}
SafeRelease(&pPocketBrush);
SafeRelease(&pRimBrush);*/
/*
        // If your on-screen pocket is a semicircle / capsule / complex path,
        // create & fill the same path geometry here instead of FillEllipse.

        hr = pWicRT->EndDraw();
        if (FAILED(hr)) {
            // continue but mask may be invalid
        }

        // Lock WIC bitmap and read pixels
        WICRect lockRect = { 0, 0, pocketMaskWidth, pocketMaskHeight };
        IWICBitmapLock* pLock = nullptr;
        hr = pWicBitmap->Lock(&lockRect, WICBitmapLockRead, &pLock);
        if (SUCCEEDED(hr) && pLock) {
            UINT cbStride = 0;
            UINT cbBufferSize = 0;
            BYTE* pv = nullptr;
            hr = pLock->GetDataPointer(&cbBufferSize, &pv);
            pLock->GetStride(&cbStride);

            if (SUCCEEDED(hr) && pv) {
                pocketMasks[p].assign(pocketMaskWidth * pocketMaskHeight, 0);
                for (int y = 0; y < pocketMaskHeight; ++y) {
                    BYTE* row = pv + y * cbStride;
                    int base = y * pocketMaskWidth;
                    for (int x = 0; x < pocketMaskWidth; ++x) {
                        BYTE a = row[x * 4 + 3]; // premultiplied BGRA -> alpha at offset 3
                        pocketMasks[p][base + x] = (a > POCKET_ALPHA_THRESHOLD) ? 1 : 0;
                    }
                }
            }
            pLock->Release();
        }
    } // end loop pockets

    // cleanup
    SafeRelease(&pBrush);
    SafeRelease(&pWicRT);
    SafeRelease(&pWicBitmap);
}*/

//orig function
/*
// Create pixel mask by rendering pocket shapes into a WIC bitmap via Direct2D
// pixelWidth/height = desired mask resolution in pixels
// originX/originY = world coordinate representing pixel (0,0) origin of mask
// scale = pixels per world unit (use 1.0f for 1 pixel per D2D unit; >1 for supersampling)
void CreatePocketMask_D2D(int pixelWidth, int pixelHeight, float originX, float originY, float scale)
{
    if (!EnsureWICFactory()) return;
    if (!pFactory) return; // requires your ID2D1Factory* (global in your code)

    maskWidth = pixelWidth;
    maskHeight = pixelHeight;
    maskOriginX = originX;
    maskOriginY = originY;
    maskScale = scale;
    pocketMask.assign(maskWidth * maskHeight, 0);

    // Create a WIC bitmap (32bpp premultiplied BGRA) to be the render target surface
    IWICBitmap* pWicBitmap = nullptr;
    HRESULT hr = g_pWICFactory->CreateBitmap(
        maskWidth, maskHeight,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &pWicBitmap);
    if (FAILED(hr) || !pWicBitmap) {
        if (pWicBitmap) pWicBitmap->Release();
        return;
    }

    // Create a Direct2D render target that draws into the WIC bitmap
    // Use default D2D render target properties but ensure DPI = 96 (pixels align)
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    rtProps.dpiX = 96.0f;
    rtProps.dpiY = 96.0f;
    ID2D1RenderTarget* pWicRT = nullptr;
    hr = pFactory->CreateWicBitmapRenderTarget(
        pWicBitmap,
        rtProps,
        &pWicRT);
    if (FAILED(hr) || !pWicRT) {
        pWicBitmap->Release();
        return;
    }

    // Create a black brush for pocket fill
    ID2D1SolidColorBrush* pBlackBrush = nullptr;
    pWicRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &pBlackBrush);

    // Set transform: scale then translate so we can draw in world coords.
    // world -> pixel: (world - origin) * scale
    D2D1::Matrix3x2F transform = D2D1::Matrix3x2F::Translation(-maskOriginX, -maskOriginY) *
        D2D1::Matrix3x2F::Scale(maskScale, maskScale);
    pWicRT->SetTransform(transform);

    // Begin drawing
    pWicRT->BeginDraw();
    pWicRT->Clear(D2D1::ColorF(0, 0.0f)); // transparent

    // ---- Draw pocket shapes (use your exact same Direct2D math here) ----
    // For pixel-perfect match you MUST draw the *same* shapes/paths you use in the screen render.
    // The example below draws filled circles at pocketPositions; replace with your path code if needed.
*/
/*    for (int i = 0; i < 6; ++i) {
        float visualRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
        // Draw as an ellipse centered at pocketPositions[i] in *world* coords — transform handles scale/origin.
        D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(pocketPositions[i].x, pocketPositions[i].y), visualRadius, visualRadius);
        pWicRT->FillEllipse(&e, pBlackBrush);
*/
/*
ID2D1SolidColorBrush* pPocketBrush = nullptr;
ID2D1SolidColorBrush* pRimBrush = nullptr;
pWicRT->CreateSolidColorBrush(POCKET_COLOR, &pPocketBrush);
pWicRT->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f), &pRimBrush);
if (pPocketBrush && pRimBrush) {
    for (int i = 0; i < 6; ++i) {
        ID2D1PathGeometry* pPath = nullptr;
        pFactory->CreatePathGeometry(&pPath);
        if (pPath) {
            ID2D1GeometrySink* pSink = nullptr;
            if (SUCCEEDED(pPath->Open(&pSink))) {
                float currentVisualRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
                float mouthRadius = currentVisualRadius * 1.5f;
                float backRadius = currentVisualRadius * 1.1f;
                D2D1_POINT_2F center = pocketPositions[i];
                D2D1_POINT_2F p1, p2;
                D2D1_SWEEP_DIRECTION sweep;
                if (i == 0) {
                    p1 = D2D1::Point2F(center.x + mouthRadius, center.y);
                    p2 = D2D1::Point2F(center.x, center.y + mouthRadius);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 2) {
                    p1 = D2D1::Point2F(center.x, center.y + mouthRadius);
                    p2 = D2D1::Point2F(center.x - mouthRadius, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 3) {
                    p1 = D2D1::Point2F(center.x, center.y - mouthRadius);
                    p2 = D2D1::Point2F(center.x + mouthRadius, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 5) {
                    p1 = D2D1::Point2F(center.x - mouthRadius, center.y);
                    p2 = D2D1::Point2F(center.x, center.y - mouthRadius);
                    sweep = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                }
                else if (i == 1) {
                    p1 = D2D1::Point2F(center.x - mouthRadius / 1.5f, center.y);
                    p2 = D2D1::Point2F(center.x + mouthRadius / 1.5f, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                }
                else {
                    p1 = D2D1::Point2F(center.x + mouthRadius / 1.5f, center.y);
                    p2 = D2D1::Point2F(center.x - mouthRadius / 1.5f, center.y);
                    sweep = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                }
                pSink->BeginFigure(center, D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLine(p1);
                pSink->AddArc(D2D1::ArcSegment(p2, D2D1::SizeF(backRadius, backRadius), 0.0f, sweep, D2D1_ARC_SIZE_SMALL));
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                SafeRelease(&pSink);
                pWicRT->FillGeometry(pPath, pPocketBrush);
                // Draw the inner rim for a beveled effect (Start)
                D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(0.8f, 0.8f, center);
                ID2D1TransformedGeometry* pTransformedGeo = nullptr;
                pFactory->CreateTransformedGeometry(pPath, &scale, &pTransformedGeo);
                if (pTransformedGeo) {
                    pWicRT->FillGeometry(pTransformedGeo, pRimBrush);
                    SafeRelease(&pTransformedGeo);
                } //End
            }
            SafeRelease(&pPath);
        }
    }
}
SafeRelease(&pPocketBrush);
SafeRelease(&pRimBrush);

// If your visual is a semicircle or a complex path, create + fill a path geometry here:
// ID2D1PathGeometry* pPath; pFactory->CreatePathGeometry(&pPath); ... FillGeometry ...
//}

// End drawing
pWicRT->EndDraw();

// Release brush & render target (we still have the WIC bitmap to lock/read)
//SafeRelease(&pBlackBrush);
SafeRelease(&pWicRT);

// Lock WIC bitmap and read pixel data (32bppPBGRA -> bytes: B G R A premultiplied)
WICRect lockRect = { 0, 0, maskWidth, maskHeight };
IWICBitmapLock* pLock = nullptr;
hr = pWicBitmap->Lock(&lockRect, WICBitmapLockRead, &pLock);
if (SUCCEEDED(hr) && pLock) {
    UINT cbStride = 0;
    UINT cbBufferSize = 0;
    BYTE* pv = nullptr;
    hr = pLock->GetDataPointer(&cbBufferSize, &pv);
    pLock->GetStride(&cbStride);

    if (SUCCEEDED(hr) && pv) {
        // Iterate rows -> columns; pixel is 4 bytes (B,G,R,A) premultiplied
        for (int y = 0; y < maskHeight; ++y) {
            BYTE* row = pv + y * cbStride;
            int base = y * maskWidth;
            for (int x = 0; x < maskWidth; ++x) {
                BYTE a = row[x * 4 + 3]; // alpha channel
                pocketMask[base + x] = (a > ALPHA_THRESHOLD) ? 1 : 0;
            }
        }
    }
    pLock->Release();
}

pWicBitmap->Release();
}
*/

// Helper: world coords -> mask pixel coords (rounded). returns false if outside mask bounds.
inline bool WorldToMaskPixel_Masks(float worldX, float worldY, int& outX, int& outY)
{
    float px = (worldX - pocketMaskOriginX) * pocketMaskScale;
    float py = (worldY - pocketMaskOriginY) * pocketMaskScale;
    int ix = (int)floorf(px + 0.5f);
    int iy = (int)floorf(py + 0.5f);
    if (ix < 0 || iy < 0 || ix >= pocketMaskWidth || iy >= pocketMaskHeight) return false;
    outX = ix; outY = iy; return true;
}

// ----------------------------------------------------------------------
// IsBallTouchingPocketMask_D2D (unchanged pixel-perfect test)
// ----------------------------------------------------------------------
bool IsBallTouchingPocketMask_D2D(const Ball& b, int pocketIndex)
{
    if (pocketIndex < 0 || pocketIndex >= (int)pocketMasks.size()) return false;
    if (pocketMaskWidth <= 0 || pocketMaskHeight <= 0) return false;
    const std::vector<uint8_t>& mask = pocketMasks[pocketIndex];
    if (mask.empty()) return false;

    // --- FOOLPROOF FIX: Transform the ball's center into the mask's local space ---
    float ballMaskX = (b.x - pocketPositions[pocketIndex].x) * pocketMaskScale + (pocketMaskWidth / 2.0f);
    float ballMaskY = (b.y - pocketPositions[pocketIndex].y) * pocketMaskScale + (pocketMaskHeight / 2.0f);
    float radiusInPixels = BALL_RADIUS * pocketMaskScale;

    // Ball bounding box in world coords
    /*float leftW = b.x - BALL_RADIUS;
    float rightW = b.x + BALL_RADIUS;
    float topW = b.y - BALL_RADIUS;
    float bottomW = b.y + BALL_RADIUS;

    int leftPx = (int)floorf((leftW - pocketMaskOriginX) * pocketMaskScale);
    int topPx = (int)floorf((topW - pocketMaskOriginY) * pocketMaskScale);
    int rightPx = (int)ceilf((rightW - pocketMaskOriginX) * pocketMaskScale);
    int bottomPx = (int)ceilf((bottomW - pocketMaskOriginY) * pocketMaskScale);

    // clamp to mask bounds
    leftPx = std::max(0, leftPx);
    topPx = std::max(0, topPx);
    rightPx = std::min(pocketMaskWidth - 1, rightPx);
    bottomPx = std::min(pocketMaskHeight - 1, bottomPx);

    // ball center and radius in pixels
    float cx = (b.x - pocketMaskOriginX) * pocketMaskScale;
    float cy = (b.y - pocketMaskOriginY) * pocketMaskScale;
    float rPx = BALL_RADIUS * pocketMaskScale;
    float rPx2 = rPx * rPx;*/

    // Define a search area around the ball's transformed position
    int startX = std::max(0, static_cast<int>(ballMaskX - radiusInPixels));
    int endX = std::min(pocketMaskWidth - 1, static_cast<int>(ballMaskX + radiusInPixels));
    int startY = std::max(0, static_cast<int>(ballMaskY - radiusInPixels));
    int endY = std::min(pocketMaskHeight - 1, static_cast<int>(ballMaskY + radiusInPixels));

    // Check each pixel within the search area
    for (int y = startY; y <= endY; ++y) {
        int rowBase = y * pocketMaskWidth;
        for (int x = startX; x <= endX; ++x) {
            // Check if this mask pixel is "solid" (part of the pocket)
            if (mask[rowBase + x] > 0) {
                // Check if the distance from the pixel to the ball's center is within the ball's radius
                float dx = (float)x - ballMaskX;
                float dy = (float)y - ballMaskY;
                if (dx * dx + dy * dy < radiusInPixels * radiusInPixels) {
                    return true; // Collision detected!
                }

                /*for (int yy = topPx; yy <= bottomPx; ++yy) {
                    int rowBase = yy * pocketMaskWidth;
                    for (int xx = leftPx; xx <= rightPx; ++xx) {
                        float px = (float)xx + 0.5f;
                        float py = (float)yy + 0.5f;
                        float dx = px - cx;
                        float dy = py - cy;
                        if (dx * dx + dy * dy <= rPx2) {
                            if (mask[rowBase + xx]) return true;*/
            }
        }
    }
    return false;
}

/*
// Pixel-perfect test using the selected pocket mask
bool IsBallTouchingPocketMask_D2D(const Ball& b, int pocketIndex)
{
    if (pocketIndex < 0 || pocketIndex >= (int)pocketMasks.size()) return false;
    if (pocketMaskWidth <= 0 || pocketMaskHeight <= 0) return false;
    const std::vector<uint8_t>& mask = pocketMasks[pocketIndex];
    if (mask.empty()) return false;

    // Ball bounding box in world coords
    float leftW = b.x - BALL_RADIUS;
    float rightW = b.x + BALL_RADIUS;
    float topW = b.y - BALL_RADIUS;
    float bottomW = b.y + BALL_RADIUS;

    int leftPx = (int)floorf((leftW - pocketMaskOriginX) * pocketMaskScale);
    int topPx = (int)floorf((topW - pocketMaskOriginY) * pocketMaskScale);
    int rightPx = (int)ceilf((rightW - pocketMaskOriginX) * pocketMaskScale);
    int bottomPx = (int)ceilf((bottomW - pocketMaskOriginY) * pocketMaskScale);

    // clamp to mask bounds
    leftPx = std::max(0, leftPx);
    topPx = std::max(0, topPx);
    rightPx = std::min(pocketMaskWidth - 1, rightPx);
    bottomPx = std::min(pocketMaskHeight - 1, bottomPx);

    // ball center and radius in pixels
    float cx = (b.x - pocketMaskOriginX) * pocketMaskScale;
    float cy = (b.y - pocketMaskOriginY) * pocketMaskScale;
    float rPx = BALL_RADIUS * pocketMaskScale;
    float rPx2 = rPx * rPx;

    for (int yy = topPx; yy <= bottomPx; ++yy) {
        int rowBase = yy * pocketMaskWidth;
        for (int xx = leftPx; xx <= rightPx; ++xx) {
            float px = (float)xx + 0.5f;
            float py = (float)yy + 0.5f;
            float dx = px - cx;
            float dy = py - cy;
            if (dx * dx + dy * dy <= rPx2) {
                if (mask[rowBase + xx]) return true;
            }
        }
    }
    return false;
}*/

bool IsWrongTargetForCurrentPlayer(const Ball* ball) {
    if (currentGameType == GameType::STRAIGHT_POOL) {
        return false; // In Straight Pool, any numbered ball is a valid target
    }

    if (currentGameType == GameType::NINE_BALL) {
        if (!ball) return false;
        if (isPushOutShot) return false; // On a push-out, any ball (or no ball) is a valid target
        // Otherwise, must hit the lowest ball
        return (ball->id != lowestBallOnTable);
    }

    // --- 8-Ball Logic ---
    // if (currentGameType == GameType::EIGHT_BALL_MODE)

    // Original 8-Ball logic
    if (!ball) return false;
    if (ball->id == 8) {
        // Hitting 8-ball is only legal if the player is on the 8-ball
        PlayerInfo& playerInfo = (currentPlayer == 1) ? player1Info : player2Info;
        return !(playerInfo.assignedType != BallType::NONE && playerInfo.ballsPocketedCount >= 7);
    }
    // Check against assigned types
    if (currentPlayer == 1 && player1Info.assignedType != BallType::NONE)
        return ball->type != player1Info.assignedType;
    if (currentPlayer == 2 && player2Info.assignedType != BallType::NONE)
        return ball->type != player2Info.assignedType;

    // If table is open (no assigned types), any ball except 8 is okay.
    return false;
}


//new polygon mask helpers
// -----------------------------
// Polygon-based pocket hit test
// -----------------------------
// Add these helper functions near your other geometry helpers.
// This implementation approximates pocket curves with a sampled polygon
// then does precise point-in-polygon and point-to-segment checks.
// This avoids any Direct2D FillContainsPoint incompatibility and
// gives deterministic shape-based results (not circle-based).
//
// Tweak SAMPLE_PER_QUARTER_CIRCLE if you want finer resolution.

static const int SAMPLE_PER_QUARTER_CIRCLE = 24; // controls polygon smoothness (increase = more precise) (default=14)

static float Sq(float v) { return v * v; }

static float DistancePointSegmentSq(const D2D1_POINT_2F& p, const D2D1_POINT_2F& a, const D2D1_POINT_2F& b)
{
    // compute squared distance from p to segment ab
    D2D1_POINT_2F ab = { b.x - a.x, b.y - a.y };
    D2D1_POINT_2F ap = { p.x - a.x, p.y - a.y };
    float abLen2 = ab.x * ab.x + ab.y * ab.y;
    if (abLen2 <= 1e-8f) {
        return (p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y);
    }
    float t = (ap.x * ab.x + ap.y * ab.y) / abLen2;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    D2D1_POINT_2F proj = { a.x + ab.x * t, a.y + ab.y * t };
    return (p.x - proj.x) * (p.x - proj.x) + (p.y - proj.y) * (p.y - proj.y);
}

// Winding/ray-cast point-in-polygon (non-zero winding). Returns true if inside.
static bool PointInPolygon(const std::vector<D2D1_POINT_2F>& poly, const D2D1_POINT_2F& pt)
{
    bool inside = false;
    size_t n = poly.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const D2D1_POINT_2F& pi = poly[i], & pj = poly[j];
        bool intersect = ((pi.y > pt.y) != (pj.y > pt.y)) &&
            (pt.x < (pj.x - pi.x)* (pt.y - pi.y) / (pj.y - pi.y + 1e-12f) + pi.x);
        if (intersect) inside = !inside;
    }
    return inside;
}

// build an approximated polygon describing the black/inner pocket area (the "hole")
// pIndex: 0..5
// output poly is in the same coordinate space as your table (pixel coordinates)
static void BuildPocketPolygon(int pIndex, std::vector<D2D1_POINT_2F>& outPoly)
{
    outPoly.clear();
    D2D1_POINT_2F center = pocketPositions[pIndex];

    // choose inner black radius for the pocket (this should match the drawn 'black' mouth)
    float vis = GetPocketVisualRadius(pIndex);
    float innerRadius = vis * 0.55f; // tweak multiplier to match your drawn black area exactly (default=0.85)

    int samples = SAMPLE_PER_QUARTER_CIRCLE * 2; // semi-circle uses half circle of samples

    // Middle pockets (top-middle index 1, bottom-middle index 4) -> semicircle opening into table
    if (pIndex == 1 || pIndex == 4) {
        // top-middle (1): semicircle opening downward => angles [0..PI]
        // bottom-middle (4): semicircle opening upward => angles [PI..2PI]
        float a0 = (pIndex == 1) ? 0.0f : PI;
        float a1 = a0 + PI;
        for (int i = 0; i <= samples; ++i) {
            float t = float(i) / float(samples);
            float a = a0 + (a1 - a0) * t;
            float x = center.x + cosf(a) * innerRadius;
            float y = center.y + sinf(a) * innerRadius;
            outPoly.push_back(D2D1::Point2F(x, y));
        }
        // close polygon by adding the center "mouth" edge (slightly inside) to make a filled shape
        // we insert two small points inside the semicircle to make the polygon closed and reasonable
        outPoly.push_back(D2D1::Point2F(center.x + innerRadius * 0.25f, center.y)); // inner mid-right
        outPoly.push_back(D2D1::Point2F(center.x - innerRadius * 0.25f, center.y)); // inner mid-left
        return;
    }

    // Corner pockets: quarter-circle interior where black area is inside the table corner.
    // Create a quarter-arc polygon oriented depending on which corner.
    float startAngle = 0.0f, endAngle = 0.0f;
    // p0 TL, p2 TR, p3 BL, p5 BR
    // angle mapping:
    // 0 -> right (0), PI/2 -> down, PI -> left, 3PI/2 -> up
    if (pIndex == 0) { // top-left: interior down-right => angles [0 .. PI/2]
        startAngle = 0.0f; endAngle = PI / 2.0f;
    }
    else if (pIndex == 2) { // top-right: interior down-left => angles [PI/2 .. PI]
        startAngle = PI / 2.0f; endAngle = PI;
    }
    else if (pIndex == 3) { // bottom-left: interior up-right => angles [3PI/2 .. 2PI]
        startAngle = 3.0f * PI / 2.0f; endAngle = 2.0f * PI;
    }
    else if (pIndex == 5) { // bottom-right: interior up-left => angles [PI .. 3PI/2]
        startAngle = PI; endAngle = 3.0f * PI / 2.0f;
    }
    else {
        // fallback: full circle
        startAngle = 0.0f; endAngle = 2.0f * PI;
    }

    int quarterSamples = SAMPLE_PER_QUARTER_CIRCLE;
    for (int i = 0; i <= quarterSamples; ++i) {
        float t = float(i) / float(quarterSamples);
        float a = startAngle + (endAngle - startAngle) * t;
        float x = center.x + cosf(a) * innerRadius;
        float y = center.y + sinf(a) * innerRadius;
        outPoly.push_back(D2D1::Point2F(x, y));
    }
    // add a small interior chord to close polygon toward table inside
    // determine inward direction (table interior)
    D2D1_POINT_2F innerOffset = { 0, 0 };
    if (pIndex == 0) innerOffset = { innerRadius * 0.35f, innerRadius * 0.35f }; // down-right
    if (pIndex == 2) innerOffset = { -innerRadius * 0.35f, innerRadius * 0.35f }; // down-left
    if (pIndex == 3) innerOffset = { innerRadius * 0.35f, -innerRadius * 0.35f }; // up-right
    if (pIndex == 5) innerOffset = { -innerRadius * 0.35f, -innerRadius * 0.35f }; // up-left

    outPoly.push_back(D2D1::Point2F(center.x + innerOffset.x, center.y + innerOffset.y));
}

// Main high precision pocket test using polygon
// returns true if ball overlaps or touches the polygonal black area
bool IsBallInPocketPolygon(const Ball& b, int pIndex)
{
    if (pIndex < 0 || pIndex > 5) return false;

    // Build polygon describing pocket black area
    std::vector<D2D1_POINT_2F> poly;
    BuildPocketPolygon(pIndex, poly);
    if (poly.size() < 3) return false;

    D2D1_POINT_2F center = D2D1::Point2F(b.x, b.y);

    // Quick bounding-box reject (cheap)
    float minx = FLT_MAX, miny = FLT_MAX, maxx = -FLT_MAX, maxy = -FLT_MAX;
    for (auto& pt : poly) {
        if (pt.x < minx) minx = pt.x;
        if (pt.y < miny) miny = pt.y;
        if (pt.x > maxx) maxx = pt.x;
        if (pt.y > maxy) maxy = pt.y;
    }
    // expand by BALL_RADIUS
    minx -= BALL_RADIUS; miny -= BALL_RADIUS; maxx += BALL_RADIUS; maxy += BALL_RADIUS;
    if (center.x < minx || center.x > maxx || center.y < miny || center.y > maxy) {
        // center outside expanded bounds; not enough to intersect
        // but still we should check if the ball edge could overlap an extended corner -> we've expanded by BALL_RADIUS so safe
        return false;
    }

    // 1) If ball center is inside the polygon -> pocketed
    if (PointInPolygon(poly, center)) {
        // Table side check: ensure ball is approaching from table side (avoid false positives from below/above)
        bool topPocket = (pIndex == 0 || pIndex == 1 || pIndex == 2);
        if (topPocket) {
            if (b.y < pocketPositions[pIndex].y - BALL_RADIUS) return false;
        }
        else {
            if (b.y > pocketPositions[pIndex].y + BALL_RADIUS) return false;
        }
        return true;
    }

    // 2) Otherwise, check distance from ball center to any polygon edge
    float rr = BALL_RADIUS * BALL_RADIUS;
    for (size_t i = 0, n = poly.size(); i < n; ++i) {
        const D2D1_POINT_2F& a = poly[i];
        const D2D1_POINT_2F& bb = poly[(i + 1) % n];
        float d2 = DistancePointSegmentSq(center, a, bb);
        if (d2 <= rr + 1e-6f) {
            // edge-touch candidate -> verify table side (avoid outside approach)
            bool topPocket = (pIndex == 0 || pIndex == 1 || pIndex == 2);
            if (topPocket) {
                if (b.y < pocketPositions[pIndex].y - BALL_RADIUS) return false;
            }
            else {
                if (b.y > pocketPositions[pIndex].y + BALL_RADIUS) return false;
            }
            return true;
        }
    }

    return false;
}

bool IsBallInPocket(const D2D1_POINT_2F& ballPos) {
    // New: check if ball is within V-shaped cutout region at any pocket
    for (int i = 0; i < 6; ++i) {
        D2D1_POINT_2F center = pocketPositions[i];
        D2D1_POINT_2F prev = pocketPositions[(i + 5) % 6];
        D2D1_POINT_2F next = pocketPositions[(i + 1) % 6];
        float dx1 = center.x - prev.x, dy1 = center.y - prev.y;
        float dx2 = next.x - center.x, dy2 = next.y - center.y;
        float len1 = sqrtf(dx1 * dx1 + dy1 * dy1);
        float len2 = sqrtf(dx2 * dx2 + dy2 * dy2);
        dx1 /= len1; dy1 /= len1;
        dx2 /= len2; dy2 /= len2;
        float bisectX = dx1 + dx2;
        float bisectY = dy1 + dy2;
        float bisectLen = sqrtf(bisectX * bisectX + bisectY * bisectY);
        bisectX /= bisectLen; bisectY /= bisectLen;
        // V-cut tip
        D2D1_POINT_2F vTip = {
            center.x + bisectX * CHAMFER_SIZE,
            center.y + bisectY * CHAMFER_SIZE
        };
        // V-cut base points
        float perpX = -bisectY, perpY = bisectX;
        D2D1_POINT_2F vBase1 = {
            center.x + perpX * (CHAMFER_SIZE)+bisectX * INNER_RIM_THICKNESS, // width/2 becomes chamfer_size
            center.y + perpY * (CHAMFER_SIZE)+bisectY * INNER_RIM_THICKNESS
        };
        D2D1_POINT_2F vBase2 = {
            center.x - perpX * (CHAMFER_SIZE)+bisectX * INNER_RIM_THICKNESS,
            center.y - perpY * (CHAMFER_SIZE)+bisectY * INNER_RIM_THICKNESS
        };
        // Point-in-triangle test for V region
        if (PointInTriangle(ballPos, vBase1, vTip, vBase2))
            return true;
    }
    return false;
}

// Helper: Point-in-triangle test (barycentric)
bool PointInTriangle(const D2D1_POINT_2F& p, const D2D1_POINT_2F& a, const D2D1_POINT_2F& b, const D2D1_POINT_2F& c) {
    float dX = p.x - c.x;
    float dY = p.y - c.y;
    float dX21 = c.x - b.x;
    float dY12 = b.y - c.y;
    float D = dY12 * (a.x - c.x) + dX21 * (a.y - c.y);
    float s = dY12 * dX + dX21 * dY;
    float t = (c.y - a.y) * dX + (a.x - c.x) * dY;
    if (D < 0) return (s <= 0) && (t <= 0) && (s + t >= D);
    return (s >= 0) && (t >= 0) && (s + t <= D);
}

// Backwards-compatible alias: some places in the code call IsBallInPocket(...)
// so provide a thin wrapper that uses the precise black-area test.
bool IsBallInPocket(const Ball& b, int pIndex)
{
    return IsBallInPocketPolygon(b, pIndex);
}

// Backwards-compatible wrapper (if your code calls IsBallInPocket(...))
bool IsBallInPocket(const Ball* ball, int p)
{
    if (!ball) return false;
    return IsBallInPocketPolygon(*ball, p);
} //end

// NEW 9-Ball helper function to re-spot a ball (e.g., 9-ball on a foul)
void RespawnBall(int ballId) {
    Ball* ballToRespawn = GetBallById(ballId);
    if (!ballToRespawn || !ballToRespawn->isPocketed) return; // Not pocketed or doesn't exist

    // Place on the foot spot (rack position)
    float targetX = RACK_POS_X;
    float targetY = RACK_POS_Y;

    // Check for overlap and nudge if necessary
    int attempts = 0;
    bool positionFound = false;
    const float collisionThresholdSq = (BALL_RADIUS * 2.0f) * (BALL_RADIUS * 2.0f);

    while (attempts < 50) {
        positionFound = true;
        for (Ball& b : balls) {
            if (b.id != ballId && !b.isPocketed) {
                if (GetDistanceSq(targetX, targetY, b.x, b.y) < collisionThresholdSq) {
                    positionFound = false;
                    break;
                }
            }
        }

        if (positionFound) break;

        // Nudge downwards if spot is occupied
        targetY += BALL_RADIUS * 0.5f;
        // Simple boundary check
        if (targetY > TABLE_BOTTOM - BALL_RADIUS) targetY = RACK_POS_Y - BALL_RADIUS * 2.0f; // wrap around
        attempts++;
    }

    ballToRespawn->x = targetX;
    ballToRespawn->y = targetY;
    ballToRespawn->vx = 0;
    ballToRespawn->vy = 0;
    ballToRespawn->isPocketed = false;
}

void HandleCheatDropPocket(Ball* b, int p) {
    // Glitch #3 Fix: Atomic check. Only process if not already pocketed.
    if (!b || b->isPocketed) return;

    // [+] NEW: Record history before processing logic changes
    bool playerWasOn8 = IsPlayerOnEightBall(currentPlayer);
    g_pocketHistory.push_back({ b->id, currentPlayer, playerWasOn8 });

    // 1. Mark as pocketed to prevent re-entry from this or other functions
    b->isPocketed = true;
    b->vx = b->vy = 0.0f;

    // 2. Play sound
    if (!g_soundEffectsMuted) {
        PlayGameSound(g_soundBufferPocket);
        //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("pocket.wav")).detach();
    }

    // *** NEW FIX: Handle Cue Ball Respawn Logic ***
    if (b->id == 0) { // Cue ball pocketed
        RespawnCueBall(false); // Respawn anywhere on table (not restricted to headstring)
        // Optional: Uncomment to enforce foul in Versus modes
        // if (!g_isPracticeMode) { foulCommitted = true; }
        return; // Prevent further processing for cue ball
    }
    // *** END NEW FIX ***

    // 3. Update game logic (scores, counts) - *ONCE*
    if (currentGameType == GameType::STRAIGHT_POOL) {
        if (b->id > 0 && b->id <= 15) {
            if (currentPlayer == 1) player1StraightPoolScore++;
            else if (!g_isPracticeMode) player2StraightPoolScore++;

            ballsOnTableCount--; // Decrement count *ONCE*
        }
    }
    else if (currentGameType == GameType::EIGHT_BALL_MODE) {
        if (b->id == 8) lastEightBallPocketIndex = p;

        if (player1Info.assignedType == BallType::NONE && (b->type == BallType::SOLID || b->type == BallType::STRIPE)) {
            AssignPlayerBallTypes(b->type, true);
        }
        else if (b->id != 0 && b->id != 8) {
            if (b->type == player1Info.assignedType) player1Info.ballsPocketedCount++;
            else if (b->type == player2Info.assignedType) player2Info.ballsPocketedCount++;
        }

        if (b->id != 0) ballsOnTableCount--; // Decrement count *ONCE*
    }
    else if (currentGameType == GameType::NINE_BALL) {
        if (b->id != 0) ballsOnTableCount--; // Decrement count *ONCE*
    }

    // 4. Check Post-Pocketing Game State (Win/Re-rack)
    if (g_isPracticeMode) {
        if (ballsOnTableCount == 0) ReRackPracticeMode();
        else currentGameState = PLAYER1_TURN; // Always P1 turn in practice
    }
    else if (currentGameType == GameType::STRAIGHT_POOL) {
        CheckGameOverConditions(false, false); // Check for win
        // Glitch #2 Fix: Call user's re-rack function if 1 ball left
        if (currentGameState != GAME_OVER && ballsOnTableCount == 1) {
            ReRackStraightPool(); // Calls your existing function
        }
        else if (currentGameState != GAME_OVER) {
            currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
            if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
        }
    }
    else if (currentGameType == GameType::EIGHT_BALL_MODE) {
        if (b->id == 8) {
            // Dropped the 8-ball, check win/loss
            int shooterId = currentPlayer;
            int opponentId = (currentPlayer == 1) ? 2 : 1;
            int called = (shooterId == 1) ? calledPocketP1 : calledPocketP2;
            PlayerInfo& shooterInfo = (shooterId == 1) ? player1Info : player2Info;
            bool clearedGroup = (shooterInfo.assignedType != BallType::NONE && shooterInfo.ballsPocketedCount >= 7);
            bool isLegalWinCheat = clearedGroup && (called != -1) && (called == p);

            if (isLegalWinCheat) FinalizeGame(shooterId, opponentId, L"(Cheat Mode)");
            else FinalizeGame(opponentId, shooterId, L"(Cheat Mode - Illegal 8-Ball)");
        }
        else if (IsPlayerOnEightBall(currentPlayer)) {
            CheckAndTransitionToPocketChoice(currentPlayer);
        }
        else {
            currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
            if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
        }
    }
    else if (currentGameType == GameType::NINE_BALL) {
        UpdateLowestBall();
        if (b->id == 9) {
            FinalizeGame(currentPlayer, (currentPlayer == 1) ? 2 : 1, L"(Cheat Mode 9-Ball)");
        }
        else {
            currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
            if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
        }
    }
}

// --- MODIFIED HELPER FOR STRAIGHT POOL RE-RACK ---
void ReRackStraightPool() {
    // --- Define Rack and Spot Positions ---
    float spacingX = BALL_RADIUS * 2.0f * 0.866f;
    float spacingY = BALL_RADIUS * 2.0f * 1.0f;
    D2D1_POINT_2F rackPositions[15]; // Positions 0 to 14
    int rackIndex = 0;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col <= row; ++col) {
            if (rackIndex >= 15) break;
            float x = RACK_POS_X + row * spacingX;
            float y = RACK_POS_Y + (col - row / 2.0f) * spacingY;
            rackPositions[rackIndex++] = D2D1::Point2F(x, y);
        }
    }
    const D2D1_POINT_2F headSpot = rackPositions[0]; // Apex position
    const float collisionThresholdSq = (BALL_RADIUS * 2.0f) * (BALL_RADIUS * 2.0f); // Squared distance for overlap check

    // --- Find the last object ball and the cue ball ---
    Ball* lastBall = nullptr;
    Ball* cueBall = GetCueBall(); // Get cue ball pointer

    for (Ball& b : balls) {
        if (b.id != 0 && !b.isPocketed) {
            lastBall = &b;
            break; // Found the single remaining object ball
        }
    }

    // Safety checks - should not happen if called correctly when 1 ball is left
    if (!lastBall || !cueBall) {
        // Error condition or unexpected state, maybe just reset game?
        // For now, just return to avoid crashing.
        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
        return;
    }

    bool lastBallMoved = false;
    bool cueBallMoved = false;

    // --- Edge Case 1: Check if lastBall interferes with the rack area (spots 1-14) ---
    for (int i = 1; i <= 14; ++i) { // Check spots where balls will be placed
        if (GetDistanceSq(lastBall->x, lastBall->y, rackPositions[i].x, rackPositions[i].y) < collisionThresholdSq) {
            // Obstruction found! Move lastBall to the headSpot.
            lastBall->x = headSpot.x;
            lastBall->y = headSpot.y;
            lastBall->vx = 0;
            lastBall->vy = 0;
            lastBallMoved = true;
            break; // No need to check further spots for lastBall
        }
    }

    // --- Edge Case 2: Check if cueBall interferes with ANY rack spot (0-14) ---
    // This check includes the headSpot where lastBall might have just been moved.
    bool cueObstructs = false;
    for (int i = 0; i <= 14; ++i) { // Check ALL spots including the head spot
        if (GetDistanceSq(cueBall->x, cueBall->y, rackPositions[i].x, rackPositions[i].y) < collisionThresholdSq) {
            cueObstructs = true;
            break;
        }
    }

    if (cueObstructs) {
        // Cue ball obstructs. Move it behind the headstring (kitchen) and give ball-in-hand.

        // Find a valid spot in the kitchen (reuse Respawn logic concept but simpler)
        float targetX = TABLE_LEFT + (HEADSTRING_X - TABLE_LEFT) * 0.5f;
        float targetY = TABLE_TOP + TABLE_HEIGHT / 2.0f;
        int attempts = 0;
        bool positionFound = false;

        // Check against lastBall's potential new position as well
        while (attempts < 50) {
            // Check overlap with lastBall only
            if (GetDistanceSq(targetX, targetY, lastBall->x, lastBall->y) >= collisionThresholdSq &&
                targetX < HEADSTRING_X - BALL_RADIUS) // Ensure it's behind headstring
            {
                positionFound = true;
                break;
            }
            // Try nudging
            targetX += (static_cast<float>(rand() % 100 - 50) / 100.0f) * BALL_RADIUS * 1.5f;
            targetY += (static_cast<float>(rand() % 100 - 50) / 100.0f) * BALL_RADIUS * 1.5f;
            // Clamp within kitchen bounds
            targetX = std::max(TABLE_LEFT + BALL_RADIUS, std::min(targetX, HEADSTRING_X - BALL_RADIUS));
            targetY = std::max(TABLE_TOP + BALL_RADIUS, std::min(targetY, TABLE_BOTTOM - BALL_RADIUS));
            attempts++;
        }
        // Fallback if nudging fails
        if (!positionFound) {
            targetX = TABLE_LEFT + BALL_RADIUS * 2.0f;
            targetY = RACK_POS_Y;
        }


        cueBall->x = targetX;
        cueBall->y = targetY;
        cueBall->vx = 0;
        cueBall->vy = 0;
        cueBallMoved = true;

        // Set state to Ball-in-Hand for the current player
        if (currentPlayer == 1) {
            currentGameState = BALL_IN_HAND_P1;
            aiTurnPending = false;
        }
        else { // Player 2
            currentGameState = BALL_IN_HAND_P2;
            aiTurnPending = isPlayer2AI; // Trigger AI placement if needed
        }
    }

    // --- Place the 14 pocketed balls back in the rack ---
    // Place them in spots 1 to 14, leaving the headSpot (index 0) open,
    // unless lastBall was moved there.
    int placedCount = 0;
    for (Ball& b : balls) {
        // Place if pocketed AND it's not the lastBall (even if lastBall was moved)
        if (b.id != 0 && b.isPocketed && (&b != lastBall) && placedCount < 14) {
            // Find next available rack spot (skip index 0 - the head spot)
            int targetRackIndex = placedCount + 1; // Start placing from index 1

            b.x = rackPositions[targetRackIndex].x;
            b.y = rackPositions[targetRackIndex].y;
            b.vx = 0;
            b.vy = 0;
            b.isPocketed = false;
            placedCount++;
        }
    }
    ballsOnTableCount = 15; // Reset count to 15 (14 placed + lastBall)

    // [+] NEW: Clear history because balls are back on table.
    // You cannot "undo" a ball that has been officially re-racked.
    g_pocketHistory.clear();
    //g_debugActionLock.store(false);

    // --- Set Game State for Continued Play (unless cue was moved) ---
    if (!cueBallMoved) {
        // If cue ball wasn't moved, the current player continues their turn normally.
        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
    }
    // Else: State was already set to BALL_IN_HAND above.

    // --- Play Re-Rack Sound ---
    if (!g_soundEffectsMuted) {
        PlayGameSound(g_soundBufferRack);
        //PlaySound(TEXT("rack.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

// Flatten one cubic bezier into a polyline (uniform sampling).
static void FlattenCubicBezier(const std::array<D2D1_POINT_2F, 4>& bz, std::vector<D2D1_POINT_2F>& outPts, int steps = 16)
{
    outPts.clear();
    outPts.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float mt = 1.0f - t;
        // De Casteljau cubic
        float x =
            mt * mt * mt * bz[0].x +
            3.0f * mt * mt * t * bz[1].x +
            3.0f * mt * t * t * bz[2].x +
            t * t * t * bz[3].x;
        float y =
            mt * mt * mt * bz[0].y +
            3.0f * mt * mt * t * bz[1].y +
            3.0f * mt * t * t * bz[2].y +
            t * t * t * bz[3].y;
        outPts.push_back(D2D1::Point2F(x, y));
    }
}

// squared distance point -> segment
static inline float PointSegDistSq(const D2D1_POINT_2F& p, const D2D1_POINT_2F& a, const D2D1_POINT_2F& b)
{
    float vx = b.x - a.x, vy = b.y - a.y;
    float wx = p.x - a.x, wy = p.y - a.y;
    float c1 = vx * wx + vy * wy;
    if (c1 <= 0.0f) {
        float dx = p.x - a.x, dy = p.y - a.y;
        return dx * dx + dy * dy;
    }
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        float dx = p.x - b.x, dy = p.y - b.y;
        return dx * dx + dy * dy;
    }
    float t = c1 / c2;
    float projx = a.x + vx * t;
    float projy = a.y + vy * t;
    float dx = p.x - projx, dy = p.y - projy;
    return dx * dx + dy * dy;
}

// Point-in-polygon (winding rule). flattened polyline is assumed closed.
static bool PointInPolygonWinding(const D2D1_POINT_2F& pt, const std::vector<D2D1_POINT_2F>& poly)
{
    int wn = 0;
    size_t n = poly.size();
    if (n < 3) return false;
    for (size_t i = 0; i < n; ++i) {
        const D2D1_POINT_2F& a = poly[i];
        const D2D1_POINT_2F& b = poly[(i + 1) % n];
        if (a.y <= pt.y) {
            if (b.y > pt.y) { // upward crossing
                float isLeft = (b.x - a.x) * (pt.y - a.y) - (pt.x - a.x) * (b.y - a.y);
                if (isLeft > 0.0f) ++wn;
            }
        }
        else {
            if (b.y <= pt.y) { // downward crossing
                float isLeft = (b.x - a.x) * (pt.y - a.y) - (pt.x - a.x) * (b.y - a.y);
                if (isLeft < 0.0f) --wn;
            }
        }
    }
    return wn != 0;
}

// Add a circular arc (possibly >90deg) approximated by cubic beziers.
// center, radius, startAngle (radians), sweepAngle (radians). Sweep may be > PI/2,
// so we subdivide into <= PI/2 segments.
static void AddArcAsCubics(std::vector<std::array<D2D1_POINT_2F, 4>>& out, D2D1_POINT_2F center, float radius, float startAngle, float sweep)
{
    // normalize sweep sign and break into pieces of max 90 degrees
    const float MAXSEG = PI / 2.0f; // 90deg
    int segs = (int)ceilf(fabsf(sweep) / MAXSEG);
    float segAngle = sweep / segs;
    float a0 = startAngle;
    for (int i = 0; i < segs; ++i) {
        float a1 = a0 + segAngle;
        float theta = a1 - a0;
        float alpha = (4.0f / 3.0f) * tanf(theta / 4.0f); // control length factor
        D2D1_POINT_2F p0 = { center.x + radius * cosf(a0), center.y + radius * sinf(a0) };
        D2D1_POINT_2F p3 = { center.x + radius * cosf(a1), center.y + radius * sinf(a1) };
        D2D1_POINT_2F t0 = { -sinf(a0), cosf(a0) }; // unit tangent at p0
        D2D1_POINT_2F t1 = { -sinf(a1), cosf(a1) }; // unit tangent at p3
        D2D1_POINT_2F p1 = { p0.x + alpha * radius * t0.x, p0.y + alpha * radius * t0.y };
        D2D1_POINT_2F p2 = { p3.x - alpha * radius * t1.x, p3.y - alpha * radius * t1.y };
        out.push_back({ p0, p1, p2, p3 });
        a0 = a1;
    }
}

// Build the pocket black-area bezier description (semicircles for middles,
// quarter/rounded for corners). Caches results in g_pocketBezierCache.
static void EnsurePocketBezierCache()
{
    if (g_pocketBezierCacheInitialized) return;
    g_pocketBezierCacheInitialized = true;
    g_pocketBezierCache.clear();
    g_pocketBezierCache.resize(6);

    for (int p = 0; p < 6; ++p) {
        D2D1_POINT_2F c = pocketPositions[p];
        float pocketVis = GetPocketVisualRadius(p); // uses your existing helper
        float inner = pocketVis * 0.88f; // base for the 'black' mouth. tweak if needed

        // decide arc angles (y increases downward)
        // Top-left (0): quarter arc from angle 0 (right) to PI/2 (down)
        // Top-middle (1): semicircle from angle 0 (right) to PI (left) - lower half
        // Top-right (2): quarter arc from PI/2 (down) to PI (left)
        // Bottom-left (3): quarter arc from 3PI/2 (up) to 2PI (right)
        // Bottom-middle (4): semicircle from PI (left) to 2PI (right) - upper half
        // Bottom-right (5): quarter arc from PI (left) to 3PI/2 (up)
        const float TWO_PI = 2.0f * PI;
        if (p == 1) {
            // top middle -> semicircle opening downward (angles 0 -> PI)
            AddArcAsCubics(g_pocketBezierCache[p], c, inner, 0.0f, PI);
        }
        else if (p == 4) {
            // bottom middle -> semicircle opening upward (angles PI -> 2PI)
            AddArcAsCubics(g_pocketBezierCache[p], c, inner, PI, PI);
        }
        else if (p == 0) {
            // top-left quarter: right -> down (0 -> PI/2)
            AddArcAsCubics(g_pocketBezierCache[p], c, inner, 0.0f, PI / 2.0f);
        }
        else if (p == 2) {
            // top-right quarter: down -> left (PI/2 -> PI)
            AddArcAsCubics(g_pocketBezierCache[p], c, inner, PI / 2.0f, PI / 2.0f);
        }
        else if (p == 3) {
            // bottom-left quarter: up -> right (3PI/2 -> 2PI)
            AddArcAsCubics(g_pocketBezierCache[p], c, inner, 3.0f * PI / 2.0f, PI / 2.0f);
        }
        else if (p == 5) {
            // bottom-right quarter: left -> up (PI -> 3PI/2)
            AddArcAsCubics(g_pocketBezierCache[p], c, inner, PI, PI / 2.0f);
        }
    }
}
//above are bezier mask helpers

//below are pixel-perfect bezier matching helpers


// -------------------------------------------------------------------------
// Geometry helpers for precise pocket shape hit-testing
// -------------------------------------------------------------------------
static float DistPointToSegment(const D2D1_POINT_2F& p, const D2D1_POINT_2F& a, const D2D1_POINT_2F& b)
{
    // squared distance project of p onto segment ab
    D2D1_POINT_2F ab = { b.x - a.x, b.y - a.y };
    D2D1_POINT_2F ap = { p.x - a.x, p.y - a.y };
    float ab2 = ab.x * ab.x + ab.y * ab.y;
    if (ab2 <= 1e-8f) {
        float dx = p.x - a.x, dy = p.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }
    float t = (ap.x * ab.x + ap.y * ab.y) / ab2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    D2D1_POINT_2F proj = { a.x + ab.x * t, a.y + ab.y * t };
    float dx = p.x - proj.x, dy = p.y - proj.y;
    return sqrtf(dx * dx + dy * dy);
}

static float NormalizeAngle(float a)
{
    // normalize to [0, 2PI)
    const float TWO_PI = 2.0f * PI;
    while (a < 0.0f) a += TWO_PI;
    while (a >= TWO_PI) a -= TWO_PI;
    return a;
}

// return true if angle `ang` is inside sector starting at startAngle
// sweeping `sweep` (sweep >= 0). Handles wrap-around.
static bool AngleInRange(float ang, float startAngle, float sweep)
{
    ang = NormalizeAngle(ang);
    startAngle = NormalizeAngle(startAngle);
    float endAngle = NormalizeAngle(startAngle + sweep);
    if (sweep >= 2.0f * PI - 1e-6f) return true;
    if (startAngle <= endAngle) {
        return (ang >= startAngle - 1e-6f && ang <= endAngle + 1e-6f);
    }
    else {
        // wrapped around
        return (ang >= startAngle - 1e-6f || ang <= endAngle + 1e-6f);
    }
}

// minimal distance from point `p` to a circular arc (centered at c, radius r,
// starting at startAngle for sweepAngle radians). Also returns if point is
// inside the filled sector (i.e., r0 <= r and angle in sector).
static float DistPointToArc(const D2D1_POINT_2F& p, const D2D1_POINT_2F& c, float r, float startAngle, float sweepAngle, bool& outInsideSector)
{
    float dx = p.x - c.x;
    float dy = p.y - c.y;
    float r0 = sqrtf(dx * dx + dy * dy);
    float ang = atan2f(dy, dx);
    ang = NormalizeAngle(ang);

    // endpoints of the arc
    float a1 = NormalizeAngle(startAngle);
    float a2 = NormalizeAngle(startAngle + sweepAngle);

    // is the point angle within sector?
    bool inSector = AngleInRange(ang, a1, sweepAngle);
    outInsideSector = (inSector && (r0 <= r + 1e-6f));

    if (inSector) {
        // distance to arc circle
        return fabsf(r0 - r);
    }
    else {
        // distance to arc endpoints
        D2D1_POINT_2F ep1 = { c.x + r * cosf(a1), c.y + r * sinf(a1) };
        D2D1_POINT_2F ep2 = { c.x + r * cosf(a2), c.y + r * sinf(a2) };
        float dx1 = p.x - ep1.x, dy1 = p.y - ep1.y;
        float dx2 = p.x - ep2.x, dy2 = p.y - ep2.y;
        float d1 = sqrtf(dx1 * dx1 + dy1 * dy1);
        float d2 = sqrtf(dx2 * dx2 + dy2 * dy2);
        return (d1 < d2) ? d1 : d2;
    }
}

// -------------------------------------------------------------------------
// High-precision pocket test: returns true when any part of the BALL
// overlaps the black pocket area (semicircle for middle pockets or
// corner pocket interior). Uses BALL_RADIUS so touching counts.
// Insert this function in the Helpers section (near other helpers).
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// Precise test whether any part of the ball overlaps the black pocket shape
// (quarter/semicircle filled area). This function *replaces* the older loose
// radius/capsule approach and matches the actual black mouth shape.
// -------------------------------------------------------------------------
bool IsBallInPocketBlackArea(const Ball& b, int pIndex)
{
    if (pIndex < 0 || pIndex > 5) return false;

    D2D1_POINT_2F pc = pocketPositions[pIndex];
    // Use pocket visual radius converted to a conservative black-mouth radius
    float pocketVis = GetPocketVisualRadius(pIndex);
    // `blackRadius` controls how big the black area is relative to visual.
    // Reduce if you want more strictness.
    const float blackRadius = pocketVis * 0.55f; //0.88f(default) => 75 =>65=>55    

    // For top middle (1): semicircle opening downward (angles 0..PI),
    // for bottom middle (4): semicircle opening upward (angles PI..2PI).
    // For corners, quarter-circle angles depend on pocket index:
    // 0 (top-left): angles 0..PI/2  (right -> down)
    // 2 (top-right): angles PI/2..PI (down -> left)
    // 3 (bottom-left): angles 3PI/2..2PI (up -> right)
    // 5 (bottom-right): angles PI..3PI/2 (left -> up)

    D2D1_POINT_2F ballCenter = { b.x, b.y };

    // --- Middle pockets: semicircle + diameter segment ---
    if (pIndex == 1 || pIndex == 4) {
        // semicircle params
        float startAngle, sweep;
        if (pIndex == 1) {
            // top middle: arc from angle 0 (right) to pi (left) -> opening downward into table
            startAngle = 0.0f;
            sweep = PI;
            // require ball to be on table side (below center) when checking "inside"
            // we handle this orientation implicitly with the semicircle sector and diameter.
        }
        else {
            // bottom middle: arc from PI to 2PI (opening upward)
            startAngle = PI;
            sweep = PI;
        }

        // 1) If ball center is inside semicircular sector (r0 <= blackRadius and angle in sector)
        bool insideSector = false;
        float dx = ballCenter.x - pc.x;
        float dy = ballCenter.y - pc.y;
        float r0 = sqrtf(dx * dx + dy * dy);
        float ang = NormalizeAngle(atan2f(dy, dx));
        if (AngleInRange(ang, startAngle, sweep) && r0 <= blackRadius + 1e-6f) {
            // ball center lies strictly inside semicircle -> definitely overlaps
            return true;
        }

        // 2) if not, compute minimal distance to the semicircle filled shape boundary:
        //    boundary = arc (circular curve) + diameter segment (straight line between arc endpoints)
        bool tmpInsideArcSector = false;
        float distToArc = DistPointToArc(ballCenter, pc, blackRadius, startAngle, sweep, tmpInsideArcSector);
        // diameter endpoints
        D2D1_POINT_2F ep1 = { pc.x + blackRadius * cosf(startAngle), pc.y + blackRadius * sinf(startAngle) };
        D2D1_POINT_2F ep2 = { pc.x + blackRadius * cosf(startAngle + sweep), pc.y + blackRadius * sinf(startAngle + sweep) };
        float distToDiameter = DistPointToSegment(ballCenter, ep1, ep2);

        float minDist = (distToArc < distToDiameter) ? distToArc : distToDiameter;

        // If any part of the ball (circle radius) reaches into the filled semicircle -> collision
        return (minDist <= BALL_RADIUS + 1e-6f);
    }
    else {
        // --- Corner pockets: treat as filled quarter-circles ---
        float startAngle = 0.0f;
        // assign angles for each corner pocket (w/ y increasing downwards)
        if (pIndex == 0) {             // top-left: right->down (0 -> PI/2)
            startAngle = 0.0f;
        }
        else if (pIndex == 2) {      // top-right: down->left (PI/2 -> PI)
            startAngle = 0.5f * PI;
        }
        else if (pIndex == 5) {      // bottom-right: left->up (PI -> 3PI/2)
            startAngle = PI;
        }
        else if (pIndex == 3) {      // bottom-left: up->right (3PI/2 -> 2PI)
            startAngle = 1.5f * PI;
        }
        float sweep = 0.5f * PI; // 90 degrees

        // center-inside test (is ball center inside the quarter sector)
        float dx = ballCenter.x - pc.x;
        float dy = ballCenter.y - pc.y;
        float r0 = sqrtf(dx * dx + dy * dy);
        float ang = NormalizeAngle(atan2f(dy, dx));
        if (AngleInRange(ang, startAngle, sweep) && r0 <= blackRadius + 1e-6f) {
            return true;
        }

        // Otherwise compute minimal distance to arc OR to the two radial segments (center->endpoint)
        bool tmpInsideArcSector = false;
        float distToArc = DistPointToArc(ballCenter, pc, blackRadius, startAngle, sweep, tmpInsideArcSector);

        // radial endpoints
        D2D1_POINT_2F ep1 = { pc.x + blackRadius * cosf(startAngle), pc.y + blackRadius * sinf(startAngle) };
        D2D1_POINT_2F ep2 = { pc.x + blackRadius * cosf(startAngle + sweep), pc.y + blackRadius * sinf(startAngle + sweep) };
        float distToRadial1 = DistPointToSegment(ballCenter, pc, ep1);
        float distToRadial2 = DistPointToSegment(ballCenter, pc, ep2);

        float minDist = distToArc;
        if (distToRadial1 < minDist) minDist = distToRadial1;
        if (distToRadial2 < minDist) minDist = distToRadial2;

        return (minDist <= BALL_RADIUS + 1e-6f);
    }

    // fallback
    return false;
}
/*
// Backwards-compatible alias: some places in the code call IsBallInPocket(...)
// so provide a thin wrapper that uses the precise black-area test.
bool IsBallInPocket(const Ball& b, int pIndex)
{
    return IsBallInPocketBlackArea(b, pIndex);
}

// Return true if the ball circle intersects the pocket black area.
// Middle pockets (indexes 1 and 4) use a horizontal capsule; corners use a circle.
bool IsBallInPocket(const Ball* ball, int p)
{
    if (!ball) return false;
    D2D1_POINT_2F center = pocketPositions[p];

    // Get the visual radius appropriate for this pocket (corner vs middle)
    const float pocketVis = GetPocketVisualRadius(p);
    // black mouth visual radius (tweak these multipliers to get the feel you want)
    const float blackRadius = pocketVis * 0.9f;
    // When testing collision of a ball's **edge**, include BALL_RADIUS so the test is between centers:
    const float testRadius = blackRadius + BALL_RADIUS;

    // Middle pockets: treat as a horizontal capsule (line segment + circular end-caps)
    if (p == 1 || p == 4) {
        const float mouthHalfLength = pocketVis * 1.2f; // half-length of opening
        D2D1_POINT_2F a = { center.x - mouthHalfLength, center.y };
        D2D1_POINT_2F b = { center.x + mouthHalfLength, center.y };

        D2D1_POINT_2F ab = { b.x - a.x, b.y - a.y };
        D2D1_POINT_2F ap = { ball->x - a.x, ball->y - a.y };
        float abLen2 = ab.x * ab.x + ab.y * ab.y;
        if (abLen2 <= 1e-6f) { // degenerate fallback to circle
            float dx = ball->x - center.x;
            float dy = ball->y - center.y;
            return (dx * dx + dy * dy) <= (testRadius * testRadius);
        }

        float t = Dot(ap, ab) / abLen2;
        t = Clampf(t, 0.0f, 1.0f);
        D2D1_POINT_2F closest = { a.x + ab.x * t, a.y + ab.y * t };
        float dx = ball->x - closest.x;
        float dy = ball->y - closest.y;
        float dist2 = dx * dx + dy * dy;

        // Ensure ball is coming from the table side (not from outside)
        const float verticalTolerance = BALL_RADIUS * 0.35f;
        bool tableSideOk = true;
        if (p == 1) { // top middle; table interior is below center
            if (ball->y + verticalTolerance < center.y) tableSideOk = false;
        }
        else { // p == 4 bottom middle; table interior is above center
            if (ball->y - verticalTolerance > center.y) tableSideOk = false;
        }

        return tableSideOk && (dist2 <= (testRadius * testRadius));
    }
    else {
        // Corner pocket: simple circle test around pocket center (black mouth)
        float dx = ball->x - center.x;
        float dy = ball->y - center.y;
        float dist2 = dx * dx + dy * dy;
        return (dist2 <= (testRadius * testRadius));
    }
}
*/

// --- NEW HELPER: Finds the first rail intersection for a trajectory line ---
bool FindFirstRailIntersection(D2D1_POINT_2F start, D2D1_POINT_2F end, D2D1_POINT_2F& outIntersection) {
    float minHitDistSq = -1.0f;
    bool foundHit = false;

    // Use the same rail segments from CheckCollisions/IsPathClear for perfect consistency
    D2D1_POINT_2F vertices[12] = {
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE }, { TABLE_LEFT + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS },
        { TABLE_RIGHT - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE }, { TABLE_RIGHT - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS },
        { TABLE_LEFT + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE },
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2 }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2 },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2 }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2 }
    };
    D2D1_POINT_2F segments[12][2] = {
        { vertices[1], vertices[2] }, { vertices[2], vertices[3] },
        { vertices[3], vertices[11] }, { vertices[11], vertices[10] },
        { vertices[10], vertices[4] }, { vertices[4], vertices[5] },
        { vertices[5], vertices[6] }, { vertices[6], vertices[7] },
        { vertices[7], vertices[9] }, { vertices[9], vertices[8] },
        { vertices[8], vertices[0] }, { vertices[0], vertices[1] }
    };

    D2D1_POINT_2F currentIntersection;
    for (int j = 0; j < 12; ++j) {
        if (LineSegmentIntersection(start, end, segments[j][0], segments[j][1], currentIntersection)) {
            float distSq = GetDistanceSq(start.x, start.y, currentIntersection.x, currentIntersection.y);
            if (!foundHit || distSq < minHitDistSq) {
                minHitDistSq = distSq;
                outIntersection = currentIntersection;
                foundHit = true;
            }
        }
    }
    return foundHit;
}
// --- NEW Forward Declarations ---

// AI Related
struct AIShotInfo; // Define below
AIShotInfo AIFindBestShot_8Ball(); // Add if you renamed it
AIShotInfo AIFindBestShot_StraightPool(); // Add this too
AIShotInfo AIFindBestShot_NineBall(); // NEW 9-Ball AI
void TriggerAIMove();
void AIMakeDecision();
void AIPlaceCueBall();
//AIShotInfo AIFindBestShot();
void AIFindBankShots(Ball* targetBall, std::vector<AIShotInfo>& possibleShots); // New
AIShotInfo EvaluateShot_8Ball(Ball* targetBall, int pocketIndex, bool isBank);
//AIShotInfo EvaluateShot(Ball* targetBall, int pocketIndex);
AIShotInfo EvaluateShot_StraightPool(Ball* targetBall, int pocketIndex, bool isBank);
AIShotInfo EvaluateShot_NineBall(Ball* targetBall, int pocketIndex, bool isBank, bool isCombo); // NEW
float EvaluateTableState_StraightPool(Ball* cueBall);
bool IsPathClear(D2D1_POINT_2F start, D2D1_POINT_2F end, int ignoredBallId1, int ignoredBallId2);
Ball* FindFirstHitBall(D2D1_POINT_2F start, float angle, float& hitDistSq); // Added hitDistSq output
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist, bool isBank); // Corrected
//float CalculateShotPower(float cueToGhostDist, float targetToPocketDist);
D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, D2D1_POINT_2F pocketPos);
//D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, int pocketIndex);
bool IsValidAIAimAngle(float angle); // Basic check
bool FindFirstRailIntersection(D2D1_POINT_2F start, D2D1_POINT_2F end, D2D1_POINT_2F& outIntersection); // Add this line

// Dialog Related
INT_PTR CALLBACK NewGameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ShowNewGameDialog(HINSTANCE hInstance);
void LoadSettings(); // For deserialization
void SaveSettings(); // For serialization
const std::wstring SETTINGS_FILE_NAME = L"Pool-Settings.txt";
void ResetGame(HINSTANCE hInstance); // Function to handle F2 reset
void UpdateWindowTitle();            // <<< ADD THIS LINE

// --- Forward Declaration for Window Procedure --- <<< Add this line HERE
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- NEW Struct for AI Shot Evaluation ---
struct AIShotInfo {
    bool possible = false;          // Is this shot considered viable?
    Ball* targetBall = nullptr;     // Which ball to hit
    int pocketIndex = -1;           // Which pocket to aim for (0-5)
    D2D1_POINT_2F ghostBallPos = { 0,0 }; // Where cue ball needs to hit target ball
    D2D1_POINT_2F predictedCueEndPos = { 0,0 }; // For positional play
    float angle = 0.0f;             // Calculated shot angle
    float power = 0.0f;             // Calculated shot power
    float score = -9999.0f; // Use a very low initial score
    //float score = -1.0f;            // Score for this shot (higher is better)
    //bool involves8Ball = false;     // Is the target the 8-ball?
    float spinX = 0.0f;
    float spinY = 0.0f;
    bool isBankShot = false;
    bool involves8Ball = false;
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
    std::wofstream outFile(SETTINGS_FILE_NAME);
    outFile.imbue(std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>));
    if (outFile.is_open()) {
        outFile << static_cast<int>(currentGameType) << std::endl; // Save Game Type
        outFile << static_cast<int>(gameMode) << std::endl;
        outFile << static_cast<int>(aiDifficulty) << std::endl;
        outFile << static_cast<int>(openingBreakMode) << std::endl;
        outFile << static_cast<int>(selectedTableColor) << std::endl;
        outFile << targetScoreStraightPool << std::endl; // Save Straight Pool target score
        // ... (save player names) ...
        std::wstring p1_safe = g_player1Name; std::replace(p1_safe.begin(), p1_safe.end(), L' ', L'_');
        std::wstring p2_safe = g_player2Name; std::replace(p2_safe.begin(), p2_safe.end(), L' ', L'_');
        std::wstring ai_safe = g_aiName; std::replace(ai_safe.begin(), ai_safe.end(), L' ', L'_');
        outFile << p1_safe.c_str() << std::endl;
        outFile << p2_safe.c_str() << std::endl;
        outFile << ai_safe.c_str() << std::endl;
        // ... (save player statistics) ...
        outFile << g_stats.p1_wins << std::endl;
        outFile << g_stats.p1_losses << std::endl;
        outFile << g_stats.ai_wins << std::endl;
        outFile << g_stats.human_vs_human_games << std::endl;
        outFile << g_isPracticeMode << std::endl; // Save Practice Mode state
        outFile.close();
    }
}

void LoadSettings() {
    std::wifstream inFile(SETTINGS_FILE_NAME);
    inFile.imbue(std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>));
    if (inFile.is_open()) {
        int gt; // Game Type
        if (inFile >> gt) { // Load Game Type first
            if (gt >= GameType::EIGHT_BALL_MODE && gt <= GameType::STRAIGHT_POOL) currentGameType = static_cast<GameType>(gt); // CORRECTED (covers 9-ball)
            else currentGameType = GameType::EIGHT_BALL_MODE; // Default if invalid // CORRECTED
        }
        int gm, aid, obm, tc; // Other settings
        if (inFile >> gm) gameMode = static_cast<GameMode>(gm);
        if (inFile >> aid) aiDifficulty = static_cast<AIDifficulty>(aid);
        if (inFile >> obm) openingBreakMode = static_cast<OpeningBreakMode>(obm);
        if (inFile >> tc) selectedTableColor = static_cast<TableColor>(tc);
        if (inFile >> targetScoreStraightPool) { // Load Straight Pool target score
            if (targetScoreStraightPool < 1 || targetScoreStraightPool > 999) targetScoreStraightPool = 25; // Validate
        }
        else {
            targetScoreStraightPool = 25; // Default if not in file
        }
        // ... (load player names) ...
        std::wstring p1_safe, p2_safe, ai_safe;
        if (inFile >> p1_safe) { std::replace(p1_safe.begin(), p1_safe.end(), L'_', L' '); g_player1Name = p1_safe; }
        if (inFile >> p2_safe) { std::replace(p2_safe.begin(), p2_safe.end(), L'_', L' '); g_player2Name = p2_safe; }
        if (inFile >> ai_safe) { std::replace(ai_safe.begin(), ai_safe.end(), L'_', L' '); g_aiName = ai_safe; }
        // ... (load player statistics) ...
        inFile >> g_stats.p1_wins >> g_stats.p1_losses >> g_stats.ai_wins >> g_stats.human_vs_human_games;
        inFile >> g_isPracticeMode;
        if (inFile.fail()) g_isPracticeMode = false; // Default if loading fails
        inFile.close();
        // Validate loaded settings (Keep existing validation)
        if (gameMode < HUMAN_VS_HUMAN || gameMode > HUMAN_VS_AI) gameMode = HUMAN_VS_HUMAN;
        if (aiDifficulty < EASY || aiDifficulty > HARD) aiDifficulty = MEDIUM;
        if (openingBreakMode < CPU_BREAK || openingBreakMode > FLIP_COIN_BREAK) openingBreakMode = CPU_BREAK;
        if (selectedTableColor < INDIGO || selectedTableColor > RED) selectedTableColor = INDIGO;
        if (targetScoreStraightPool < 1 || targetScoreStraightPool > 999) targetScoreStraightPool = 25;
    }
    else {
        // File doesn't exist, set default score
        targetScoreStraightPool = 25;
    }
}
// --- End Settings Serialization Functions ---

 // --- NEW Dialog Procedure for Editing Player Names ---
INT_PTR CALLBACK EditNamesDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        // Populate the text fields with the current global names
        SetDlgItemText(hDlg, IDC_EDIT_P1_NAME, g_player1Name.c_str());
        SetDlgItemText(hDlg, IDC_EDIT_P2_NAME, g_player2Name.c_str());
        SetDlgItemText(hDlg, IDC_EDIT_AI_NAME, g_aiName.c_str());
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case IDOK:
        {
            // Retrieve the new names from the text fields and update the global variables
            wchar_t buffer[256];
            GetDlgItemText(hDlg, IDC_EDIT_P1_NAME, buffer, 256);
            g_player1Name = buffer;
            GetDlgItemText(hDlg, IDC_EDIT_P2_NAME, buffer, 256);
            g_player2Name = buffer;
            GetDlgItemText(hDlg, IDC_EDIT_AI_NAME, buffer, 256);
            g_aiName = buffer;

            // If names are empty, reset to default
            if (g_player1Name.empty()) g_player1Name = L"Anti-Deluvian™";
            if (g_player2Name.empty()) g_player2Name = L"NetBurst-Hex™";
            if (g_aiName.empty()) g_aiName = L"Virtus Pro™";

            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
    }
    break;
    }
    return (INT_PTR)FALSE;
}

// --- NEW Dialog Procedure ---
INT_PTR CALLBACK NewGameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    // Helper lambda to update the statistics text fields
    auto UpdateStatsDisplay = [hDlg]() {
        wchar_t buffer[128];
        // P1 vs AI stats
        swprintf_s(buffer, L"P1: %d W / %d L (vs AI)", g_stats.p1_wins, g_stats.p1_losses);
        SetDlgItemText(hDlg, IDC_STATS_P1, buffer);
        // P2 (Human vs Human) stats
        swprintf_s(buffer, L"2P: %d Games Played", g_stats.human_vs_human_games);
        SetDlgItemText(hDlg, IDC_STATS_2P, buffer);
        // AI vs P1 stats
        swprintf_s(buffer, L"AI: %d W / %d L (vs P1)", g_stats.ai_wins, g_stats.p1_wins);
        SetDlgItemText(hDlg, IDC_STATS_AI, buffer);
    };

    // --- NEW: Helper lambda to enable/disable controls based on game mode ---
    auto UpdateControlStates = [hDlg]() {
        int selectedGameModeIndex = (int)SendMessage(GetDlgItem(hDlg, IDC_COMBO_GAMEMODE), CB_GETCURSEL, 0, 0);
        // CORRECTED for 3 modes: 0=8Ball, 1=9Ball, 2=StraightPool
        GameType selectedGameType = static_cast<GameType>(selectedGameModeIndex);
        bool isCPUGame = IsDlgButtonChecked(hDlg, IDC_RADIO_CPU) == BST_CHECKED;
        bool isPractice = IsDlgButtonChecked(hDlg, IDC_CHECK_PRACTICE) == BST_CHECKED;

        // --- NEW: Enable/Disable Straight Pool Score Controls ---
        // --- MODIFIED: Enable/Disable Straight Pool Score Controls (Only for Straight Pool) ---
        bool enableStraightPoolOptions = (selectedGameType == GameType::STRAIGHT_POOL);
        EnableWindow(GetDlgItem(hDlg, IDC_STATIC_BALLS_WIN), enableStraightPoolOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_BALLS_WIN), enableStraightPoolOptions);

        // --- MODIFIED: Disable Player Type based on Practice Mode ---
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_2P), !isPractice);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_CPU), !isPractice);

        // Enable/Disable AI group and Break Mode group based on Player Type AND Game Mode
        bool enableAIOptions = isCPUGame && !isPractice; // AI options enabled only if Human vs CPU and NOT practice
        EnableWindow(GetDlgItem(hDlg, IDC_GROUP_AI), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_EASY), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_MEDIUM), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_HARD), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_GROUP_BREAK_MODE), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_CPU_BREAK), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_P1_BREAK), enableAIOptions);
        EnableWindow(GetDlgItem(hDlg, IDC_RADIO_FLIP_BREAK), enableAIOptions);

        // Straight Pool doesn't use AI difficulty related to 8-ball strategy,
        // but we keep the difficulty selection enabled for general AI behavior (like shot selection accuracy).
        // Break modes are also relevant for who breaks.
    };

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

        // [+] NEW: Set the Dialog Icon (Small and Big)
        // This puts the app icon in the top-left corner of the dialog title bar
        HICON hIcon = LoadIcon((HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE), MAKEINTRESOURCE(IDI_ICON1));
        SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

        // --- Populate Game Mode ComboBox ---
        HWND hGameModeCombo = GetDlgItem(hDlg, IDC_COMBO_GAMEMODE);
        SendMessage(hGameModeCombo, CB_ADDSTRING, 0, (LPARAM)gameTypeNames[0]); // 8-Ball
        SendMessage(hGameModeCombo, CB_ADDSTRING, 0, (LPARAM)gameTypeNames[1]); // 9-Ball
        SendMessage(hGameModeCombo, CB_ADDSTRING, 0, (LPARAM)gameTypeNames[2]); // Straight Pool
        SendMessage(hGameModeCombo, CB_SETCURSEL, (WPARAM)currentGameType, 0);   // Set initial selection

        // Set initial state based on current global settings (or defaults)
        CheckRadioButton(hDlg, IDC_RADIO_2P, IDC_RADIO_CPU, (gameMode == HUMAN_VS_HUMAN) ? IDC_RADIO_2P : IDC_RADIO_CPU);

        // Set initial AI difficulty (will be enabled/disabled by UpdateControlStates)
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

        // --- NEW: Populate the Table Color ComboBox ---
        HWND hCombo = GetDlgItem(hDlg, IDC_COMBO_TABLECOLOR);
        for (int i = 0; i < 5; ++i) {
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)tableColorNames[i]);
        }
        // Set the initial selection based on the loaded/default setting
        SendMessage(hCombo, CB_SETCURSEL, (WPARAM)selectedTableColor, 0);

        // --- NEW: Populate the Straight Pool score ---
        SetDlgItemInt(hDlg, IDC_EDIT_BALLS_WIN, targetScoreStraightPool, FALSE);

        // --- NEW: Set initial Practice Mode checkbox state ---
        CheckDlgButton(hDlg, IDC_CHECK_PRACTICE, g_isPracticeMode ? BST_CHECKED : BST_UNCHECKED);

        // --- NEW: Initial display of statistics ---
        UpdateStatsDisplay();

        // --- Initial update of enabled/disabled controls ---
        UpdateControlStates();
    }
    return (INT_PTR)TRUE;

    case WM_COMMAND:
    { // Add an opening brace to create a new scope for the entire case.
        HWND hCombo;
        int selectedIndex;
        switch (LOWORD(wParam)) {
        case IDC_COMBO_GAMEMODE: // Game mode selection changed
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                UpdateControlStates(); // Update enabled states when game mode changes
            }
            return (INT_PTR)TRUE;

        case IDC_RADIO_2P:
        case IDC_RADIO_CPU:
            UpdateControlStates(); // Update enabled states when player type changes
        /* {
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
        }*/
            return (INT_PTR)TRUE;
        case IDC_CHECK_PRACTICE: // Practice mode toggled
            UpdateControlStates(); // Update all control states
            return (INT_PTR)TRUE;
        case IDC_BTN_EDITNAMES:
            // Launch the new "Edit Names" dialog
            DialogBoxParam((HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE), MAKEINTRESOURCE(IDD_EDITNAMESDLG), hDlg, EditNamesDialogProc, 0);
            return (INT_PTR)TRUE;

        case IDC_BTN_RESET_SCORES:
            // Reset the stats, save, and update the display
            if (MessageBox(hDlg, L"Are you sure you want to reset all player statistics?", L"Confirm Reset", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                g_stats = {}; // Zero-initialize the struct
                SaveSettings();
                UpdateStatsDisplay();
            }
            return (INT_PTR)TRUE;

        case IDOK:
            // --- Retrieve Game Mode ---
            selectedIndex = (int)SendMessage(GetDlgItem(hDlg, IDC_COMBO_GAMEMODE), CB_GETCURSEL, 0, 0);
            if (selectedIndex != CB_ERR) {
                currentGameType = static_cast<GameType>(selectedIndex);
            }

            // --- Retrieve Straight Pool Target Score (if mode is Straight Pool) ---
            if (currentGameType == GameType::STRAIGHT_POOL) {
                BOOL bSuccess = FALSE;
                UINT newValue = GetDlgItemInt(hDlg, IDC_EDIT_BALLS_WIN, &bSuccess, FALSE);

                if (bSuccess && newValue >= 1 && newValue <= 999) {
                    targetScoreStraightPool = newValue;
                }
                else {
                    // If invalid input or out of range, reset to default
                    targetScoreStraightPool = 25;
                }
            }
            // (If 8-Ball is selected, we just ignore the value in the box)

            // --- Retrieve Practice Mode ---
            g_isPracticeMode = (IsDlgButtonChecked(hDlg, IDC_CHECK_PRACTICE) == BST_CHECKED);

            // Retrieve Player Type and AI settings
            if (IsDlgButtonChecked(hDlg, IDC_RADIO_CPU) == BST_CHECKED && !g_isPracticeMode) { // Ignore if practice mode
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
            }

            if (g_isPracticeMode) {
                gameMode = HUMAN_VS_HUMAN; // Force 1P mode
                isPlayer2AI = false;
            }

            // --- NEW: Retrieve selected table color ---
            hCombo = GetDlgItem(hDlg, IDC_COMBO_TABLECOLOR);
            selectedIndex = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (selectedIndex != CB_ERR) {
                selectedTableColor = static_cast<TableColor>(selectedIndex);
            }

            SaveSettings(); // Save settings when OK is pressed
            EndDialog(hDlg, IDOK); // Close dialog, return IDOK
            return (INT_PTR)TRUE;

        case IDCANCEL: // Handle Cancel or closing the dialog
            // Optionally, could reload settings here if you want cancel to revert to previously saved state
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
    } // Add a closing brace for the new scope.
    break; // End WM_COMMAND
    }
    return (INT_PTR)FALSE; // Default processing
}

// --- NEW Helper to Show Dialog ---
void ShowNewGameDialog(HINSTANCE hInstance) {
    if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_NEWGAMEDLG), hwndMain, NewGameDialogProc, 0) == IDOK) {
        // User clicked Start, reset game with new settings
        isPlayer2AI = (gameMode == HUMAN_VS_AI);
        player1Info.name = g_player1Name;
        player2Info.name = isPlayer2AI ? g_aiName : g_player2Name;

        UpdateWindowTitle(); // Update window title based on gameMode AND currentGameType
        TABLE_COLOR = tableColorValues[selectedTableColor];

        InitGame(); // Re-initialize game logic & board
        InvalidateRect(hwndMain, NULL, TRUE);
    }
    else {
        // User cancelled dialog
    }
}

// --- NEW Reset Game Function ---
void ResetGame(HINSTANCE hInstance) {
    // Call the helper function to show the dialog and re-init if OK clicked
    ShowNewGameDialog(hInstance);
}

// --- WinMain ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // [+] NEW: Initialize GDI+ variables
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    // Initialize GDI+. If it fails, just proceed without custom images.
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

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

    // --- Apply the selected table color to the global before anything else draws ---
    TABLE_COLOR = tableColorValues[selectedTableColor];

    // Set AI flag based on game mode
    isPlayer2AI = (gameMode == HUMAN_VS_AI);
    player1Info.name = g_player1Name;
    if (isPlayer2AI) {
        player2Info.name = g_aiName;
        //switch (aiDifficulty) {
        //case EASY: player2Info.name = L"Virtus Pro (Easy)"/*"CPU (Easy)"*/; break;
        //case MEDIUM:player2Info.name = L"Virtus Pro (Medium)"/*"CPU (Medium)"*/; break;
        //case HARD: player2Info.name = L"Virtus Pro (Hard)"/*"CPU (Hard)"*/; break;
        //}
    }
    else {
        player2Info.name = g_player2Name;
        //player2Info.name = L"NetBurst-Hex™"/*"Billy Ray Cyrus"*//*"Player 2"*/;
    }
    // --- End of Dialog Logic ---


    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BLISS_GameEngine"/*"Direct2D_8BallPool"*/;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); // Use your actual icon ID here

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Window Registration Failed.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return -1;
    }

    // --- ACTION 4: Calculate Centered Window Position WITH MENU ---
    // We treat 1024x700 as the CLIENT area (Game Area). 
    // We use AdjustWindowRect to calculate the full Window size including the Menu Bar.
    const int CLIENT_WIDTH = 1024;
    const int CLIENT_HEIGHT = 700;

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    // Calculate required window size to ensure the Game Area remains 1024x700
    RECT rcWindow = { 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT };
    AdjustWindowRect(&rcWindow, dwStyle, TRUE); // TRUE because we are adding a Menu

    int windowWidth = rcWindow.right - rcWindow.left;
    int windowHeight = rcWindow.bottom - rcWindow.top;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;

    // --- Change Window Title based on mode ---

    //std::wstring windowTitle = L"Midnight Pool 4"/*"Direct2D 8-Ball Pool"*/;
    //if (gameMode == HUMAN_VS_HUMAN) windowTitle += L" (Human vs Human)";
    //else windowTitle += L" (Human vs " + player2Info.name + L")";

    //DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX; // No WS_THICKFRAME, No WS_MAXIMIZEBOX

    // Load the Menu Resource
    HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAIN_MENU));

    hwndMain = CreateWindowEx(
        0, L"BLISS_GameEngine"/*"Direct2D_8BallPool"*/, L"Midnight Pool 4", dwStyle, // Create with a temporary title
        windowX, windowY, windowWidth, windowHeight,
        NULL, hMenu, hInstance, NULL // Pass hMenu here
        //hwndMain = CreateWindowEx(
            //0, L"BLISS_GameEngine"/*"Direct2D_8BallPool"*/, windowTitle.c_str(), dwStyle,
            //windowX, windowY, WINDOW_WIDTH, WINDOW_HEIGHT,
            //NULL, NULL, hInstance, NULL
    );

    if (!hwndMain) {
        MessageBox(NULL, L"Window Creation Failed.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return -1;
    }

    // --- NEW: Initialize Menu State ---
    // Check/Uncheck items based on default globals
    CheckMenuItem(hMenu, ID_GAME_MUTESOUNDFX, g_soundEffectsMuted ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_GAME_MUTEMUSIC, isMusicPlaying.load() ? MF_UNCHECKED : MF_CHECKED); // Logic inverted: Checked = Muted
    CheckMenuItem(hMenu, ID_GAME_CHEATMODE, cheatModeEnabled ? MF_CHECKED : MF_UNCHECKED);

    // Set initial Radio Button for Table Color
    int colorID = ID_TABLECOLOR_INDIGO; // Default
    if (selectedTableColor == TAN) colorID = ID_TABLECOLOR_TAN;
    else if (selectedTableColor == TEAL) colorID = ID_TABLECOLOR_TEAL;
    else if (selectedTableColor == GREEN) colorID = ID_TABLECOLOR_GREEN;
    else if (selectedTableColor == RED) colorID = ID_TABLECOLOR_RED;
    CheckMenuRadioItem(hMenu, ID_TABLECOLOR_INDIGO, ID_TABLECOLOR_RED, colorID, MF_BYCOMMAND);

    // Initialize Direct2D Resources AFTER window creation
    if (FAILED(CreateDeviceResources())) {
        MessageBox(NULL, L"Failed to create Direct2D resources.", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwndMain);
        CoUninitialize();
        return -1;
    }

    // Set the final, correct window title after everything is ready
    UpdateWindowTitle();

    // --- NEW: Preload Sound Effects ---
    LoadWavFileToMemory(L"cue.wav", g_soundBufferCue);
    LoadWavFileToMemory(L"pocket.wav", g_soundBufferPocket);
    LoadWavFileToMemory(L"wall.wav", g_soundBufferWall);
    LoadWavFileToMemory(L"poolballhit.wav", g_soundBufferHit);
    LoadWavFileToMemory(L"rack.wav", g_soundBufferRack);
    // [+] NEW: Load Win/Loss/Foul sounds
    LoadWavFileToMemory(L"won.wav", g_soundBufferWon);
    LoadWavFileToMemory(L"loss.wav", g_soundBufferLoss);
    LoadWavFileToMemory(L"foul.wav", g_soundBufferFoul);
    // ----------------------------------

    InitGame(); // Initialize game state AFTER resources are ready & mode is set
    Sleep(500); // Allow window to fully initialize before starting the countdown //midi func
    // --- FIX: Sync Menu State (Music is now playing, so Uncheck "Mute") ---
    // The menu was initialized before music started (when isMusicPlaying was false/default),
    // so it defaulted to Checked. We must explicitly uncheck it now that music is playing.
    if (hwndMain) {
        HMENU hCurrentMenu = GetMenu(hwndMain);
        if (hCurrentMenu) {
            CheckMenuItem(hCurrentMenu, ID_GAME_MUTEMUSIC, MF_UNCHECKED);
        }
    }
    // ----------------------------------------------------------------------
    StartMidi(hwndMain, std::wstring(TEXT("BSQ.MID"))); // Replace with your MIDI filename
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

    // [+] NEW: Shut down GDI+ before exiting
    Gdiplus::GdiplusShutdown(gdiplusToken);


    KillTimer(hwndMain, ID_TIMER);
    DiscardDeviceResources();
    SaveSettings(); // Save settings on exit
    CoUninitialize();

    return (int)msg.wParam;
}

// --- NEW HELPER FUNCTION TO UPDATE WINDOW TITLE ---
void UpdateWindowTitle() {
    if (!hwndMain) return;

    std::wstring windowTitle = L"Midnight Pool 4";

    // --- NEW: Check for Practice Mode first ---
    if (g_isPracticeMode) {
        windowTitle += L" (Practice Mode)";

        // Optionally add game type to practice mode title
        if (currentGameType == STRAIGHT_POOL) windowTitle += L" - Straight Pool";
        else if (currentGameType == GameType::NINE_BALL) windowTitle += L" - 9-Ball";
        else windowTitle += L" - 8-Ball";
    }
    else {

        // Add Game Mode
        if (currentGameType == STRAIGHT_POOL) windowTitle += L" (Straight Pool)";
        else if (currentGameType == GameType::NINE_BALL) windowTitle += L" (9-Ball)";
        else windowTitle += L" (8-Ball)";

        // Add Player Type
        if (gameMode == HUMAN_VS_HUMAN) windowTitle += L" - Human vs Human";
        else windowTitle += L" - Human vs " + player2Info.name;
    }

    if (g_soundEffectsMuted) windowTitle += L" [Muted]";

    SetWindowText(hwndMain, windowTitle.c_str());
}

// [+] NEW: Dialog Procedure for the About Box
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Locate the JPG resource data
        HRSRC hResource = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_HACKER_IMG), L"JPG");
        if (!hResource) return (INT_PTR)TRUE;

        DWORD imageSize = SizeofResource(GetModuleHandle(NULL), hResource);
        const void* pResourceData = LockResource(LoadResource(GetModuleHandle(NULL), hResource));
        if (!pResourceData) return (INT_PTR)TRUE;

        // Create a memory stream from the resource data
        HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
        if (hBuffer)
        {
            void* pBuffer = GlobalLock(hBuffer);
            if (pBuffer)
            {
                CopyMemory(pBuffer, pResourceData, imageSize);
                IStream* pStream = NULL;
                if (CreateStreamOnHGlobal(hBuffer, FALSE, &pStream) == S_OK)
                {
                    // Load the JPG using GDI+
                    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromStream(pStream);
                    if (pBitmap && pBitmap->GetLastStatus() == Gdiplus::Ok)
                    {
                        HBITMAP hbmReturn = NULL;
                        // Convert GDI+ bitmap to a standard HBITMAP handle with a black background
                        pBitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0), &hbmReturn);

                        // Send the HBITMAP to the picture control
                        SendDlgItemMessage(hDlg, IDC_HACKER_PIC, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbmReturn);

                        // Note: The picture control now owns the HBITMAP and will delete it.
                        delete pBitmap;
                    }
                    pStream->Release();
                }
                GlobalUnlock(hBuffer);
            }
            GlobalFree(hBuffer);
        }

        // Center the dialog on screen
        RECT rcDlg;
        GetWindowRect(hDlg, &rcDlg);

        int dlgWidth = rcDlg.right - rcDlg.left;
        int dlgHeight = rcDlg.bottom - rcDlg.top;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        int xPos = (screenWidth - dlgWidth) / 2;
        int yPos = (screenHeight - dlgHeight) / 2;

        SetWindowPos(hDlg, HWND_TOP, xPos, yPos, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER);

        // Now set the long About text /*4827+ 8903+ 10192+ 10192+ 11578+ 11885+ 11902+ 12014+ 12127+*/
        const wchar_t* aboutText =
            L"Direct2D-based StickPool game made in C++ from scratch (12413+ lines of code) (non-OOP-based)\n"
            L"First successful Clone in C++ (no other sites or projects were there to glean from.)\n"
            L"Made with AI assist (others were in JS / non-8-Ball in C# etc.)\n"
            L"Copyright (C) 2026 Evans Thorpemorton, Entisoft Solutions.\n"
            L"Midnight Pool 4. 'BLISS' Game Engine / TitanCore Vortex Engine.\n"
            L"ClusterPipeline / VectorFlowOptimizer / HyperSyncOSCompiler / NovaGridTitanVaultFS Provided by GeminiAI.\n"
            L"Includes AI Difficulty Modes, Aim-Trajectory For Table Rails + Hard Angles TipShots.\n"
            L"Controls: F2=New Game, G=TrickShot, T=TestMode, M=Mute, "
            L"ArrowKeys(+Shift)=AimCueStick, Up/Down=ShotPower, Spacebar=Shoot";

        SetDlgItemText(hDlg, IDC_ABOUT_TEXT, aboutText);
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        // Close dialog when OK is clicked or Esc is pressed
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// [+] FINAL FIX: Robust Debug Undo (With Semaphore Re-enabled)
void DebugReturnLastBall() {
    // [+] NEW: Acquire Semaphore to prevent re-entry
    // If lock is already true, return immediately.
    bool expected = false;
    if (!g_debugActionLock.compare_exchange_strong(expected, true)) return;

    // 1. Validation
    if (g_pocketHistory.empty()) {
        g_debugActionLock.store(false); // [+] Release lock before returning
        return;
    }

    // 2. Pop event
    PocketEvent lastEvent = g_pocketHistory.back();
    g_pocketHistory.pop_back();

    Ball* b = GetBallById(lastEvent.ballId);
    if (!b) {
        g_debugActionLock.store(false); // [+] Release lock before returning
        return;
    }

    // 3. Reset State
    b->isPocketed = false;
    b->vx = 0; b->vy = 0;
    b->rotationAngle = 0.0f;

    // 4. Place at Rack Spot
    float targetX = RACK_POS_X;
    float targetY = RACK_POS_Y;

    // Collision loop
    for (int i = 0; i < 100; ++i) {
        bool overlap = false;
        for (const auto& other : balls) {
            if (!other.isPocketed && other.id != b->id) {
                if (GetDistanceSq(targetX, targetY, other.x, other.y) < (BALL_RADIUS * 2.1f) * (BALL_RADIUS * 2.1f)) {
                    overlap = true; break;
                }
            }
        }
        if (!overlap) break;
        targetY += BALL_RADIUS * 2.1f;
        if (targetY > TABLE_BOTTOM - BALL_RADIUS) {
            targetY = TABLE_TOP + BALL_RADIUS + (i * 2.0f);
            targetX -= BALL_RADIUS * 2.1f;
        }
    }
    b->x = targetX;
    b->y = targetY;

    // 5. Restore Global Counters
    ballsOnTableCount = std::min((int)balls.size(), ballsOnTableCount + 1);

    if (currentGameType == GameType::STRAIGHT_POOL) {
        if (lastEvent.playerId == 1) player1StraightPoolScore = std::max(0, player1StraightPoolScore - 1);
        else player2StraightPoolScore = std::max(0, player2StraightPoolScore - 1);
    }
    else if (currentGameType == GameType::EIGHT_BALL_MODE) {
        if (b->id != 0 && b->id != 8) {
            if (lastEvent.playerId == 1 && player1Info.assignedType == b->type)
                player1Info.ballsPocketedCount = std::max(0, player1Info.ballsPocketedCount - 1);
            else if (lastEvent.playerId == 2 && player2Info.assignedType == b->type)
                player2Info.ballsPocketedCount = std::max(0, player2Info.ballsPocketedCount - 1);
        }
        // Reset 8-ball selection state if needed
        bool p1On8 = IsPlayerOnEightBall(1);
        bool p2On8 = IsPlayerOnEightBall(2);
        if (currentGameState == CHOOSING_POCKET_P1 && !p1On8) {
            currentGameState = PLAYER1_TURN;
            calledPocketP1 = -1; pocketCallMessage = L"";
        }
        else if ((currentGameState == CHOOSING_POCKET_P2 || currentGameState == AI_THINKING) && !p2On8 && currentPlayer == 2) {
            currentGameState = PLAYER2_TURN;
            calledPocketP2 = -1; pocketCallMessage = L"";
            aiTurnPending = isPlayer2AI;
        }
    }
    else if (currentGameType == GameType::NINE_BALL) {
        UpdateLowestBall();
    }

    // 6. Cancel Game Over if active
    if (currentGameState == GAME_OVER) {
        gameOverMessage = L"";
        currentPlayer = lastEvent.playerId;
        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
    }

    // [+] NEW: Release Semaphore at end of function
    g_debugActionLock.store(false);
}

// --- WndProc ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Ball* cueBall = nullptr;
    switch (msg) {
        // --- NEW: Handle Menu Commands ---
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        HMENU hMenu = GetMenu(hwnd);

        switch (wmId) {
        case ID_FILE_NEWGAME:
            ResetGame((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
            break;

        case ID_FILE_EXIT:
            StopMidi();
            SaveSettings();
            PostQuitMessage(0);
            break;

        case ID_GAME_MUTEMUSIC:
            // Toggle Music Logic
            if (isMusicPlaying.load()) {
                StopMidi(); // Stop if playing
                CheckMenuItem(hMenu, ID_GAME_MUTEMUSIC, MF_CHECKED); // Checked means Muted
            }
            else {
                TCHAR midiPathBuf[MAX_PATH];
                GetModuleFileName(NULL, midiPathBuf, MAX_PATH);
                TCHAR* lastBackslash = _tcsrchr(midiPathBuf, '\\');
                if (lastBackslash != NULL) { *(lastBackslash + 1) = '\0'; }
                _tcscat_s(midiPathBuf, MAX_PATH, TEXT("BSQ.MID"));
                StartMidi(hwnd, std::wstring(midiPathBuf));
                CheckMenuItem(hMenu, ID_GAME_MUTEMUSIC, MF_UNCHECKED); // Unchecked means Playing
            }
            UpdateWindowTitle();
            break;

        case ID_GAME_MUTESOUNDFX:
            g_soundEffectsMuted = !g_soundEffectsMuted;
            CheckMenuItem(hMenu, ID_GAME_MUTESOUNDFX, g_soundEffectsMuted ? MF_CHECKED : MF_UNCHECKED);
            UpdateWindowTitle();
            break;

        /*case ID_GAME_CHEATMODE:
            cheatModeEnabled = !cheatModeEnabled;
            CheckMenuItem(hMenu, ID_GAME_CHEATMODE, cheatModeEnabled ? MF_CHECKED : MF_UNCHECKED);
            if (cheatModeEnabled) MessageBeep(MB_ICONEXCLAMATION);
            else MessageBeep(MB_OK);
            InvalidateRect(hwnd, NULL, FALSE); // Redraw to show/hide "CHEAT MODE" text
            break;*/

            // --- Toggle Cheat Mode (Kinda like your existing handler for cheat) ---
        case ID_GAME_CHEATMODE:
        {
            // Toggle the cheat-mode flag
            cheatModeEnabled = !cheatModeEnabled;

            // Sync the menu checkbox for cheat mode
            HMENU hMenu = GetMenu(hwnd);
            if (hMenu) {
                CheckMenuItem(hMenu, ID_GAME_CHEATMODE,
                    cheatModeEnabled ? MF_CHECKED : MF_UNCHECKED);

                // Enable or gray the "Return Sunk Ball" menu item based ONLY on cheatModeEnabled
                EnableMenuItem(hMenu,
                    ID_GAME_UNDO_SUNK_BALL,
                    MF_BYCOMMAND | (cheatModeEnabled ? MF_ENABLED : MF_GRAYED));

                // Refresh the menu visuals immediately
                DrawMenuBar(hwnd);
            }

            // Optional audible feedback
            MessageBeep(cheatModeEnabled ? MB_ICONEXCLAMATION : MB_OK);

            // Redraw window so any cheat-mode visuals update
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

        case ID_GAME_DEBUGMODE:
            g_debugMode = !g_debugMode;
            CheckMenuItem(hMenu, ID_GAME_DEBUGMODE, g_debugMode ? MF_CHECKED : MF_UNCHECKED);
            // You can add logic in DrawUI to show "DEBUG" text if you like
            InvalidateRect(hwnd, NULL, FALSE);
            break;

            // --- NEW: Handle Test Mode Menu Click ---
        case ID_GAME_TESTMODE:
            // Trigger the existing 'T' key logic in WM_KEYDOWN
            SendMessage(hwnd, WM_KEYDOWN, 'T', 0);
            break;

        case ID_GAME_ENDACTION:
            // Trigger the 'Z' key logic
            SendMessage(hwnd, WM_KEYDOWN, 'Z', 0);
            break;

        case ID_GAME_RETRY:
            // Trigger the 'R' key logic
            SendMessage(hwnd, WM_KEYDOWN, 'R', 0);
            break;

            // [+] NEW: Show Paths Menu Command
        case ID_GAME_TRACEPATHS:
            SendMessage(hwnd, WM_KEYDOWN, 'P', 0);
            break;

        // --- Return Sunk Ball command (guarded so it only runs when Cheat Mode is ON) ---
        case ID_GAME_UNDO_SUNK_BALL:
        {
            // Server-side safety: only allow the action when cheatModeEnabled is true.
            if (!cheatModeEnabled) {
                MessageBeep(MB_ICONEXCLAMATION);
                break;
            }

            // Perform the undo and refresh view
            DebugReturnLastBall();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

        /*case ID_GAME_UNDO_SUNK_BALL:
        {
            // permissive: allow either Debug Mode or Cheat Mode to use the undo
            if (cheatModeEnabled) {
                // Optional debug trace while testing:
                //OutputDebugStringA("DBG: Menu/Accel Ctrl+Y -> Return Sunk Ball\n");

                DebugReturnLastBall();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            else {
                // Audible feedback to indicate it's not allowed right now
                MessageBeep(MB_ICONEXCLAMATION);
            }
        }
        break;*/

            // --- Table Color Hot-Swap ---
        case ID_TABLECOLOR_INDIGO:
        case ID_TABLECOLOR_TAN:
        case ID_TABLECOLOR_TEAL:
        case ID_TABLECOLOR_GREEN:
        case ID_TABLECOLOR_RED:
        {
            // Map ID to Enum
            if (wmId == ID_TABLECOLOR_INDIGO) selectedTableColor = INDIGO;
            else if (wmId == ID_TABLECOLOR_TAN) selectedTableColor = TAN;
            else if (wmId == ID_TABLECOLOR_TEAL) selectedTableColor = TEAL;
            else if (wmId == ID_TABLECOLOR_GREEN) selectedTableColor = GREEN;
            else if (wmId == ID_TABLECOLOR_RED) selectedTableColor = RED;

            // Apply Color Immediately
            TABLE_COLOR = tableColorValues[selectedTableColor];

            // Update Radio Check
            CheckMenuRadioItem(hMenu, ID_TABLECOLOR_INDIGO, ID_TABLECOLOR_RED, wmId, MF_BYCOMMAND);

            // Force Redraw
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

        case ID_HELP_CONTROLS:
            MessageBox(hwnd,
                L"--- CONTROLS ---\n\n"
                L"Aiming:\n"
                L"  MOUSE: Click & Drag Cue Ball (Hand) or Cue Stick\n"
                L"  LEFT/RIGHT ARROW: Fine Tune Angle\n"
                L"  SHIFT + ARROW: Fast Tune Angle\n\n"
                L"Shooting:\n"
                L"  MOUSE: Drag Stick Back & Release\n"
                L"  SPACEBAR: Shoot (uses Power Meter)\n"
                L"  UP/DOWN ARROW: Adjust Power Meter\n\n"
                L"English (Spin):\n"
                L"  Click the Cue Ball Indicator (Top Left) to apply spin.\n\n"
                L"Shortcuts:\n"
                L"  M: Mute Audio\n"
                L"  F2: New Game\n"
                L"  G: Toggle Cheat Mode\n"
                L"  K: Toggle Kibitzer Mode\n"
                L"  T: Test Mode\n"
                L"  Z: End Action\n"
                L"  R: Retry Shot\n"
                L"  P: Trace Paths\n"
                L"  Ctrl+Z: Return PocketedBall To Table (In DebugMode)\n",
                L"Keyboard Controls", MB_OK | MB_ICONINFORMATION);
            break;

        case ID_HELP_INSTRUCTIONS:
            MessageBox(hwnd,
                L"=== 8-BALL POOL ===\n"
                L"OBJECTIVE: Pocket all balls of your assigned group (Solids 1-7 or Stripes 9-15) and then pocket the 8-Ball to win.\n"
                L"RULES:\n"
                L"- The table is 'Open' until a player legally pockets a ball.\n"
                L"- You must hit a ball of your assigned group first.\n"
                L"- If you pocket the 8-Ball before clearing your group, or scratch while shooting the 8-Ball, you lose.\n"
                L"- You must call the pocket for the 8-Ball shot.\n\n"

                L"=== NINE BALL ===\n"
                L"OBJECTIVE: Legally pocket the 9-Ball.\n"
                L"RULES:\n"
                L"- You must always contact the lowest numbered ball on the table first.\n"
                L"- If you hit the lowest ball first and the 9-Ball falls in (combo), you win immediately.\n"
                L"- Shots do not need to be called.\n"
                L"- Three consecutive fouls result in a loss.\n\n"

                L"=== STRAIGHT POOL (14.1 Continuous) ===\n"
                L"OBJECTIVE: Be the first to reach the target score (default 25 points).\n"
                L"RULES:\n"
                L"- Any object ball is a legal target. Each ball is worth 1 point.\n"
                L"- You must call the pocket for every shot.\n"
                L"- When 14 balls are pocketed, they are re-racked, leaving the 15th ball and the cue ball in place to continue the run.",
                L"Game Instructions", MB_OK | MB_ICONINFORMATION);
            break;

        case ID_HELP_ABOUT:
            // Trigger the existing F1 About Box logic
            SendMessage(hwnd, WM_KEYDOWN, VK_F1, 0);
            break;
        }
        return 0;
    }
    /*case MM_MCINOTIFY:
        // This message is received when the MIDI file finishes playing.
        if (wParam == MCI_NOTIFY_SUCCESSFUL && isMusicPlaying) {
            // If music is still enabled, restart the track from the beginning.
            mciSendCommand(midiDeviceID, MCI_SEEK, MCI_SEEK_TO_START, NULL);
            MCI_PLAY_PARMS mciPlay = { 0 };
            mciPlay.dwCallback = (DWORD_PTR)hwnd;
            mciSendCommand(midiDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&mciPlay);
        }
        return 0;*/
    /*case WM_CREATE:
        return 0;*/
    case WM_CREATE:
    {
        // Ensure the "Return Sunk Ball" menu item starts grayed out.
        HMENU hMenu = GetMenu(hwnd);
        if (hMenu) {
            // Make sure ID_GAME_RETURN_SUNK_BALL is defined in resource.h
            EnableMenuItem(
                hMenu,
                ID_GAME_UNDO_SUNK_BALL,
                MF_BYCOMMAND | MF_GRAYED
            );

            // Keep Cheat menu checkbox in sync in case cheatModeEnabled has a startup value
            CheckMenuItem(
                hMenu,
                ID_GAME_CHEATMODE,
                cheatModeEnabled ? MF_CHECKED : MF_UNCHECKED
            );

            DrawMenuBar(hwnd);
        }

        return 0;
    }
    case WM_PAINT:
        OnPaint();
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
            GameUpdate();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
    {
        cueBall = GetCueBall();
        bool canPlayerControl = ((currentPlayer == 1 && (currentGameState == PLAYER1_TURN || currentGameState == AIMING || currentGameState == BREAKING || currentGameState == BALL_IN_HAND_P1 || currentGameState == PRE_BREAK_PLACEMENT)) ||
            (currentPlayer == 2 && !isPlayer2AI && (currentGameState == PLAYER2_TURN || currentGameState == AIMING || currentGameState == BREAKING || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT)));
        // Check if Ctrl is pressed
        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000);
        if (wParam == VK_F2) {
            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            ResetGame(hInstance);
            return 0;
        }
        else if (wParam == VK_F1) {
            // [+] NEW: Create and show the custom modal dialog box
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, AboutDlgProc);
            return 0;
        }
        //break;
        if (wParam == 'M' || wParam == 'm') {
            // --- FOOLPROOF Mute Toggle Logic ---
            // 1. Toggle the sound effects mute flag.
            g_soundEffectsMuted = !g_soundEffectsMuted;

            // 2. Toggle the music based on its current state.
            if (isMusicPlaying) {
                // If music is currently playing, stop it.
                StopMidi();
            }
            else {
                // If music is stopped, start it again.
                TCHAR midiPathBuf[MAX_PATH];
                GetModuleFileName(NULL, midiPathBuf, MAX_PATH);
                TCHAR* lastBackslash = _tcsrchr(midiPathBuf, '\\');
                if (lastBackslash != NULL) { *(lastBackslash + 1) = '\0'; }
                _tcscat_s(midiPathBuf, MAX_PATH, TEXT("BSQ.MID"));
                // copy into an owning std::wstring and pass that into StartMidi so the thread owns a copy
                std::wstring midiPathStr = midiPathBuf;
                StartMidi(hwndMain, midiPathStr);
            }
            // 3. Update the window title to reflect the new mute state.
            UpdateWindowTitle();
            // --- End Mute Toggle ---
            /*g_soundEffectsMuted = !g_soundEffectsMuted; // Toggle sound effects mute
            if (isMusicPlaying) {
                StopMidi();
                isMusicPlaying = false;
            }
            else {
                TCHAR midiPath[MAX_PATH];
                GetModuleFileName(NULL, midiPath, MAX_PATH);
                TCHAR* lastBackslash = _tcsrchr(midiPath, '\\');
                if (lastBackslash != NULL) {
                    *(lastBackslash + 1) = '\0';
                }
                _tcscat_s(midiPath, MAX_PATH, TEXT("BSQ.MID"));
                StartMidi(hwndMain, midiPath);
                isMusicPlaying = true;
            }*/
        }
        // --- FOOLPROOF FIX: Handle Cheat Toggle as a Global Hotkey ---
        // Moved outside the canPlayerControl block to work in any game state.
        // --- Make G toggle DEBUG MODE (and sync the menu check) ---
// Accept both uppercase and lowercase 'G'
        if (wParam == 'G' || wParam == 'g') {
            cheatModeEnabled = !cheatModeEnabled;

            // Sync the Menu Bar checkmark state so the UI reflects the hotkey toggle.
            HMENU hMenu = GetMenu(hwnd);
            if (hMenu) {
                CheckMenuItem(hMenu, ID_GAME_CHEATMODE, cheatModeEnabled ? MF_CHECKED : MF_UNCHECKED);
            }

            // Audible feedback to know what happened
            if (cheatModeEnabled)
                MessageBeep(MB_ICONEXCLAMATION);
            else
                MessageBeep(MB_OK);

            // Force immediate redraw so the Debug label/visuals appear/disappear
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        /*if (wParam == 'G' || wParam == 'g') {
            cheatModeEnabled = !cheatModeEnabled;
            if (cheatModeEnabled)
                MessageBeep(MB_ICONEXCLAMATION);
            else
                MessageBeep(MB_OK);
            return 0; // Exit after toggling
        }*/
        // --- End of Fix ---
        // --- NEW: Toggle Kibitzer/Debug Mode with 'K' ---
        // --- Make K toggle KIBITZER MODE (and sync the menu check) ---
// Accept both uppercase and lowercase 'K'
        if (wParam == 'K' || wParam == 'k') {
            g_debugMode = !g_debugMode;

            // Sync the Menu Bar checkmark state so the UI reflects the hotkey toggle.
            HMENU hMenu = GetMenu(hwnd);
            if (hMenu) {
                CheckMenuItem(hMenu, ID_GAME_DEBUGMODE, g_debugMode ? MF_CHECKED : MF_UNCHECKED);
            }

            // Audible feedback to know what happened
            if (g_debugMode)
                MessageBeep(MB_ICONEXCLAMATION);
            else
                MessageBeep(MB_OK);

            // Force immediate redraw so the Kibitzer visuals appear/disappear
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        /*if (wParam == 'K' || wParam == 'k') {
            g_debugMode = !g_debugMode;

            // Sync the Menu Bar checkmark state
            HMENU hMenu = GetMenu(hwnd);
            if (hMenu) {
                CheckMenuItem(hMenu, ID_GAME_DEBUGMODE, g_debugMode ? MF_CHECKED : MF_UNCHECKED);
            }

            // Force immediate redraw to show/hide trajectories
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }*/
        // ------------------------------------------------

        // --- NEW: Handle End Action (Fast-Forward) ---
        //if (wParam == 'Z' || wParam == 'z') {
        if ((wParam == 'Z' || wParam == 'z') && !ctrlDown) {
            // Only allow skipping when the shot is actually happening
            if (currentGameState == SHOT_IN_PROGRESS) {
                SkipCurrentShot();
                InvalidateRect(hwnd, NULL, FALSE); // Redraw immediately at the stop position
            }
            return 0;
        }

        // [+] NEW: Retry Shot (Undo) with 'R'
        if (wParam == 'R' || wParam == 'r') {
            // Conditions: Valid backup exists, balls stopped, game running, human turn (or human reviewing CPU result)
            if (g_hasUndo && !AreBallsMoving() && currentGameState != GAME_OVER) {
                RestoreStateFromUndo();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        // [+] NEW: Toggle "Show Paths" with 'P'
        if (wParam == 'P' || wParam == 'p') {
            g_tracePaths = !g_tracePaths;

            // Update menu checkmark
            HMENU hMenu = GetMenu(hwnd);
            if (hMenu) {
                CheckMenuItem(hMenu, ID_GAME_TRACEPATHS, g_tracePaths ? MF_CHECKED : MF_UNCHECKED);
            }

            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        // [+] NEW: Ctrl+Z (Undo Pocket) as backup
        //if ((wParam == 'Y' || wParam == 'y') && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if ((wParam == 'Z' || wParam == 'z') && ctrlDown) {
            if (cheatModeEnabled) {
                DebugReturnLastBall();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        if (canPlayerControl) {
            bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            float angleStep = shiftPressed ? 0.05f : 0.01f;
            float powerStep = 0.2f;
            switch (wParam) {
            case VK_LEFT:
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    cueAngle -= angleStep;
                    if (cueAngle < 0) cueAngle += 2 * PI;
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = false;
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;
            case VK_RIGHT:
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    cueAngle += angleStep;
                    if (cueAngle >= 2 * PI) cueAngle -= 2 * PI;
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = false;
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;
            case VK_UP:
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    shotPower -= powerStep;
                    if (shotPower < 0.0f) shotPower = 0.0f;
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = true;
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;
            case VK_DOWN:
                if (currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING) {
                    shotPower += powerStep;
                    if (shotPower > MAX_SHOT_POWER) shotPower = MAX_SHOT_POWER;
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN) currentGameState = AIMING;
                    isAiming = true;
                    isDraggingStick = false;
                    keyboardAimingActive = true;
                }
                break;
            case VK_SPACE:
                if ((currentGameState == AIMING || currentGameState == BREAKING || currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN)
                    && currentGameState != SHOT_IN_PROGRESS && currentGameState != AI_THINKING)
                {
                    if (shotPower > 0.15f) {
                        firstHitBallIdThisShot = -1;
                        cueHitObjectBallThisShot = false;
                        railHitAfterContact = false;
                        if (!g_soundEffectsMuted) {
                            std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
                        }
                        //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
                        ApplyShot(shotPower, cueAngle, cueSpinX, cueSpinY);
                        currentGameState = SHOT_IN_PROGRESS;
                        foulCommitted = false;
                        pocketedThisTurn.clear();
                        shotPower = 0;
                        isAiming = false; isDraggingStick = false;
                        keyboardAimingActive = false;
                    }
                }
                break;
            case VK_ESCAPE:
                if ((currentGameState == AIMING || currentGameState == BREAKING) || shotPower > 0)
                {
                    shotPower = 0.0f;
                    isAiming = false;
                    isDraggingStick = false;
                    keyboardAimingActive = false;
                    if (currentGameState != BREAKING) {
                        // FIX: Check if we need to return to pocket selection state
                        if (IsPlayerOnEightBall(currentPlayer)) {
                            currentGameState = (currentPlayer == 1) ? CHOOSING_POCKET_P1 : CHOOSING_POCKET_P2;
                        }
                        else {
                            currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                        }
                    }
                }
                break;
                /*case VK_ESCAPE:
                    if ((currentGameState == AIMING || currentGameState == BREAKING) || shotPower > 0)
                    {
                        shotPower = 0.0f;
                        isAiming = false;
                        isDraggingStick = false;
                        keyboardAimingActive = false;
                        if (currentGameState != BREAKING) {
                            currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                        }
                    }
                    break;*/
                    // === DEBUG: Press 'T' to set up endgame 8-ball test position ===
            case 'T':
            case 't':
            {
                // Assign ball types to players
                player1Info.assignedType = BallType::SOLID;
                player2Info.assignedType = BallType::STRIPE;
                player1Info.ballsPocketedCount = 6;
                player2Info.ballsPocketedCount = 6;

                // Reset foul/GameOver flags
                foulCommitted = false;
                gameOverMessage = L"";

                // Stop all ball motion and clear pocket states first
                for (auto& b : balls) {
                    b.vx = 0.0f;
                    b.vy = 0.0f;
                    b.rotationAngle = 0.0f;
                    b.rotationAxis = 0.0f;
                    b.isPocketed = false; // will mark pocketed ones below
                }

                // Pocket solids 1..6 (mark as pocketed)
                for (int id = 1; id <= 6; ++id) {
                    for (auto& b : balls) {
                        if (b.id == id) {
                            b.isPocketed = true;
                            // place pocketed balls off-screen or at pocket position if you prefer:
                            b.x = TABLE_RIGHT + 40.0f + (id * 6.0f);
                            b.y = TABLE_TOP - 40.0f;
                        }
                    }
                }

                // Pocket stripes 9..14 (mark as pocketed)
                for (int id = 9; id <= 14; ++id) {
                    for (auto& b : balls) {
                        if (b.id == id) {
                            b.isPocketed = true;
                            b.x = TABLE_RIGHT + 40.0f + (id * 6.0f);
                            b.y = TABLE_BOTTOM + 40.0f;
                        }
                    }
                }

                // Keep on-table: solid id=7, stripe id=15, eight id=8, cue id=0
                // place them near the table center (cluster) and cue behind the kitchen (headstring)
                float centerX = TABLE_LEFT + (TABLE_WIDTH * 0.5f);
                float centerY = RACK_POS_Y; // use rack Y as approximate table center row
                float spacing = BALL_RADIUS * 2.6f; // small separation

                for (auto& b : balls) {
                    if (b.id == 7) {           // one solid left
                        b.isPocketed = false;
                        b.vx = b.vy = 0.0f;
                        b.x = centerX - spacing;
                        b.y = centerY;
                    }
                    else if (b.id == 15) {     // one stripe right
                        b.isPocketed = false;
                        b.vx = b.vy = 0.0f;
                        b.x = centerX + spacing;
                        b.y = centerY;
                    }
                    else if (b.id == 8) {      // 8-ball center
                        b.isPocketed = false;
                        b.vx = b.vy = 0.0f;
                        b.x = centerX;
                        b.y = centerY;
                    }
                    else if (b.id == 0) {      // cue ball behind the kitchen (HEADSTRING_X)
                        b.isPocketed = false;
                        b.vx = b.vy = 0.0f;
                        // place cue behind the headstring, a bit left of it for a natural break shot position
                        b.x = HEADSTRING_X - (TABLE_WIDTH * 0.08f);
                        b.y = centerY;
                    }
                }

                // Ensure current player is Player 1 and set game state so the HUD/UI reflects turn
                currentPlayer = 1;
                currentGameState = PLAYER1_TURN;

                // Force a UI refresh so you see the new state immediately
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
            /*case 'G':
                cheatModeEnabled = !cheatModeEnabled;
                if (cheatModeEnabled)
                    MessageBeep(MB_ICONEXCLAMATION);
                else
                    MessageBeep(MB_OK);
                break;
            default:
                break;
            }
            return 0;*/
            default:
                break;
            }
            return 0;
        }
    }
    return 0;
    case WM_MOUSEMOVE: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
            (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {
            int oldHover = currentlyHoveredPocket;
            currentlyHoveredPocket = -1;
            for (int i = 0; i < 6; ++i) {
                // FIXED: Use correct radius for hover detection
                float hoverRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
                if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < hoverRadius * hoverRadius * 2.25f) {
                    currentlyHoveredPocket = i;
                    break;
                }
            }
            if (oldHover != currentlyHoveredPocket) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        cueBall = GetCueBall();
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
        if (isDraggingCueBall &&
            ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                (!isPlayer2AI && currentPlayer == 2 && currentGameState == BALL_IN_HAND_P2) ||
                currentGameState == PRE_BREAK_PLACEMENT))
        {
            bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
            cueBall->x = (float)ptMouse.x;
            cueBall->y = (float)ptMouse.y;
            cueBall->vx = cueBall->vy = 0;
        }
        else if ((isAiming || isDraggingStick) &&
            ((currentPlayer == 1 && (currentGameState == AIMING || currentGameState == BREAKING)) ||
                (!isPlayer2AI && currentPlayer == 2 && (currentGameState == AIMING || currentGameState == BREAKING))))
        {
            float dx = (float)ptMouse.x - cueBall->x;
            float dy = (float)ptMouse.y - cueBall->y;
            if (dx != 0 || dy != 0) cueAngle = atan2f(dy, dx);
            if (!keyboardAimingActive) {
                float pullDist = GetDistance((float)ptMouse.x, (float)ptMouse.y, aimStartPoint.x, aimStartPoint.y);
                shotPower = std::min(pullDist / 10.0f, MAX_SHOT_POWER);
            }
        }
        else if (isSettingEnglish &&
            ((currentPlayer == 1 && (currentGameState == PLAYER1_TURN || currentGameState == AIMING || currentGameState == BREAKING)) ||
                (!isPlayer2AI && currentPlayer == 2 && (currentGameState == PLAYER2_TURN || currentGameState == AIMING || currentGameState == BREAKING))))
        {
            float dx = (float)ptMouse.x - spinIndicatorCenter.x;
            float dy = (float)ptMouse.y - spinIndicatorCenter.y;
            float dist = GetDistance(dx, dy, 0, 0);
            if (dist > spinIndicatorRadius) { dx *= spinIndicatorRadius / dist; dy *= spinIndicatorRadius / dist; }
            cueSpinX = dx / spinIndicatorRadius;
            cueSpinY = dy / spinIndicatorRadius;
        }
        else {
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);

        // --- FOOLPROOF FIX: Prioritize Cheat Mode Dragging ---
        // This block is moved to the top. If cheat mode is on and we click a ball,
        // we start dragging and immediately exit the handler. This prevents the
        // CHOOSING_POCKET state from interfering.
        if (cheatModeEnabled) {
            for (Ball& ball : balls) {
                float distSq = GetDistanceSq(ball.x, ball.y, (float)ptMouse.x, (float)ptMouse.y);
                if (distSq <= BALL_RADIUS * BALL_RADIUS * 4) { // Increased hit area for easier grabbing
                    isDraggingCueBall = true; // Re-using this flag for general ball dragging
                    draggingBallId = ball.id;
                    // If we grab the cue ball, we can also enter a placement state
                    if (ball.id == 0) {
                        if (currentPlayer == 1)
                            currentGameState = BALL_IN_HAND_P1;
                        else if (currentPlayer == 2 && !isPlayer2AI)
                            currentGameState = BALL_IN_HAND_P2;
                    }
                    return 0; // IMPORTANT: Exit after starting a drag
                }
            }
        }
        // --- End of Prioritized Cheat Mode Logic ---


        // Pocket selection logic now runs only if cheat mode dragging didn't activate.
        if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
            (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {

            int clickedPocketIndex = -1;
            for (int i = 0; i < 6; ++i) {
                float clickRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
                if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < clickRadius * clickRadius * 2.25f) {
                    clickedPocketIndex = i;
                    break;
                }
            }

            if (clickedPocketIndex != -1) {
                if (currentPlayer == 1) calledPocketP1 = clickedPocketIndex;
                else calledPocketP2 = clickedPocketIndex;
                InvalidateRect(hwnd, NULL, FALSE);
                // Do not return here, allow aiming to start if clicking near cue ball
            }

            Ball* cueBall = GetCueBall();
            int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
            if (cueBall && calledPocket != -1 && GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y) < BALL_RADIUS * BALL_RADIUS * 25) {
                currentGameState = AIMING;
                pocketCallMessage = L"";
                isAiming = true;
                aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y);
                return 0;
            }
            return 0; // Return here if no aim was initiated
        }

        // The rest of the original WM_LBUTTONDOWN logic follows...
        Ball* cueBall = GetCueBall();
        bool canPlayerClickInteract = ((currentPlayer == 1) || (currentPlayer == 2 && !isPlayer2AI));
        bool canInteractState = (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN ||
            currentGameState == AIMING || currentGameState == BREAKING ||
            currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 ||
            currentGameState == PRE_BREAK_PLACEMENT);

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

        bool isPlacingBall = (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT);
        bool isPlayerAllowedToPlace = (isPlacingBall &&
            ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                (currentPlayer == 2 && !isPlayer2AI && currentGameState == BALL_IN_HAND_P2) ||
                (currentGameState == PRE_BREAK_PLACEMENT)));

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
                    if (currentGameState == PRE_BREAK_PLACEMENT) currentGameState = BREAKING;
                    else if (currentGameState == BALL_IN_HAND_P1) currentGameState = PLAYER1_TURN;
                    else if (currentGameState == BALL_IN_HAND_P2) currentGameState = PLAYER2_TURN;
                    cueAngle = 0.0f;
                }
            }
            return 0;
        }

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
    }
                       /*case WM_LBUTTONDOWN: {
                           ptMouse.x = LOWORD(lParam);
                           ptMouse.y = HIWORD(lParam);
                           if ((currentGameState == CHOOSING_POCKET_P1 && currentPlayer == 1) ||
                               (currentGameState == CHOOSING_POCKET_P2 && currentPlayer == 2 && !isPlayer2AI)) {
                               int clickedPocketIndex = -1;
                               for (int i = 0; i < 6; ++i) {
                                   // FIXED: Use correct radius for click detection
                                   float clickRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
                                   if (GetDistanceSq((float)ptMouse.x, (float)ptMouse.y, pocketPositions[i].x, pocketPositions[i].y) < clickRadius * clickRadius * 2.25f) {
                                       clickedPocketIndex = i;
                                       break;
                                   }
                               }
                               if (clickedPocketIndex != -1) {
                                   if (currentPlayer == 1) calledPocketP1 = clickedPocketIndex;
                                   else calledPocketP2 = clickedPocketIndex;
                                   InvalidateRect(hwnd, NULL, FALSE);
                                   return 0;
                               }
                               Ball* cueBall = GetCueBall();
                               int calledPocket = (currentPlayer == 1) ? calledPocketP1 : calledPocketP2;
                               if (cueBall && calledPocket != -1 && GetDistanceSq(cueBall->x, cueBall->y, (float)ptMouse.x, (float)ptMouse.y) < BALL_RADIUS * BALL_RADIUS * 25) {
                                   currentGameState = AIMING;
                                   pocketCallMessage = L"";
                                   isAiming = true;
                                   aimStartPoint = D2D1::Point2F((float)ptMouse.x, (float)ptMouse.y);
                                   return 0;
                               }
                               return 0;
                           }
                           if (cheatModeEnabled) {
                               for (Ball& ball : balls) {
                                   float distSq = GetDistanceSq(ball.x, ball.y, (float)ptMouse.x, (float)ptMouse.y);
                                   if (distSq <= BALL_RADIUS * BALL_RADIUS * 4) {
                                       isDraggingCueBall = true;
                                       draggingBallId = ball.id;
                                       if (ball.id == 0) {
                                           if (currentPlayer == 1)
                                               currentGameState = BALL_IN_HAND_P1;
                                           else if (currentPlayer == 2 && !isPlayer2AI)
                                               currentGameState = BALL_IN_HAND_P2;
                                       }
                                       return 0;
                                   }
                               }
                           }
                           Ball* cueBall = GetCueBall();
                           bool canPlayerClickInteract = ((currentPlayer == 1) || (currentPlayer == 2 && !isPlayer2AI));
                           bool canInteractState = (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN ||
                               currentGameState == AIMING || currentGameState == BREAKING ||
                               currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 ||
                               currentGameState == PRE_BREAK_PLACEMENT);
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
                           bool isPlacingBall = (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT);
                           bool isPlayerAllowedToPlace = (isPlacingBall &&
                               ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                                   (currentPlayer == 2 && !isPlayer2AI && currentGameState == BALL_IN_HAND_P2) ||
                                   (currentGameState == PRE_BREAK_PLACEMENT)));
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
                                       if (currentGameState == PRE_BREAK_PLACEMENT) currentGameState = BREAKING;
                                       else if (currentGameState == BALL_IN_HAND_P1) currentGameState = PLAYER1_TURN;
                                       else if (currentGameState == BALL_IN_HAND_P2) currentGameState = PLAYER2_TURN;
                                       cueAngle = 0.0f;
                                   }
                               }
                               return 0;
                           }
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
                       }*/
    case WM_LBUTTONUP: {
        // --- Cheat-mode pocket processing ---
               // --- Glitch #3 Fix: Atomic Cheat-Mode Pocket Processing ---
        if (cheatModeEnabled && draggingBallId != -1) {
            Ball* b = GetBallById(draggingBallId);
            if (b) {
                // Check all 6 pockets
                for (int p = 0; p < 6; ++p) {
                    // Use the precise mask test
                    if (IsBallTouchingPocketMask_D2D(*b, p)) {
                        // Call the atomic handler function.
                        // This function handles the sound, score, ball count,
                        // and re-rack logic all in one safe, atomic call.
                        HandleCheatDropPocket(b, p);

                        // Break the pocket loop *immediately* to prevent
                        // the ball from being pocketed in two places at once.
                        break;
                    }
                }
            } // End if (b)

            // Reset dragging state regardless of pocketing
            draggingBallId = -1;
            isDraggingCueBall = false;
            // We DO NOT return. Allow standard LBUTTONUP logic to run
            // (which will reset isAiming, isDraggingStick, etc.)
        } // --- End Glitch #3 Fix ---

        // --- Standard Mouse Release Logic (Aiming, Placement, English) ---
        // (This is the code you provided, placed *after* the cheat mode block)

        ptMouse.x = LOWORD(lParam);
        ptMouse.y = HIWORD(lParam);
        Ball* cueBall = GetCueBall(); // Re-get cueBall pointer just in case

        // Handle release after aiming/dragging stick
        if ((isAiming || isDraggingStick) &&
            ((currentPlayer == 1 && (currentGameState == AIMING || currentGameState == BREAKING)) ||
                (!isPlayer2AI && currentPlayer == 2 && (currentGameState == AIMING || currentGameState == BREAKING))))
        {
            bool wasAiming = isAiming; // Store state before resetting
            bool wasDraggingStick = isDraggingStick;
            isAiming = false; isDraggingStick = false; // Reset flags

            if (shotPower > 0.15f) { // Only shoot if there's power
                if (currentGameState != AI_THINKING) { // Don't allow human shot during AI thinking
                    firstHitBallIdThisShot = -1; cueHitObjectBallThisShot = false; railHitAfterContact = false;
                    if (!g_soundEffectsMuted) {
                        PlayGameSound(g_soundBufferCue);
                        //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
                    }
                    ApplyShot(shotPower, cueAngle, cueSpinX, cueSpinY);
                    currentGameState = SHOT_IN_PROGRESS;
                    foulCommitted = false; pocketedThisTurn.clear();
                }
            }
            else if (currentGameState != AI_THINKING) { // If no power, revert state (unless AI thinking)
                if (currentGameState != BREAKING) { // Don't revert BREAKING state just by clicking
                    // FIX: Check if we need to return to pocket selection state
                    if (currentGameType == GameType::EIGHT_BALL_MODE && IsPlayerOnEightBall(currentPlayer)) {
                        currentGameState = (currentPlayer == 1) ? CHOOSING_POCKET_P1 : CHOOSING_POCKET_P2;
                    }
                    else { // Otherwise, go back to standard turn state
                        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                    }
                    if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = false; // Cancel pending AI turn if human reverted
                }
            }
            shotPower = 0; // Reset shot power after release
        }

        // Handle release after dragging cue ball for placement
        if (isDraggingCueBall) { // Note: This flag is now ONLY used for cue ball placement, not cheat dragging
            isDraggingCueBall = false;
            bool isPlacingState = (currentGameState == BALL_IN_HAND_P1 || currentGameState == BALL_IN_HAND_P2 || currentGameState == PRE_BREAK_PLACEMENT);
            bool isPlayerAllowed = (isPlacingState &&
                ((currentPlayer == 1 && currentGameState == BALL_IN_HAND_P1) ||
                    (currentPlayer == 2 && !isPlayer2AI && currentGameState == BALL_IN_HAND_P2) ||
                    (currentGameState == PRE_BREAK_PLACEMENT && (currentPlayer == 1 || !isPlayer2AI)) // Allow human P1/P2 in PRE_BREAK
                    ));

            if (isPlayerAllowed && cueBall) {
                bool behindHeadstring = (currentGameState == PRE_BREAK_PLACEMENT);
                if (IsValidCueBallPosition(cueBall->x, cueBall->y, behindHeadstring)) {
                    // Valid placement, finalize state
                    if (currentGameState == PRE_BREAK_PLACEMENT) {
                        currentGameState = BREAKING; // Ready to aim the break
                    }
                    else if (currentGameState == BALL_IN_HAND_P1) {
                        currentGameState = PLAYER1_TURN;
                    }
                    else if (currentGameState == BALL_IN_HAND_P2) {
                        currentGameState = PLAYER2_TURN;
                    }
                    cueAngle = 0.0f; // Reset aim angle after placement

                    // After placing ball-in-hand, check if player is now on the 8-ball
                    if (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN)
                    {
                        if (currentGameType == GameType::EIGHT_BALL_MODE) {
                            CheckAndTransitionToPocketChoice(currentPlayer);
                        }
                    }
                }
                else {
                    // Invalid placement - ideally, the visual indicator already showed this.
                    // You might want to force the ball back to a default valid spot here,
                    // or just leave it where it is and rely on the visual cue.
                    // For simplicity, we leave it, assuming the player saw the red indicator.
                }
            }
        }

        // Handle release after setting english
        if (isSettingEnglish) {
            isSettingEnglish = false;
        }

        return 0; // Standard return for message handled
    } // End WM_LBUTTONUP case
    // FOOLPROOF FIX: Handle ONLY the UP event to prevent double-firing
    // --- GUARANTEED FIX: Handle Mouse Down instead of Up ---
    case WM_RBUTTONDOWN:
    {
        // Only require Debug Mode (do NOT require Kibitzer). Use the exact pocket radius
        if (cheatModeEnabled) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            bool clickedPocket = false;
            for (int i = 0; i < 6; ++i) {
                // Use the actual hole radius for hit-testing (no padding).
                float pocketRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;

                if (GetDistanceSq((float)x, (float)y, pocketPositions[i].x, pocketPositions[i].y) < pocketRadius * pocketRadius) {
                    clickedPocket = true;
                    break;
                }
            }

            if (clickedPocket) {
                DebugReturnLastBall(); // Call the silent undo function
                InvalidateRect(hwnd, NULL, FALSE); // Force immediate redraw
                return 0;
            }
        }
        break;
    }
    case WM_DESTROY:
        // Correctly stops the music and cleans up resources on exit.
        StopMidi();
        SaveSettings();
        PostQuitMessage(0);
        return 0;
        /*isMusicPlaying = false;
        if (midiDeviceID != 0) {
            mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
            midiDeviceID = 0;
            SaveSettings();
        }
        PostQuitMessage(0);
        return 0;*/
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
    if (!pTextFormatBold && pDWriteFactory) {
        hr = pDWriteFactory->CreateTextFormat(
            L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            16.0f, L"en-us", &pTextFormatBold
        );
        if (SUCCEEDED(hr)) {
            pTextFormatBold->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pTextFormatBold->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
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

    if (!pBallNumFormat && pDWriteFactory)
    {
        hr = pDWriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            10.0f,                       // << small size for ball decals
            L"en-us",
            &pBallNumFormat);
        if (SUCCEEDED(hr))
        {
            pBallNumFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pBallNumFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
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

    // --- FOOLPROOF FIX: Create a correctly sized, per-pocket canvas for collision masks ---
    // Each mask is a small, local bitmap, preventing coordinate system mismatches.
    // The canvas for each mask only needs to be slightly larger than the pocket opening plus the ball.
    const float maxPocketRadius = std::max(CORNER_HOLE_VISUAL_RADIUS, MIDDLE_HOLE_VISUAL_RADIUS);
    const int maskSize = static_cast<int>((maxPocketRadius + BALL_RADIUS) * 2.5f); // Generous square canvas size
    CreatePocketMasks_D2D(
        maskSize,    // Small, per-pocket canvas width
        maskSize,    // Small, per-pocket canvas height
        0.0f,        // Origin X is not used in the per-pocket mask model
        0.0f,        // Origin Y is not used in the per-pocket mask model
        1.0f         // Scale remains 1.0
    );

    return hr;
}

/*HRESULT CreateDeviceResources() {
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
    if (!pTextFormatBold && pDWriteFactory) {
        hr = pDWriteFactory->CreateTextFormat(
            L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            16.0f, L"en-us", &pTextFormatBold
        );
        if (SUCCEEDED(hr)) {
            pTextFormatBold->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pTextFormatBold->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
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

    if (!pBallNumFormat && pDWriteFactory)
    {
        hr = pDWriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            10.0f,                       // << small size for ball decals
            L"en-us",
            &pBallNumFormat);
        if (SUCCEEDED(hr))
        {
            pBallNumFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pBallNumFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
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

    //CreatePocketMasks_D2D((int)TABLE_WIDTH, (int)TABLE_HEIGHT, TABLE_LEFT, TABLE_TOP, 1.0f);
        // --- FOOLPROOF FIX: Create a larger, padded canvas for the collision masks ---
    // This padding ensures that the enlarged pocket shapes are not clipped.
    const float maskPadding = MIDDLE_HOLE_VISUAL_RADIUS * 3.0f; // Generous padding
    CreatePocketMasks_D2D(
        (int)TABLE_WIDTH + 2 * (int)maskPadding,    // New, wider canvas
        (int)TABLE_HEIGHT + 2 * (int)maskPadding,   // New, taller canvas
        TABLE_LEFT - maskPadding,                   // Shift the origin to the top-left of the new canvas
        TABLE_TOP - maskPadding,
        1.0f
    );

    return hr;
}*/

void DiscardDeviceResources() {
    SafeRelease(&pRenderTarget);
    SafeRelease(&pTextFormat);
    SafeRelease(&pTextFormatBold);
    SafeRelease(&pLargeTextFormat);
    SafeRelease(&pBallNumFormat);            // NEW
    SafeRelease(&pDWriteFactory);
    ShutdownPocketMasks_D2D();
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
    bool wasPracticeMode = g_isPracticeMode; // Store practice mode state
    srand((unsigned int)time(NULL));
    isOpeningBreakShot = true;
    aiPlannedShotDetails.isValid = false;
    aiIsDisplayingAim = false;
    aiAimDisplayFramesLeft = 0;
    pocketedThisTurn.clear();
    balls.clear();

    // Reset Player Info based on Game Type
    player1Info.assignedType = BallType::NONE; // Always NONE initially, especially for Straight Pool
    player1Info.ballsPocketedCount = 0;        // Used only for 8-ball group tracking
    player2Info.assignedType = BallType::NONE;
    player2Info.ballsPocketedCount = 0;
    player1StraightPoolScore = 0;              // Reset Straight Pool score
    player2StraightPoolScore = 0;
    ballsOnTableCount = 15;                    // Reset ball count

    // [+] NEW: Clear pocket history on new game
    g_pocketHistory.clear();

    // [+] NEW: Force reset semaphore to unlocked
    g_debugActionLock.store(false);

    // 8-Ball specific resets
    if (currentGameType == GameType::EIGHT_BALL_MODE) {
        lastEightBallPocketIndex = -1;
        calledPocketP1 = -1;
        calledPocketP2 = -1;
        pocketCallMessage = L"";
    }
    else if (currentGameType == GameType::NINE_BALL) {
        lowestBallOnTable = 1;
        player1ConsecutiveFouls = 0;
        player2ConsecutiveFouls = 0;
        isPushOutAvailable = false;
        isPushOutShot = false;
        ballsOnTableCount = 9; // 9-Ball uses 9 object balls
    }

    // --- Racking (Same for both modes initially) ---
    // Create Cue Ball
    balls.push_back({ 0, BallType::CUE_BALL, TABLE_LEFT + TABLE_WIDTH * 0.15f, RACK_POS_Y, 0, 0, GetBallColor(0), false, 0.0f, 0.0f });

    std::vector<Ball> objectBalls;
    /*for (int i = 1; i <= 15; ++i) {
        BallType type = BallType::NONE; // Assign specific types only for 8-ball display consistency
        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            if (i == 8) type = BallType::EIGHT_BALL;
            else if (i < 8) type = BallType::SOLID;
            else type = BallType::STRIPE;
        }
        // For Straight Pool, type doesn't matter for rules, but assign for color
        else {
            if (i == 8) type = BallType::EIGHT_BALL; // Keep 8 black visually
            else if (i < 8) type = BallType::SOLID;  // Keep colors consistent
            else type = BallType::STRIPE;
        }
        objectBalls.push_back({ i, type, 0, 0, 0, 0, GetBallColor(i), false, 0.0f, 0.0f });
    }

    // Racking Logic (Standard Triangle) - No changes needed here */
    float spacingX = BALL_RADIUS * 2.0f * 0.866f;
    float spacingY = BALL_RADIUS * 2.0f * 1.0f;
    /*D2D1_POINT_2F rackPositions[15];
    int rackIndex = 0;
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col <= row; ++col) {
            if (rackIndex >= 15) break;
            float x = RACK_POS_X + row * spacingX;
            float y = RACK_POS_Y + (col - row / 2.0f) * spacingY;
            rackPositions[rackIndex++] = D2D1::Point2F(x, y);
        }
    }

    // Place balls in rack (8-ball in center for 8-ball mode, random for straight pool visual)
    std::random_shuffle(objectBalls.begin(), objectBalls.end()); // Shuffle all 15

    if (currentGameType == GameType::EIGHT_BALL_MODE) { */

    if (currentGameType == GameType::NINE_BALL) {
        // --- 9-Ball Racking (Diamond Shape) ---
        for (int i = 1; i <= 9; ++i) {
            // Type is irrelevant for 9-ball rules, but set for color
            BallType type = (i == 9) ? BallType::SOLID : ((i < 8) ? BallType::SOLID : BallType::STRIPE);
            if (i == 8) type = BallType::EIGHT_BALL; // Keep 8-ball black
            objectBalls.push_back({ i, type, 0, 0, 0, 0, GetBallColor(i), false, 0.0f, 0.0f });
        }

        // Diamond rack positions (1-2-3-2-1 formation)
        D2D1_POINT_2F rackPositions[9];
        rackPositions[0] = D2D1::Point2F(RACK_POS_X, RACK_POS_Y); // 1-ball (Apex)
        rackPositions[1] = D2D1::Point2F(RACK_POS_X + spacingX, RACK_POS_Y - spacingY / 2.0f); // Row 2
        rackPositions[2] = D2D1::Point2F(RACK_POS_X + spacingX, RACK_POS_Y + spacingY / 2.0f);
        rackPositions[3] = D2D1::Point2F(RACK_POS_X + 2 * spacingX, RACK_POS_Y - spacingY); // Row 3
        rackPositions[4] = D2D1::Point2F(RACK_POS_X + 2 * spacingX, RACK_POS_Y); // 9-ball (Center)
        rackPositions[5] = D2D1::Point2F(RACK_POS_X + 2 * spacingX, RACK_POS_Y + spacingY);
        rackPositions[6] = D2D1::Point2F(RACK_POS_X + 3 * spacingX, RACK_POS_Y - spacingY / 2.0f); // Row 4
        rackPositions[7] = D2D1::Point2F(RACK_POS_X + 3 * spacingX, RACK_POS_Y + spacingY / 2.0f);
        rackPositions[8] = D2D1::Point2F(RACK_POS_X + 4 * spacingX, RACK_POS_Y); // Row 5 (Back)

        // Find 1-ball and 9-ball
        Ball ball1 = objectBalls[0]; // ID 1
        Ball ball9 = objectBalls[8]; // ID 9
        objectBalls.erase(objectBalls.begin() + 8); // Remove 9-ball
        objectBalls.erase(objectBalls.begin());     // Remove 1-ball

        // [+] C++20 Migration: Use std::shuffle
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(objectBalls.begin(), objectBalls.end(), g); // Shuffle balls 2-8

        // Place 1-ball at Apex (Pos 0)
        ball1.x = rackPositions[0].x; ball1.y = rackPositions[0].y;
        balls.push_back(ball1);
        // Place 9-ball at Center (Pos 4)
        ball9.x = rackPositions[4].x; ball9.y = rackPositions[4].y;
        balls.push_back(ball9);

        // Place remaining 7 balls in other spots
        int otherBallIdx = 0;
        for (int i = 1; i < 9; ++i) {
            if (i == 4) continue; // Skip center spot
            Ball& b = objectBalls[otherBallIdx++];
            b.x = rackPositions[i].x;
            b.y = rackPositions[i].y;
            balls.push_back(b);
        }
    }
    else {
        // --- 8-Ball and Straight Pool Racking (Triangle) ---
        // --- FIX: Check for custom ball count in Straight Pool Practice Mode ---
               // --- FIX: 8-Ball and Straight Pool always rack 15 balls ---
        int numBallsToCreate = 15;
        if (currentGameType == GameType::STRAIGHT_POOL) {
            ballsOnTableCount = 15; // Explicitly set 15 for 14.1 Continuous
        }
        // --- END FIX ---
        for (int i = 1; i <= numBallsToCreate; ++i) {
            BallType type = (i == 8) ? BallType::EIGHT_BALL : ((i < 8) ? BallType::SOLID : BallType::STRIPE);
            objectBalls.push_back({ i, type, 0, 0, 0, 0, GetBallColor(i), false, 0.0f, 0.0f });
        }

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

        // [+] C++20 Migration: Use std::shuffle
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(objectBalls.begin(), objectBalls.end(), g); // Shuffle all N

        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            // Ensure 8-ball is in the middle (rack index 4) for 8-ball
            int eightBallCurrentIndex = -1;
            for (size_t i = 0; i < objectBalls.size(); ++i) {
                if (objectBalls[i].id == 8) {
                    eightBallCurrentIndex = (int)i;
                    break;
                }
            }
            if (eightBallCurrentIndex != -1 && eightBallCurrentIndex != 4) {
                std::swap(objectBalls[eightBallCurrentIndex], objectBalls[4]);
            }
            /*// Optional: Ensure front ball isn't 8-ball (already handled by shuffle mostly)
            // Optional: Ensure bottom corners are different types (complex rule, skipping for now)
        }
        // For Straight Pool, completely random placement is fine.

        for (int i = 0; i < 15; ++i) {
            Ball& ballToPlace = objectBalls[i];
            ballToPlace.x = rackPositions[i].x;
            ballToPlace.y = rackPositions[i].y;
            ballToPlace.vx = 0; ballToPlace.vy = 0; ballToPlace.isPocketed = false;
            balls.push_back(ballToPlace);
        } */
        }

        for (int i = 0; i < (int)objectBalls.size() && i < 15; ++i) {
            Ball& ballToPlace = objectBalls[i];
            ballToPlace.x = rackPositions[i].x;
            ballToPlace.y = rackPositions[i].y;
            balls.push_back(ballToPlace);
        }
    } // End 8-Ball/Straight Pool Racking
    // --- End Racking ---

    // Determine Who Breaks and Initial State (Keep existing logic - applies to both modes)
    if (isPlayer2AI && !g_isPracticeMode) { // AI breaks only if not practice mode
        switch (openingBreakMode) {
            // ... (keep CPU_BREAK, P1_BREAK, FLIP_COIN_BREAK logic) ...
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
        currentPlayer = 1;
        currentGameState = PRE_BREAK_PLACEMENT;
        aiTurnPending = false;
    }

    // Reset other state variables
    foulCommitted = false;
    gameOverMessage = L"";
    // firstBallPocketedAfterBreak used only in 8-ball for assignment
    shotPower = 0.0f; cueSpinX = 0.0f; cueSpinY = 0.0f;
    isAiming = false; isDraggingCueBall = false; isSettingEnglish = false;
    cueAngle = 0.0f;

    g_hasUndo = false; // [+] NEW: Clear undo history on game start

    // --- Restore Practice Mode State ---
    g_isPracticeMode = wasPracticeMode; // Restore practice mode state

    if (g_isPracticeMode) {
        // Force practice mode settings
        gameMode = HUMAN_VS_HUMAN; // Practice is always 1P
        isPlayer2AI = false;
        currentPlayer = 1;
        player1Info.name = g_player1Name; // Use P1 name
        player2Info.name = L""; // No P2
        if (currentGameType == GameType::STRAIGHT_POOL) {
            player1StraightPoolScore = 0; // Reset score for Straight Pool practice
            player2StraightPoolScore = 0;
        }
        currentGameState = PRE_BREAK_PLACEMENT; // Start by placing the ball
        aiTurnPending = false;
    }

    // [+] NEW: Play Rack sound for the initial break
    if (!g_soundEffectsMuted) {
        PlayGameSound(g_soundBufferRack);
    }
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
                    if (!g_soundEffectsMuted) {
                        PlayGameSound(g_soundBufferCue);
                        /*std::thread([](const TCHAR* soundName) {
                            PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT);
                            }, TEXT("cue.wav")).detach();*/
                    }

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

            // --- FOOLPROOF FIX: Update rotation based on velocity ---
            float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
            if (speed > 0.01f) {
                // The amount of rotation is proportional to the distance traveled.
                // We can use the horizontal velocity component for a simple, effective rolling look.
                b.rotationAngle += b.vx / (BALL_RADIUS * 10.0f); // Adjust divisor for spin speed

                // Normalize the angle to keep it within the 0 to 2*PI range
                const float TWO_PI = 2.0f * 3.1415926535f;
                if (b.rotationAngle > TWO_PI) b.rotationAngle -= TWO_PI;
                if (b.rotationAngle < 0) b.rotationAngle += TWO_PI;
            }
            // --- End Rotation Fix ---

                        // move
            b.x += b.vx;
            b.y += b.vy;

            // --- Update rotation from distance travelled this frame ---
            // Use the magnitude of the displacement (speed) to compute angular change:
            //   deltaAngle (radians) = distance / radius
            // This produces physically-plausible rotation regardless of direction.
            /* {
                float speed = sqrtf(b.vx * b.vx + b.vy * b.vy); // distance units per frame
                if (speed > 1e-6f) {
                    float deltaAngle = speed / BALL_RADIUS; // radians
                    // subtract -> clockwise feel, you can invert if you prefer
                    b.rotationAngle -= deltaAngle;
                    // keep angle bounded to avoid large drift (optional)
                    if (b.rotationAngle > 2.0f * PI || b.rotationAngle < -2.0f * PI)
                        b.rotationAngle = fmodf(b.rotationAngle, 2.0f * PI);
                }
            }*/

            // Apply friction
            b.vx *= FRICTION;
            b.vy *= FRICTION;

            // --- ROTATION UPDATE: convert linear motion into visual rotation ---
            // Use the distance moved per frame (approx = speed since position increased by vx/vy per tick)
            // and convert to angular delta: deltaAngle = distance / radius.
            // Choose a sign so opposite directions produce opposite rotation.
            {
                const float eps = 1e-5f;
                float vx = b.vx;
                float vy = b.vy;
                float speed = sqrtf(vx * vx + vy * vy);
                if (speed > eps) {
                    // distance moved this frame (we assume velocity units per tick)
                    float distance = speed; // velocities are per-tick; scale if you have explicit dt
                    float deltaAngle = distance / BALL_RADIUS; // radians per tick

                    // Heuristic sign: prefer rightwards motion to rotate positive, leftwards negative.
                    // This is a simple robust sign determination that gives visually plausible spin.
                    float sign = (vx > 0.0f) ? 1.0f : -1.0f;

                    // A small refinement: if horizontal velocity near zero, base sign on vertical
                    if (fabsf(vx) < 0.0001f) sign = (vy > 0.0f) ? 1.0f : -1.0f;

                    b.rotationAngle += deltaAngle * sign;

                    // Keep angle bounded to avoid large values
                    if (b.rotationAngle > 2.0f * PI) b.rotationAngle = fmodf(b.rotationAngle, 2.0f * PI);
                    if (b.rotationAngle < -2.0f * PI) b.rotationAngle = fmodf(b.rotationAngle, 2.0f * PI);

                    // rotation axis = direction perpendicular to travel. Used for highlight offset.
                    b.rotationAxis = atan2f(vy, vx) + (PI * 0.5f);
                }
            }
            // --- End rotation update ---

                   // [+] NEW: Record ball path for high-precision rendering
            if (currentGameState == SHOT_IN_PROGRESS && !b.isPocketed) {
                std::vector<D2D1_POINT_2F>& trail = g_tempShotTrails[b.id];
                // Record if trail is empty OR ball moved > 1.4 pixels (sqrt(2))
                // This captures micro-turns and wall bounces accurately.
                if (trail.empty() || GetDistanceSq(trail.back().x, trail.back().y, b.x, b.y) > 2.0f) {
                    trail.push_back(D2D1::Point2F(b.x, b.y));
                }
            }

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

    bool playedWallSoundThisFrame = false;
    bool playedCollideSoundThisFrame = false;
    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& b1 = balls[i];
        if (b1.isPocketed) continue;

        // --- DEFINITIVE FIX: Prioritize Pocket Detection Over All Other Collisions ---
// As the advisory correctly states, we must check if a ball is in a pocket's area FIRST.
// If it is, we skip ALL other wall/rim physics for that ball on this frame to
// prevent it from incorrectly bouncing off the edge.
        bool skipAllWallCollisions = false;
        for (int p = 0; p < 6; ++p) {
            if (IsBallTouchingPocketMask_D2D(b1, p)) {
                skipAllWallCollisions = true;
                break; // Ball is in a pocket's hittable area, no need to check other pockets.
            }
        }

        // If the ball is in a pocket area, we jump straight to checking the next ball.
        // The CheckPockets() function, which runs in the main game loop, will handle the sinking.
        if (skipAllWallCollisions) {
            continue;
        }
        // --- END OF POCKET PRIORITY FIX ---

        // --- FOOLPROOF FIX 2: Synchronize Rim Collision with Pocketing ---
        // If a ball is already inside a pocket's mask area, we MUST skip rim collision
        // to prevent it from bouncing back out.
        bool skipRimCollisionForThisBall = false;
        /*for (int p = 0; p < 6; ++p) {
            if (IsBallTouchingPocketMask_D2D(b1, p)) {
                skipRimCollisionForThisBall = true;
                break;
            }
        }
        if (skipRimCollisionForThisBall) {
            continue; // Skip all wall/rim collision checks for this ball and move to the next.
        }*/
        // --- END OF SYNCHRONIZATION FIX ---

        // --- FOOLPROOF FIX 1: Accurate V-Cut Rim Collision Physics ---
//const float rimWidth = 15.0f;
        //const float vCutDepth = 35.0f; // How deep the V-cut is.

        // --- NEW: If this ball already overlaps a pocket mouth/area then skip rim collision.
        // Rationale: avoid reflecting balls back off the rim when they are inside the V-cut opening.
        //bool skipRimCollisionForThisBall = false;
        /*for (int p = 0; p < 6; ++p) {
            if (IsBallInPocketPolygon(b1, p) || IsBallTouchingPocketMask_D2D(b1, p)) {
                skipRimCollisionForThisBall = true;
                break;
            }
        }*/

        // If the ball is not near a pocket mouth, do the rim collision tests. Otherwise allow pocketing logic.
        if (!skipAllWallCollisions) { // We can reuse the flag from the definitive fix.
        //if (!skipRimCollisionForThisBall) {
            // Define the 12 vertices of the new inner rim boundary
            D2D1_POINT_2F vertices[12] = {
                /*0*/ D2D1::Point2F(TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE),                                 // Top-Left pocket V-cut (inner point)
                /*1*/ D2D1::Point2F(TABLE_LEFT + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS),                                 // Top-Left pocket V-cut (outer point)
                /*2*/ D2D1::Point2F(TABLE_RIGHT - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS),                                // Top-Right pocket V-cut (outer point)
                /*3*/ D2D1::Point2F(TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE),                                 // Top-Right pocket V-cut (inner point)
                /*4*/ D2D1::Point2F(TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE),                              // Bottom-Right pocket V-cut (inner point)
                /*5*/ D2D1::Point2F(TABLE_RIGHT - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS),                              // Bottom-Right pocket V-cut (outer point)
                /*6*/ D2D1::Point2F(TABLE_LEFT + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS),                               // Bottom-Left pocket V-cut (outer point)
                /*7*/ D2D1::Point2F(TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE),                               // Bottom-Left pocket V-cut (inner point)
                /*8*/ D2D1::Point2F(TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2),           // Mid-Left pocket V-cut (top point)
                /*9*/ D2D1::Point2F(TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2),           // Mid-Left pocket V-cut (bottom point)
                /*10*/ D2D1::Point2F(TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2),          // Mid-Right pocket V-cut (bottom point)
                /*11*/ D2D1::Point2F(TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2)           // Mid-Right pocket V-cut (top point)
            };

            // Define the 12 line segments that form the entire inner boundary
            D2D1_POINT_2F segments[12][2] = {
                { vertices[1], vertices[2] },   // Top rail
                { vertices[2], vertices[3] },   // Top-Right V
                { vertices[3], vertices[11] },  // Right rail (top part)
                { vertices[11], vertices[10] }, // Right rail (middle part - straight)
                { vertices[10], vertices[4] },  // Right rail (bottom part)
                { vertices[4], vertices[5] },   // Bottom-Right V
                { vertices[5], vertices[6] },   // Bottom rail
                { vertices[6], vertices[7] },   // Bottom-Left V
                { vertices[7], vertices[9] },   // Left rail (bottom part)
                { vertices[9], vertices[8] },   // Left rail (middle part - straight)
                { vertices[8], vertices[0] },   // Left rail (top part)
                { vertices[0], vertices[1] }    // Top-Left V
            };

            for (int j = 0; j < 12; ++j) { // Iterate through all 12 rim segments
                D2D1_POINT_2F p1 = segments[j][0];
                D2D1_POINT_2F p2 = segments[j][1];
                float distSq = PointToLineSegmentDistanceSq(D2D1::Point2F(b1.x, b1.y), p1, p2);

                if (distSq < BALL_RADIUS * BALL_RADIUS) {
                    float segDx = p2.x - p1.x, segDy = p2.y - p1.y;
                    float normalX = -segDy, normalY = segDx;
                    float normalLen = sqrtf(normalX * normalX + normalY * normalY);
                    if (normalLen > 1e-6f) { normalX /= normalLen; normalY /= normalLen; }

                    float dot = b1.vx * normalX + b1.vy * normalY;
                    b1.vx -= 2 * dot * normalX;
                    b1.vy -= 2 * dot * normalY;

                    b1.x += normalX * (BALL_RADIUS - sqrtf(distSq));
                    b1.y += normalY * (BALL_RADIUS - sqrtf(distSq));

                    if (!playedWallSoundThisFrame) {
                        if (!g_soundEffectsMuted) {
                            PlayGameSound(g_soundBufferWall);
                            //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                        }
                        //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                        playedWallSoundThisFrame = true;
                    }
                    break; // A ball can only hit one wall segment per frame.
                }
            }
        }
        // --- END OF V-CUT RIM COLLISION FIX ---

        /*// --- FOOLPROOF FIX 1: Inner Chamfered Rim Collision ---
        const float rimWidth = 15.0f; // This MUST match the rimWidth in DrawTable
        const float rimLeft = TABLE_LEFT + rimWidth;
        const float rimRight = TABLE_RIGHT - rimWidth;
        const float rimTop = TABLE_TOP + rimWidth;
        const float rimBottom = TABLE_BOTTOM - rimWidth;

        // Check for collision with the four segments of the inner rim
        if (b1.x - BALL_RADIUS < rimLeft && b1.x > TABLE_LEFT) {
            b1.x = rimLeft + BALL_RADIUS;
            b1.vx *= -1.0f; // Reverse horizontal velocity
            if (!playedWallSoundThisFrame) {
                std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                playedWallSoundThisFrame = true;
            }
        }
        if (b1.x + BALL_RADIUS > rimRight && b1.x < TABLE_RIGHT) {
            b1.x = rimRight - BALL_RADIUS;
            b1.vx *= -1.0f;
            if (!playedWallSoundThisFrame) {
                std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                playedWallSoundThisFrame = true;
            }
        }
        if (b1.y - BALL_RADIUS < rimTop && b1.y > TABLE_TOP) {
            b1.y = rimTop + BALL_RADIUS;
            b1.vy *= -1.0f; // Reverse vertical velocity
            if (!playedWallSoundThisFrame) {
                std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                playedWallSoundThisFrame = true;
            }
        }
        if (b1.y + BALL_RADIUS > rimBottom && b1.y < TABLE_BOTTOM) {
            b1.y = rimBottom - BALL_RADIUS;
            b1.vy *= -1.0f;
            if (!playedWallSoundThisFrame) {
                std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                playedWallSoundThisFrame = true;
            }
        }
        // --- END OF RIM COLLISION FIX ---*/

        bool nearPocket[6];
        for (int p = 0; p < 6; ++p) {
            // Use correct radius to check if a ball is near a pocket to prevent wall collisions
            float physicalPocketRadius = ((p == 1 || p == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS) * 1.05f;
            float checkRadiusSq = (physicalPocketRadius + BALL_RADIUS) * (physicalPocketRadius + BALL_RADIUS) * 1.1f;
            nearPocket[p] = GetDistanceSq(b1.x, b1.y, pocketPositions[p].x, pocketPositions[p].y) < checkRadiusSq;
        }

        bool nearTopLeftPocket = nearPocket[0];
        bool nearTopMidPocket = nearPocket[1];
        bool nearTopRightPocket = nearPocket[2];
        bool nearBottomLeftPocket = nearPocket[3];
        bool nearBottomMidPocket = nearPocket[4];
        bool nearBottomRightPocket = nearPocket[5];
        bool collidedWallThisBall = false;
        if (b1.x - BALL_RADIUS < left) {
            if (!nearTopLeftPocket && !nearBottomLeftPocket) {
                b1.x = left + BALL_RADIUS; b1.vx *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    if (!g_soundEffectsMuted) {
                        std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    }
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true;
            }
        }
        if (b1.x + BALL_RADIUS > right) {
            if (!nearTopRightPocket && !nearBottomRightPocket) {
                b1.x = right - BALL_RADIUS; b1.vx *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    if (!g_soundEffectsMuted) {
                        std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    }
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true;
            }
        }
        if (b1.y - BALL_RADIUS < top) {
            if (!nearTopLeftPocket && !nearTopMidPocket && !nearTopRightPocket) {
                b1.y = top + BALL_RADIUS; b1.vy *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    if (!g_soundEffectsMuted) {
                        std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    }
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true;
            }
        }
        if (b1.y + BALL_RADIUS > bottom) {
            if (!nearBottomLeftPocket && !nearBottomMidPocket && !nearBottomRightPocket) {
                b1.y = bottom - BALL_RADIUS; b1.vy *= -1.0f; collidedWallThisBall = true;
                if (!playedWallSoundThisFrame) {
                    if (!g_soundEffectsMuted) {
                        std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("wall.wav")).detach();
                    }
                    playedWallSoundThisFrame = true;
                }
                if (cueHitObjectBallThisShot) railHitAfterContact = true;
            }
        }
        if (collidedWallThisBall) {
            if (b1.x <= left + BALL_RADIUS || b1.x >= right - BALL_RADIUS) { b1.vy += cueSpinX * b1.vx * 0.05f; }
            if (b1.y <= top + BALL_RADIUS || b1.y >= bottom - BALL_RADIUS) { b1.vx -= cueSpinY * b1.vy * 0.05f; }
            cueSpinX *= 0.7f; cueSpinY *= 0.7f;
        }
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
                b1.x -= overlap * 0.5f * nx; b1.y -= overlap * 0.5f * ny;
                b2.x += overlap * 0.5f * nx; b2.y += overlap * 0.5f * ny;
                float rvx = b1.vx - b2.vx; float rvy = b1.vy - b2.vy;
                float velAlongNormal = rvx * nx + rvy * ny;
                if (velAlongNormal > 0) {
                    if (!playedCollideSoundThisFrame) {
                        if (!g_soundEffectsMuted) {
                            PlayGameSound(g_soundBufferHit);
                            //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("poolballhit.wav")).detach();
                        }
                        //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("poolballhit.wav")).detach();
                        playedCollideSoundThisFrame = true;
                    }
                    if (firstHitBallIdThisShot == -1) {
                        if (b1.id == 0) {
                            firstHitBallIdThisShot = b2.id;
                            cueHitObjectBallThisShot = true;
                        }
                        else if (b2.id == 0) {
                            firstHitBallIdThisShot = b1.id;
                            cueHitObjectBallThisShot = true;
                        }
                    }
                    else if (b1.id == 0 || b2.id == 0) {
                        cueHitObjectBallThisShot = true;
                    }
                    float impulse = velAlongNormal;
                    b1.vx -= impulse * nx; b1.vy -= impulse * ny;
                    b2.vx += impulse * nx; b2.vy += impulse * ny;
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
        }
    }
}


bool CheckPockets() {
    bool anyPocketed = false;
    bool playedPocketSound = false; // Ensures sound plays only once per frame

    for (auto& b : balls) {
        if (b.isPocketed) continue;

        for (int p = 0; p < 6; ++p) {
            // --- FOOLPROOF FIX 3: Use the Unified Mask for Pocket Detection ---
            // This is now the ONLY test for whether a ball is pocketed.
            if (IsBallTouchingPocketMask_D2D(b, p)) {

                // [+] NEW: Record history
                // We assume the current player (or the one whose turn just finished) "owns" this event
                // This is an approximation for simultaneous potting, but sufficient for Undo.
                bool playerWasOn8 = IsPlayerOnEightBall(currentPlayer);
                g_pocketHistory.push_back({ b.id, currentPlayer, playerWasOn8 });

                // Mark the ball as pocketed
                b.isPocketed = true;
                b.vx = b.vy = 0.0f; // Stop its movement
                pocketedThisTurn.push_back(b.id);
                anyPocketed = true;

                if (b.id == 8) {
                    lastEightBallPocketIndex = p;
                }

                // Play the pocket sound (only once per physics update)
                if (!playedPocketSound) {
                    if (!g_soundEffectsMuted) {
                        PlayGameSound(g_soundBufferPocket);
                        //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("pocket.wav")).detach();
                    }
                    //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("pocket.wav")).detach();
                    playedPocketSound = true;
                }

                break; // Ball is pocketed, no need to check other pockets for it.
            }
        }
    }
    return anyPocketed;
}

// start gemini fallthrough final
/*
// Replace older CheckPockets() with this stricter test which uses the black-area helper.
bool CheckPockets() {
    bool anyPocketed = false;
    bool playedPocketSound = false;

    for (auto& b : balls) {
        if (b.isPocketed) continue;

        for (int p = 0; p < 6; ++p) {
            if (!IsBallTouchingPocketMask_D2D(b, p)) continue;

            // Mark pocketed
            if (b.id == 8) {
                lastEightBallPocketIndex = p;
            }
            b.isPocketed = true;
            b.vx = b.vy = 0.0f;
            pocketedThisTurn.push_back(b.id);
            anyPocketed = true;

            if (!playedPocketSound) {
                // Play pocket sound once per CheckPockets() pass
                std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("pocket.wav")).detach();
                playedPocketSound = true;
            }
            break; // this ball is pocketed; continue with next ball
        }
    }
    return anyPocketed;
}
*/
// end gemini fallthrough final


/*bool CheckPockets() {
    bool anyPocketed = false;
    bool ballPocketedThisCheck = false;
    for (auto& b : balls) {
        if (b.isPocketed) continue;
        for (int p = 0; p < 6; ++p) {
            D2D1_POINT_2F center = pocketPositions[p];
            float dx = b.x - center.x;
            float dy = b.y - center.y;

            // Use the correct radius for the pocket being checked
            float currentPocketRadius = (p == 1 || p == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;

            bool isInPocket = false;
            // The pocketing conditions now use the correct radius
            if (p == 1) {
                if (dy >= 0 && dx * dx + dy * dy <= currentPocketRadius * currentPocketRadius)
                    isInPocket = true;
            }
            else if (p == 4) {
                if (dy <= 0 && dx * dx + dy * dy <= currentPocketRadius * currentPocketRadius)
                    isInPocket = true;
            }
            else {
                if (dx * dx + dy * dy <= currentPocketRadius * currentPocketRadius)
                    isInPocket = true;
            }

            if (isInPocket) {
                if (b.id == 8) {
                    lastEightBallPocketIndex = p;
                }
                b.isPocketed = true;
                b.vx = b.vy = 0.0f;
                pocketedThisTurn.push_back(b.id);
                anyPocketed = true;
                if (!ballPocketedThisCheck) {
                    std::thread([](const TCHAR* soundName) {
                        PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT);
                        }, TEXT("pocket.wav")).detach();
                        ballPocketedThisCheck = true;
                }
                break;
            }
        }
    }
    return anyPocketed;
}*/


bool AreBallsMoving() {
    for (size_t i = 0; i < balls.size(); ++i) {
        if (!balls[i].isPocketed && (balls[i].vx != 0 || balls[i].vy != 0)) {
            return true;
        }
    }
    return false;
}

// NEW 9-Ball helper
void UpdateLowestBall() {
    for (int id = 1; id <= 9; ++id) {
        Ball* b = GetBallById(id);
        if (b && !b->isPocketed) {
            lowestBallOnTable = id;
            return;
        }
    }
    lowestBallOnTable = 9; // Fallback, only 9-ball is left
}

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


// [+] NEW: Captures the current game state for the Retry feature
void SaveStateForUndo() {
    g_undoBackup.balls = balls;
    g_undoBackup.gameState = currentGameState;
    g_undoBackup.currentPlayer = currentPlayer;
    g_undoBackup.p1Info = player1Info;
    g_undoBackup.p2Info = player2Info;
    g_undoBackup.p1ScoreStraight = player1StraightPoolScore;
    g_undoBackup.p2ScoreStraight = player2StraightPoolScore;
    g_undoBackup.ballsOnTable = ballsOnTableCount;
    g_undoBackup.lowestBall = lowestBallOnTable;
    g_undoBackup.p1Fouls = player1ConsecutiveFouls;
    g_undoBackup.p2Fouls = player2ConsecutiveFouls;
    g_undoBackup.pushOut = isPushOutAvailable;
    g_undoBackup.calledP1 = calledPocketP1;
    g_undoBackup.calledP2 = calledPocketP2;
    g_undoBackup.pocketMsg = pocketCallMessage;
    g_undoBackup.foul = foulCommitted;
    g_undoBackup.openingBreak = isOpeningBreakShot;
    g_undoBackup.aiTurnPending = aiTurnPending;
    g_hasUndo = true;
}

// [+] NEW: Restores the game state
void RestoreStateFromUndo() {
    if (!g_hasUndo) return;

    balls = g_undoBackup.balls;
    currentGameState = g_undoBackup.gameState;
    currentPlayer = g_undoBackup.currentPlayer;
    player1Info = g_undoBackup.p1Info;
    player2Info = g_undoBackup.p2Info;
    player1StraightPoolScore = g_undoBackup.p1ScoreStraight;
    player2StraightPoolScore = g_undoBackup.p2ScoreStraight;
    ballsOnTableCount = g_undoBackup.ballsOnTable;
    lowestBallOnTable = g_undoBackup.lowestBall;
    player1ConsecutiveFouls = g_undoBackup.p1Fouls;
    player2ConsecutiveFouls = g_undoBackup.p2Fouls;
    isPushOutAvailable = g_undoBackup.pushOut;
    calledPocketP1 = g_undoBackup.calledP1;
    calledPocketP2 = g_undoBackup.calledP2;
    pocketCallMessage = g_undoBackup.pocketMsg;
    foulCommitted = g_undoBackup.foul;
    isOpeningBreakShot = g_undoBackup.openingBreak;
    aiTurnPending = g_undoBackup.aiTurnPending;

    // Reset transient shot variables
    shotPower = 0.0f;
    cueSpinX = 0.0f;
    cueSpinY = 0.0f;
    isAiming = false;
    pocketedThisTurn.clear();
}


// --- Game Logic ---

void ApplyShot(float power, float angle, float spinX, float spinY) {
    Ball* cueBall = GetCueBall();
    if (cueBall) {

        // [+] NEW: Handle Undo Logic
        // Only save state if a Human is shooting.
        // If CPU shoots, invalidate previous undo to prevent exploits.
        if (!isPlayer2AI || currentPlayer == 1) {
            SaveStateForUndo();
        }
        else {
            g_hasUndo = false;
        }

        // [+] NEW: Start recording paths for this shot
        g_lastShotTrails.clear(); // <--- ADD THIS: Clear old history immediately when new shot starts
        g_tempShotTrails.clear();
        for (const auto& ball : balls) {
            if (!ball.isPocketed) {
                g_tempShotTrails[ball.id].push_back(D2D1::Point2F(ball.x, ball.y));
            }
        }

        // --- Play Cue Strike Sound (Threaded) ---
        if (power > 0.1f) { // Only play if it's an audible shot
            if (!g_soundEffectsMuted) {
                PlayGameSound(g_soundBufferCue);
                //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
            }
            //std::thread([](const TCHAR* soundName) { PlaySound(soundName, NULL, SND_FILENAME | SND_NODEFAULT); }, TEXT("cue.wav")).detach();
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
    bool cueBallPocketedThisTurn = false; // Renamed to avoid conflict
    bool eightBallPocketedThisTurn = false; // Renamed
    bool nineBallPocketedThisTurn = false; // NEW variable for 9-Ball
    bool playerContinuesTurn = false;
    int numberedBallsPocketedThisTurn = 0;

    // --- Step 1: Check pocketed balls ---
    for (int id : pocketedThisTurn) {
        Ball* b = GetBallById(id);
        if (currentGameType == GameType::NINE_BALL && b && b->id == 9) nineBallPocketedThisTurn = true;
        if (!b) continue;

        if (b->id == 0) {
            cueBallPocketedThisTurn = true;
        }
        else if (b->id == 8 && currentGameType == GameType::EIGHT_BALL_MODE) { // Only track 8-ball pocketing in 8-ball mode
            eightBallPocketedThisTurn = true;
        }
        else if (b->id > 0 && b->id <= 15) { // Any numbered ball
            numberedBallsPocketedThisTurn++;
            if (currentGameType != GameType::NINE_BALL || g_isPracticeMode) ballsOnTableCount--; // Decrement balls on table count (9-ball handles this differently, unless practice)

            // Score for Straight Pool
            if (currentGameType == STRAIGHT_POOL) {
                if (currentPlayer == 1) player1StraightPoolScore++;
                else if (!g_isPracticeMode) player2StraightPoolScore++; // Don't score for P2 in practice
                playerContinuesTurn = true; // Player continues in Straight Pool if they pocket any numbered ball (Practice mode handles this separately)
            }
            // Score/Continuation logic for 8-Ball (needs assigned types)
            else if (currentGameType == GameType::EIGHT_BALL_MODE) {
                PlayerInfo& shootingPlayer = (currentPlayer == 1) ? player1Info : player2Info;
                if (shootingPlayer.assignedType != BallType::NONE && b->type == shootingPlayer.assignedType) {
                    // Must update the count *before* checking game over conditions
                    if (b->id != 8) { // Don't double-count 8-ball here
                        shootingPlayer.ballsPocketedCount++;
                    }
                    playerContinuesTurn = true; // Continues turn if own ball pocketed
                }
            }
            // Continuation logic for 9-Ball
            else if (currentGameType == GameType::NINE_BALL) {
                if (b->id != 9) { // Pocketing any ball *except* the 9-ball
                    playerContinuesTurn = true;
                }
                // If 9-ball is pocketed, game over is checked later
            }
        }
    }

    // --- PRACTICE MODE LOGIC ---
    if (g_isPracticeMode) {
        if (cueBallPocketedThisTurn) {
            RespawnCueBall(false);
            // RespawnCueBall sets state to BALL_IN_HAND_P1
        }
        else if (ballsOnTableCount == 0) {
            ReRackPracticeMode();
        }
        else {
            // Always let the player shoot again!
            currentGameState = PLAYER1_TURN;
        }
        pocketedThisTurn.clear();
        return;
    }
    // --- END PRACTICE MODE LOGIC ---

    // --- Step 2: Check Fouls (Must happen before checking Game Over) ---
    bool turnFoul = false;
    if (cueBallPocketedThisTurn) {
        turnFoul = true;
    }
    else if (isPushOutShot) {
        // Only foul on a push-out is a scratch, which is already handled.
        // No foul for not hitting lowest ball or rail.
        turnFoul = false;
        isPushOutShot = false; // The push-out shot has been taken
    }
    else {
        Ball* firstHit = GetBallById(firstHitBallIdThisShot);
        if (!firstHit) { // Rule: Hitting nothing is a foul
            turnFoul = true;
        }
        // 8-Ball specific foul checks
        else if (currentGameType == GameType::EIGHT_BALL_MODE) {
            if (player1Info.assignedType != BallType::NONE) { // Colors are assigned
                PlayerInfo& shootingPlayer = (currentPlayer == 1) ? player1Info : player2Info;
                bool wasOnEightBall = (shootingPlayer.assignedType != BallType::NONE && (shootingPlayer.ballsPocketedCount) >= 7);

                if (wasOnEightBall) {
                    if (firstHit->id != 8) turnFoul = true; // Must hit 8-ball first when on it
                }
                else {
                    if (firstHit->type != shootingPlayer.assignedType && firstHit->id != 8) {
                        turnFoul = true;
                    }
                    else if (firstHit->id == 8) { // Hitting 8-ball first when not on it
                        turnFoul = true;
                    }
                }
            }
        }
        // 9-Ball specific foul checks
        else if (currentGameType == GameType::NINE_BALL) {
            if (firstHit->id != lowestBallOnTable) {
                turnFoul = true; // Must hit lowest ball first
            }
            else if (!railHitAfterContact && numberedBallsPocketedThisTurn == 0 && !nineBallPocketedThisTurn) {
                turnFoul = true; // Legal hit, but no ball pocketed AND no rail hit after contact
            }
        }
        // Straight Pool has no "wrong ball first" foul.
    }

    foulCommitted = turnFoul;

    // --- Step 3: Check Game Over (Now that foul flag is set) ---
    if (currentGameType == GameType::EIGHT_BALL_MODE && eightBallPocketedThisTurn) {
        CheckGameOverConditions(true, cueBallPocketedThisTurn);
    }
    else if (currentGameType == GameType::NINE_BALL && nineBallPocketedThisTurn) {
        CheckGameOverConditions(true, cueBallPocketedThisTurn);
    }
    else if (currentGameType == GameType::STRAIGHT_POOL) {
        CheckGameOverConditions(false, cueBallPocketedThisTurn);
    }

    if (currentGameState == GAME_OVER) {
        pocketedThisTurn.clear();
        return;
    }

    // --- Step 4: State Transitions ---
    if (foulCommitted) {
        if (turnFoul && foulDisplayCounter <= 0) foulDisplayCounter = 120; // Show "FOUL!" text

        // [+] NEW: Play Foul Sound
        if (!g_soundEffectsMuted) {
            PlayGameSound(g_soundBufferFoul);
        }

        if (currentGameType == GameType::NINE_BALL) {
            if (currentPlayer == 1) player1ConsecutiveFouls++;
            else player2ConsecutiveFouls++;

            // Check 3-foul rule
            if (player1ConsecutiveFouls >= 3) {
                FinalizeGame(2, 1, L"3 Consecutive Fouls");
                pocketedThisTurn.clear();
                return;
            }
            if (player2ConsecutiveFouls >= 3) {
                FinalizeGame(1, 2, L"3 Consecutive Fouls");
                pocketedThisTurn.clear();
                return;
            }
        }

        SwitchTurns();
        RespawnCueBall(false); // Ball-in-hand anywhere
    }
    else if (currentGameType == GameType::EIGHT_BALL_MODE && player1Info.assignedType == BallType::NONE && numberedBallsPocketedThisTurn > 0) {
        // 8-Ball: Assign types on break or first legal pocket
        BallType firstType = BallType::NONE;
        for (int id : pocketedThisTurn) {
            Ball* b = GetBallById(id);
            if (b && b->id > 0 && b->id != 8) {
                firstType = (b->id < 8) ? BallType::SOLID : BallType::STRIPE;
                break;
            }
        }
        if (firstType != BallType::NONE) {
            AssignPlayerBallTypes(firstType, true); // Assign and credit the shooter
        }
        playerContinuesTurn = true; // Ensure turn continues
        CheckAndTransitionToPocketChoice(currentPlayer); // Check if now on 8-ball

    }
    else if (currentGameType == STRAIGHT_POOL && ballsOnTableCount == 1) {
        // Straight Pool: Trigger re-rack
        ReRackStraightPool();
    }
    else if (playerContinuesTurn) {
        // Player legally pocketed a ball
        if (currentPlayer == 1) player1ConsecutiveFouls = 0; // Reset foul count on legal pocket
        else player2ConsecutiveFouls = 0;

        if (isOpeningBreakShot && currentGameType == GameType::NINE_BALL) {
            isPushOutAvailable = true; // Turn continues, but next player gets push-out option
            SwitchTurns(); // Turn *does* switch, opponent decides push-out
        }
        else {
            if (currentGameType == GameType::EIGHT_BALL_MODE) {
                CheckAndTransitionToPocketChoice(currentPlayer);
            }
            else if (currentGameType == GameType::NINE_BALL) {
                UpdateLowestBall();
                currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
            }
            else { // Straight Pool
                currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
                if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
            }
        }
    }
    else { // Missed shot or push-out
        if (isOpeningBreakShot && currentGameType == GameType::NINE_BALL) {
            isPushOutAvailable = true; // Legal break, no pocket, opponent gets push-out
        }

        // Missed or pocketed opponent's ball (in 8-ball)
        SwitchTurns();
    }

    if (isOpeningBreakShot) isOpeningBreakShot = false; // Consume break shot flag
    pocketedThisTurn.clear(); // Clear for the next turn

       // [+] NEW: Finalize trails for the completed shot to be drawn on the next turn
    g_lastShotTrails = g_tempShotTrails;
    g_tempShotTrails.clear();
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

// [+] NEW: Instantly simulates the rest of the shot
void SkipCurrentShot() {
    // Only skip if balls are currently in motion
    if (currentGameState != SHOT_IN_PROGRESS) return;

    // Temporarily mute to prevent a "machine gun" sound effect during rapid simulation
    bool wasMuted = g_soundEffectsMuted;
    g_soundEffectsMuted = true;

    // Loop physics until all balls come to a stop or a safety limit is hit
    // (5000 steps is roughly 80 seconds of simulated game time)
    for (int i = 0; i < 5000 && AreBallsMoving(); ++i) {
        UpdatePhysics();
        CheckCollisions();
        CheckPockets();
    }

    g_soundEffectsMuted = wasMuted;

    // Finalize the turn results and transition game states
    ProcessShotResults();
}

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


// --- CORRECTED: Centralized Game Finalization Function ---
// This function is now the ONLY way a game should end. It handles the
// game over message, statistics updates, and saving the settings file.
void FinalizeGame(int winner, int loser, const std::wstring& reason) {
    // 1. Set the game over message
    PlayerInfo& winnerInfo = (winner == 1) ? player1Info : player2Info;
    gameOverMessage = winnerInfo.name + L" Wins! " + reason;

    // [+] NEW: Play Win/Loss Sound Logic
    if (!g_soundEffectsMuted) {
        // If playing against AI, distinguish between Winning and Losing
        if (gameMode == HUMAN_VS_AI) {
            if (winner == 1) {
                // Human Won -> Play Won Sound
                PlayGameSound(g_soundBufferWon);
            }
            else {
                // AI Won (Human Lost) -> Play Loss Sound
                PlayGameSound(g_soundBufferLoss);
            }
        }
        else {
            // Human vs Human (or Practice): Always play Won sound for the victor
            PlayGameSound(g_soundBufferWon);
        }
    }

    // 2. Update statistics based on the game mode and outcome
    if (gameMode == HUMAN_VS_AI) {
        // --- FOOLPROOF FIX: Use a strict if/else block ---
        // This guarantees that ONLY the winner's stats are updated.
        if (winner == 1) { // Human (P1) is the winner
            g_stats.p1_wins++;
            // The AI's loss count is implicitly tracked by P1's wins.
        }
        else { // AI (P2) is the winner
            g_stats.ai_wins++;
            g_stats.p1_losses++;
        }
    }
    else { // Human vs Human
        g_stats.human_vs_human_games++;
    }

    // 3. Save the updated statistics to the file immediately
    SaveSettings();

    // 4. Set the final game state
    currentGameState = GAME_OVER;
}

/*// --- NEW: Centralized Game Finalization Function ---
// This function is now the ONLY way a game should end. It handles the
// game over message, statistics updates, and saving the settings file.
void FinalizeGame(int winner, int loser, const std::wstring& reason) {
    // 1. Set the game over message
    PlayerInfo& winnerInfo = (winner == 1) ? player1Info : player2Info;
    gameOverMessage = winnerInfo.name + L" Wins! " + reason;

    // 2. Update statistics based on the game mode and outcome
    if (gameMode == HUMAN_VS_AI) {
        if (winner == 1) { // Human (P1) wins
            g_stats.p1_wins++;
            // AI's losses are implicitly tracked by P1's wins.
        }
        else { // AI (P2) wins
            g_stats.ai_wins++;
            g_stats.p1_losses++;
        }
    }
    else { // Human vs Human
        g_stats.human_vs_human_games++;
    }

    // 3. Save the updated statistics to the file immediately
    SaveSettings();

    // 4. Set the final game state
    currentGameState = GAME_OVER;
}*/

// --- REFACTORED: Determines winner/loser and calls the centralized FinalizeGame function ---
void CheckGameOverConditions(bool eightBallPocketed, bool cueBallPocketed) {
    if (g_isPracticeMode) return; // No game over in practice mode
    if (currentGameType == GameType::EIGHT_BALL_MODE) {
        // --- 8-Ball Game Over Logic --- (eightBallPocketed means 8-ball)
        if (!eightBallPocketed) return; // Only check if 8-ball was pocketed this turn

        int shooterId = currentPlayer;
        int opponentId = (currentPlayer == 1) ? 2 : 1;
        PlayerInfo& shooterInfo = (shooterId == 1) ? player1Info : player2Info;

        // Handle 8-ball on break (re-spot) - This check should ideally be in ProcessShotResults *before* calling this
        if (player1Info.assignedType == BallType::NONE) {
            Ball* b = GetBallById(8);
            if (b) { b->isPocketed = false; b->x = RACK_POS_X; b->y = RACK_POS_Y; b->vx = b->vy = 0; }
            // Don't end game, foul may apply if cue was also pocketed (handled in ProcessShotResults)
            // Need to prevent this function from proceeding if it was break. Add check in ProcessShotResults?
            // Let's assume ProcessShotResults handles the break case and doesn't call this func if 8-ball pocketed on break.
            return; // Avoid game over on break
        }

        int calledPocket = (shooterId == 1) ? calledPocketP1 : calledPocketP2;
        int actualPocket = lastEightBallPocketIndex; // Updated in CheckPockets

        // Check if player had cleared their group *before* this shot finished.
        // ballsPocketedCount includes balls made *this* turn.
        int ballsNeeded = 7;
        bool clearedGroupBeforeShot = (shooterInfo.ballsPocketedCount >= ballsNeeded);

        bool isLegalWin =
            clearedGroupBeforeShot &&             // Must have cleared group before this shot
            (calledPocket != -1) &&               // Must have called a pocket
            (calledPocket == actualPocket) &&     // Must have gone in the called pocket
            (!cueBallPocketed);                   // Must not have scratched

        std::wstring reason = L"";
        if (cueBallPocketed) reason = L"(Scratched on 8-Ball)";
        else if (!clearedGroupBeforeShot) reason = L"(8-Ball Pocketed Early)";
        else if (calledPocket == -1) reason = L"(Pocket Not Called)";
        else if (calledPocket != actualPocket) reason = L"(8-Ball in Wrong Pocket)";

        if (isLegalWin) {
            FinalizeGame(shooterId, opponentId, L""); // Legal win
        }
        else {
            FinalizeGame(opponentId, shooterId, reason); // Illegal shot, opponent wins
        }

    }
    else if (currentGameType == GameType::NINE_BALL) {
        // --- 9-Ball Game Over Logic --- (eightBallPocketed means 9-ball)
        if (!eightBallPocketed) return; // Not a game-ending shot

        if (foulCommitted) {
            // 9-ball was pocketed, but it was a foul shot (e.g., scratch, wrong ball first)
            RespawnBall(9); // Re-spot the 9-ball
            // Foul is handled by ProcessShotResults (ball-in-hand, etc.)
        }
        else {
            // Legal 9-Ball pocket. This is a WIN.
            int shooterId = currentPlayer;
            int opponentId = (currentPlayer == 1) ? 2 : 1;
            FinalizeGame(shooterId, opponentId, L"Pocketed the 9-Ball");
        }
    }
    else if (currentGameType == STRAIGHT_POOL) {
        // --- Straight Pool Game Over Logic ---
        if (player1StraightPoolScore >= targetScoreStraightPool) {
            FinalizeGame(1, 2, L"Reached Target Score");
        }
        else if (player2StraightPoolScore >= targetScoreStraightPool) {
            FinalizeGame(2, 1, L"Reached Target Score");
        }
        // Otherwise, game continues
    }
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
    currentPlayer = (currentPlayer == 1) ? 2 : 1;
    isAiming = false;
    shotPower = 0.0f;
    currentlyHoveredPocket = -1; // Reset hover

    // Reset 8-ball calls (only relevant for 8-ball, but safe to clear always)
    calledPocketP1 = -1;
    calledPocketP2 = -1;
    pocketCallMessage.clear();

    // Foul Handling (Ball-in-hand overrides) - applies to both modes
    if (foulCommitted)
    {
        if (currentPlayer == 1) { // P1 gets ball in hand
            currentGameState = BALL_IN_HAND_P1;
            aiTurnPending = false;
        }
        else { // P2 gets ball in hand
            currentGameState = BALL_IN_HAND_P2;
            aiTurnPending = isPlayer2AI; // Trigger AI placement if needed
        }
        foulCommitted = false; // Consume foul flag
        return;
    }

    // Normal Turn Transition (No Foul)
    if (currentGameType == GameType::EIGHT_BALL_MODE) {
        CheckAndTransitionToPocketChoice(currentPlayer); // Check if new player is on 8-ball
    }
    else if (currentGameType == GameType::NINE_BALL) {
        UpdateLowestBall();
        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (currentPlayer == 2 && isPlayer2AI) aiTurnPending = true;
    }
    else { // STRAIGHT_POOL
        currentGameState = (currentPlayer == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (currentPlayer == 2 && isPlayer2AI) {
            aiTurnPending = true; // Trigger AI's turn
        }
    }
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
    float left = TABLE_LEFT + WOODEN_RAIL_THICKNESS + BALL_RADIUS;
    float right = TABLE_RIGHT - WOODEN_RAIL_THICKNESS - BALL_RADIUS;
    float top = TABLE_TOP + WOODEN_RAIL_THICKNESS + BALL_RADIUS;
    float bottom = TABLE_BOTTOM - WOODEN_RAIL_THICKNESS - BALL_RADIUS;

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

void CheckAndTransitionToPocketChoice(int playerID) {
    bool needsToCall = IsPlayerOnEightBall(playerID);

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
        // Player does not need to call a pocket, proceed to normal turn.
        pocketCallMessage = L"";
        currentGameState = (playerID == 1) ? PLAYER1_TURN : PLAYER2_TURN;
        if (playerID == 2 && isPlayer2AI) {
            aiTurnPending = true;
        }
    }
}

//template here


// --- CPU Ball?in?Hand Placement --------------------------------
// --- REPLACED: Strategic Cue Ball Placement ---
// Scans the table to find the optimal position for the cue ball after a foul.
void AIPlaceCueBall() {
    Ball* cue = GetCueBall();
    if (!cue) return;

    AIShotInfo bestOverallShot;
    bestOverallShot.score = -99999.0f; // Use a very low starting score
    D2D1_POINT_2F bestCuePos = { 0, 0 };
    bool foundValidPlacement = false;

    // Define a grid to check for potential cue ball positions.
    // Grid density changes with AI difficulty for faster/smarter placement.
    int gridSize = 10; // Medium difficulty
    if (aiDifficulty == EASY) gridSize = 7;
    if (aiDifficulty == HARD) gridSize = 16;

    float stepX = TABLE_WIDTH / gridSize;
    float stepY = TABLE_HEIGHT / gridSize;

    // Iterate over the grid to find the best spot
    for (int i = 0; i <= gridSize; ++i) {
        for (int j = 0; j <= gridSize; ++j) {
            float candidateX = TABLE_LEFT + i * stepX;
            float candidateY = TABLE_TOP + j * stepY;

            // Ensure the candidate spot is valid (not on another ball or out of bounds)
            if (!IsValidCueBallPosition(candidateX, candidateY, false)) {
                continue;
            }

            // Temporarily move the cue ball to this candidate spot to evaluate shots from there
            float originalX = cue->x;
            float originalY = cue->y;
            cue->x = candidateX;
            cue->y = candidateY;

            // Find the best possible shot the AI can make from this spot
            // Determine the best shot based on the current game type
            AIShotInfo shotFromHere;

            if (currentGameType == GameType::EIGHT_BALL_MODE) {
                shotFromHere = AIFindBestShot_8Ball();
            }
            else { // Assumes STRAIGHT_POOL if not 8-Ball
                shotFromHere = AIFindBestShot_StraightPool();
            }
            //AIShotInfo shotFromHere = AIFindBestShot();

            // If the shot from here is better than any found so far, store it
            if (shotFromHere.possible && shotFromHere.score > bestOverallShot.score) {
                bestOverallShot = shotFromHere;
                bestCuePos = { candidateX, candidateY };
                foundValidPlacement = true;
            }

            // Restore the cue ball's position for the next grid check
            cue->x = originalX;
            cue->y = originalY;
        }
    }

    // After checking all spots, move the cue ball to the best one found
    if (foundValidPlacement) {
        cue->x = bestCuePos.x;
        cue->y = bestCuePos.y;
    }
    else {
        // Fallback if no shots are possible from anywhere (highly unlikely)
        // Use the old RespawnCueBall logic to just find any valid spot.
        RespawnCueBall(false);
    }

    cue->vx = 0;
    cue->vy = 0;
}

/*// Moves the cue ball to a legal "ball in hand" position for the AI.
void AIPlaceCueBall() {
    Ball* cue = GetCueBall();
    if (!cue) return;

    // Simple strategy: place back behind the headstring at the standard break spot
    cue->x = TABLE_LEFT + TABLE_WIDTH * 0.15f;
    cue->y = RACK_POS_Y;
    cue->vx = cue->vy = 0.0f;
}*/

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

// --- BEGIN: ENHANCED AI LOGIC ---

// --- REPLACED: Foolproof Bank Shot Calculation (v2) ---
// --- Modify AIFindBankShots (or create AIFindBankShots_StraightPool) ---
// If adapting, it needs to use EvaluateShot_StraightPool for scoring.
// Let's adapt the existing one with an if check:
void AIFindBankShots(Ball* targetBall, std::vector<AIShotInfo>& possibleShots) {
    if (!targetBall) return;
    Ball* cue = GetCueBall();
    if (!cue) return;

    // --- NEW: Use the EXACT 12-segment chamfered rim geometry from physics ---
    D2D1_POINT_2F vertices[12] = {
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE }, { TABLE_LEFT + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS },
        { TABLE_RIGHT - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE }, { TABLE_RIGHT - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS },
        { TABLE_LEFT + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE },
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2 }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2 },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2 }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2 }
    };
    D2D1_POINT_2F segments[12][2] = {
        { vertices[1], vertices[2] }, { vertices[2], vertices[3] },
        { vertices[3], vertices[11] }, { vertices[11], vertices[10] },
        { vertices[10], vertices[4] }, { vertices[4], vertices[5] },
        { vertices[5], vertices[6] }, { vertices[6], vertices[7] },
        { vertices[7], vertices[9] }, { vertices[9], vertices[8] },
        { vertices[8], vertices[0] }, { vertices[0], vertices[1] }
    };
    // --- END NEW GEOMETRY ---

    D2D1_POINT_2F targetPos = { targetBall->x, targetBall->y };

    for (int p = 0; p < 6; ++p) { // For each pocket
        D2D1_POINT_2F pocketPos = pocketPositions[p];
        for (int r = 0; r < 12; ++r) { // For each of the 12 rail segments
            D2D1_POINT_2F p1 = segments[r][0];
            D2D1_POINT_2F p2 = segments[r][1];

            // Reflect pocket center across the line of the rail segment
            // (This part is complex, simplified for this diff, but assumes reflection works)
            // Simplified: use a virtual pocket based on simple horizontal/vertical lines for now
            // A full implementation would reflect across the arbitrary line p1-p2.
            // For this fix, let's use the old virtual pocket logic but check against new segments.

            D2D1_POINT_2F virtualPocket, bouncePoint;
            float segDx = p2.x - p1.x, segDy = p2.y - p1.y;

            if (fabsf(segDx) > fabsf(segDy)) { // More horizontal rail
                virtualPocket = { pocketPos.x, 2 * p1.y - pocketPos.y };
            }
            else { // More vertical rail
                virtualPocket = { 2 * p1.x - pocketPos.x, pocketPos.y };
            }

            if (!LineSegmentIntersection(targetPos, virtualPocket, p1, p2, bouncePoint)) {
                continue; // No intersection on this *segment*
            }

            // Now we have a valid bouncePoint on a valid chamfered rail segment
            D2D1_POINT_2F ghostBallForBank = CalculateGhostBallPos(targetBall, bouncePoint); // Aim for the bounce point

            // Path Checks (same for both modes)
            if (IsPathClear({ cue->x, cue->y }, ghostBallForBank, cue->id, targetBall->id) &&
                IsPathClear(targetPos, bouncePoint, targetBall->id, -1) &&
                IsPathClear(bouncePoint, pocketPos, targetBall->id, -1))
            {
                // --- Evaluate using the correct function based on game mode ---
                AIShotInfo bankCand;
                bool is9BallBank = false;
                if (currentGameType == GameType::EIGHT_BALL_MODE) {
                    bankCand = EvaluateShot_8Ball(targetBall, p, true);
                }
                else if (currentGameType == GameType::NINE_BALL) {
                    bankCand = EvaluateShot_NineBall(targetBall, p, true, false); // isBank=true, isCombo=false
                    is9BallBank = true;
                }
                else { // STRAIGHT_POOL
                    bankCand = EvaluateShot_StraightPool(targetBall, p, true);
                }

                if (bankCand.possible) {
                    possibleShots.push_back(bankCand);
                }
            }
        }
    }
}

//[00000]
/*// --- REPLACED: Foolproof Bank Shot Calculation ---
void AIFindBankShots(Ball* targetBall, std::vector<AIShotInfo>& possibleShots) {
    if (!targetBall) return;
    Ball* cue = GetCueBall();
    if (!cue) return;

    // Define the 4 primary rail lines for one-cushion banks.
    const D2D1_POINT_2F railLines[4][2] = {
        { {TABLE_LEFT, TABLE_TOP + INNER_RIM_THICKNESS}, {TABLE_RIGHT, TABLE_TOP + INNER_RIM_THICKNESS} },         // Top rail line
        { {TABLE_LEFT, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_RIGHT, TABLE_BOTTOM - INNER_RIM_THICKNESS} },     // Bottom rail line
        { {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP}, {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM} },        // Left rail line
        { {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP}, {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM} },      // Right rail line
    };

    D2D1_POINT_2F targetPos = { targetBall->x, targetBall->y };

    for (int p = 0; p < 6; ++p) {
        D2D1_POINT_2F pocketPos = pocketPositions[p];

        for (int r = 0; r < 4; ++r) {
            D2D1_POINT_2F p1 = railLines[r][0];

            D2D1_POINT_2F virtualPocket;
            if (r < 2) { // Horizontal rails
                virtualPocket = { pocketPos.x, 2 * p1.y - pocketPos.y };
            }
            else {     // Vertical rails
                virtualPocket = { 2 * p1.x - pocketPos.x, pocketPos.y };
            }

            D2D1_POINT_2F bouncePoint;
            if (!LineSegmentIntersection(targetPos, virtualPocket, railLines[r][0], railLines[r][1], bouncePoint)) {
                continue;
            }

            // Validate that the bounce point is on a hittable part of the cushion (not in a pocket jaw)
            bool bouncePointIsSafe = true;
            for (int pocketIdx = 0; pocketIdx < 6; ++pocketIdx) {
                if (GetDistanceSq(bouncePoint.x, bouncePoint.y, pocketPositions[pocketIdx].x, pocketPositions[pocketIdx].y) < pow(GetPocketVisualRadius(pocketIdx) * 1.5f, 2)) {
                    bouncePointIsSafe = false;
                    break;
                }
            }
            if (!bouncePointIsSafe) continue;

            // Check if all paths for the bank shot are clear
            if (!IsPathClear(targetPos, bouncePoint, targetBall->id, -1) ||
                !IsPathClear(bouncePoint, pocketPos, targetBall->id, -1)) {
                continue;
            }

            D2D1_POINT_2F ghostBallForBank = CalculateGhostBallPos(targetBall, bouncePoint);

            if (IsPathClear({ cue->x, cue->y }, ghostBallForBank, cue->id, targetBall->id)) {
                // If all checks pass, evaluate this bank shot
                AIShotInfo bankCand = EvaluateShot(targetBall, p, true);
                if (bankCand.possible) {
                    possibleShots.push_back(bankCand);
                }
            }
        }
    }
}*/

// --- NEW: AI Bank Shot Calculation ---
/*void AIFindBankShots(Ball* targetBall, std::vector<AIShotInfo>& possibleShots) {
    if (!targetBall) return;
    Ball* cue = GetCueBall();
    if (!cue) return;

    // Define the 6 primary rail cushions for banking (simplified for clarity)
    const D2D1_POINT_2F railCushions[6][2] = {
        { {TABLE_LEFT + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS}, {TABLE_RIGHT - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS} },         // Top
        { {TABLE_LEFT + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_RIGHT - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS} },     // Bottom
        { {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE}, {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE} },        // Left
        { {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE}, {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE} },      // Right
    };
    // Normals for each cushion (pointing inwards)
    const D2D1_POINT_2F railNormals[4] = { {0, 1}, {0, -1}, {1, 0}, {-1, 0} };

    D2D1_POINT_2F targetPos = { targetBall->x, targetBall->y };

    for (int p = 0; p < 6; ++p) { // Iterate through all pockets
        D2D1_POINT_2F pocketPos = pocketPositions[p];

        for (int r = 0; r < 4; ++r) { // Iterate through the 4 main rail lines
            D2D1_POINT_2F railLinePoint = railCushions[r][0]; // A point on the rail line

            // Reflect the pocket's position across the rail line to create a "virtual pocket"
            D2D1_POINT_2F virtualPocket;
            if (r < 2) { // Horizontal rails (Top/Bottom)
                virtualPocket = { pocketPos.x, 2 * railLinePoint.y - pocketPos.y };
            }
            else {     // Vertical rails (Left/Right)
                virtualPocket = { 2 * railLinePoint.x - pocketPos.x, pocketPos.y };
            }

            // Find intersection of line (target -> virtual pocket) with the actual rail segment
            D2D1_POINT_2F bouncePoint;
            if (!LineSegmentIntersection(targetPos, virtualPocket, railCushions[r][0], railCushions[r][1], bouncePoint)) {
                continue; // The calculated bounce point isn't on the physical cushion segment.
            }

            // Check if the bank shot paths are clear
            if (!IsPathClear(targetPos, bouncePoint, targetBall->id, -1) ||
                !IsPathClear(bouncePoint, pocketPos, targetBall->id, -1)) {
                continue;
            }

            // Now, determine the ghost ball position to send the target ball to the bounce point
            D2D1_POINT_2F ghostBallForBank = CalculateGhostBallPos(targetBall, bouncePoint);

            // Finally, check if the path from the cue ball to this ghost ball is clear
            if (IsPathClear({ cue->x, cue->y }, ghostBallForBank, cue->id, targetBall->id)) {
                AIShotInfo bankShot;
                bankShot.possible = true;
                bankShot.targetBall = targetBall;
                bankShot.pocketIndex = p;
                bankShot.isBankShot = true;
                bankShot.involves8Ball = (targetBall->id == 8);

                // Calculate angle and power for this bank shot
                float dx = ghostBallForBank.x - cue->x;
                float dy = ghostBallForBank.y - cue->y;
                bankShot.angle = atan2f(dy, dx);

                float cueToGhostDist = GetDistance(cue->x, cue->y, ghostBallForBank.x, ghostBallForBank.y);
                float targetToBounceDist = GetDistance(targetPos.x, targetPos.y, bouncePoint.x, bouncePoint.y);
                float bounceToPocketDist = GetDistance(bouncePoint.x, bouncePoint.y, pocketPos.x, pocketPos.y);

                bankShot.power = CalculateShotPower(cueToGhostDist, targetToBounceDist + bounceToPocketDist);
                bankShot.power = std::clamp(bankShot.power * 1.25f, 0.2f, MAX_SHOT_POWER); // Banks need extra power

                // Score the shot (banks are harder, so they get a lower base score)
                bankShot.score = 750.0f - (cueToGhostDist + (targetToBounceDist + bounceToPocketDist) * 1.8f);

                possibleShots.push_back(bankShot);
            }
        }
    }
}*/

// Helper function to evaluate the quality of the table layout from the cue ball's perspective.
// A higher score means a better "leave" with more options for the next shot.
float EvaluateTableState(int player, Ball* cueBall) {
    if (!cueBall) return 0.0f;
    float score = 0.0f;
    const BallType targetType = (player == 1) ? player1Info.assignedType : player2Info.assignedType;

    for (Ball& b : balls) {
        // Only consider own balls that are on the table
        if (b.isPocketed || b.id == 0 || b.type != targetType) continue;

        // Check for clear shots to any pocket
        for (int p = 0; p < 6; ++p) {
            D2D1_POINT_2F ghostPos = CalculateGhostBallPos(&b, pocketPositions[p]);
            //D2D1_POINT_2F ghostPos = CalculateGhostBallPos(&b, p);
            if (IsPathClear(D2D1::Point2F(cueBall->x, cueBall->y), ghostPos, cueBall->id, b.id)) {
                // Bonus for having any open, pottable shot
                score += 50.0f;
            }
        }
    }
    return score;
}

// --- REPLACED: Sophisticated Physics-Based Power Calculation ---
// Calculates shot power using a model that better accounts for energy loss over distance.
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist, bool isBank) {
    // A more physical model: Required energy (and thus power) is not linear with distance due to friction.
    // A square root model provides a more realistic power curve that ramps up for long shots.

    // Part 1: Power needed for the target ball to travel to the pocket after being hit.
    // This is the baseline energy that must be transferred.
    float target_leg_power = 2.8f * sqrtf(targetToPocketDist); // FIX: Use sqrtf for float operations

    // Part 2: Power needed for the cue ball to reach the target with enough energy left to transfer.
    float cue_leg_power = 2.2f * sqrtf(cueToGhostDist); // FIX: Use sqrtf for float operations

    // The total power is the sum of the power needed for both legs of the journey.
    float total_power_units = cue_leg_power + target_leg_power;

    // Apply a significant multiplier for bank shots, which lose a lot of energy.
    if (isBank) {
        total_power_units *= 1.7f;
    }

    // Now, we scale these calculated "power units" to the game's actual [0, MAX_SHOT_POWER] range.
    // First, find a reasonable maximum for our power units (e.g., a long shot across the diagonal).
    float max_possible_units = 2.5f * sqrtf(TABLE_WIDTH) + 2.0f * sqrtf(TABLE_HEIGHT); // FIX: Use sqrtf for float operations

    // Calculate the power as a ratio of the required units vs the max possible.
    float power_ratio = std::clamp(total_power_units / (max_possible_units * 0.9f), 0.0f, 1.0f);

    // Map this ratio to the game's power range, starting from a higher minimum base power.
    const float MIN_POWER_OUTPUT = 4.0f;
    float finalPower = MIN_POWER_OUTPUT + power_ratio * (MAX_SHOT_POWER - MIN_POWER_OUTPUT);

    return std::clamp(finalPower, 3.0f, MAX_SHOT_POWER);
}

/*// --- REPLACED: Sophisticated Physics-Based Power Calculation ---
// Calculates shot power using a model that better accounts for energy loss over distance.
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist, bool isBank) {
    // A more physical model: Required energy (and thus power) is not linear with distance due to friction.
    // A square root model provides a more realistic power curve that ramps up for long shots.

    // Part 1: Power needed for the target ball to travel to the pocket after being hit.
    // This is the baseline energy that must be transferred.
    float target_leg_power = 2.5f * sqrt(targetToPocketDist);

    // Part 2: Power needed for the cue ball to reach the target with enough energy left to transfer.
    float cue_leg_power = 2.0f * sqrt(cueToGhostDist);

    // The total power is the sum of the power needed for both legs of the journey.
    float total_power_units = cue_leg_power + target_leg_power;

    // Apply a significant multiplier for bank shots, which lose a lot of energy.
    if (isBank) {
        total_power_units *= 1.5f;
    }

    // Now, we scale these calculated "power units" to the game's actual [0, MAX_SHOT_POWER] range.
    // First, find a reasonable maximum for our power units (e.g., a long shot across the diagonal).
    float max_possible_units = 2.5f * sqrt(TABLE_WIDTH) + 2.0f * sqrt(TABLE_HEIGHT); // Theoretical max for scaling

    // Calculate the power as a ratio of the required units vs the max possible.
    float power_ratio = std::clamp(total_power_units / (max_possible_units * 0.9f), 0.0f, 1.0f);

    // Map this ratio to the game's power range, starting from a higher minimum base power.
    const float MIN_POWER_OUTPUT = 3.5f;
    float finalPower = MIN_POWER_OUTPUT + power_ratio * (MAX_SHOT_POWER - MIN_POWER_OUTPUT);

    return std::clamp(finalPower, 2.5f, MAX_SHOT_POWER);
}*/

//[00000]
/*// --- REPLACED: Sophisticated Power Calculation ---
// Calculates a realistic shot power based on distance and shot type, with enhanced strength for long shots.
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist, bool isBank) {
    // 1. Calculate the total distance the energy needs to cover.
    float totalDist = cueToGhostDist + targetToPocketDist;

    // 2. Establish a higher base power to overcome initial friction and ensure short taps are firm.
    const float min_power = 12.5f; // Increased from 2.0f to give all shots more "punch".
    const float max_power_scale = 15.0f; // The maximum power to add based on distance.

    // 3. Use a non-linear (quadratic) scaling for distance.
    // This makes the power increase much more significantly for longer shots compared to shorter ones.
    float distance_ratio = totalDist / (TABLE_WIDTH * 1.5f);
    float power = min_power + (distance_ratio * distance_ratio) * max_power_scale;

    // 4. Apply a more significant multiplier for bank shots.
    // Bank shots lose considerable energy at the cushion and require significantly more initial force.
    if (isBank) {
        power *= 1.45f; // Increased from 1.35f
    }

    // 5. Add a slight random variation to make the AI feel less robotic.
    power *= (0.98f + (rand() % 5) / 100.0f); // Tighter +/- 2% variation for consistency

    // 6. Clamp the final value to the game's absolute maximum.
    return std::clamp(power, 2.0f, MAX_SHOT_POWER);
}*/

/*// --- REPLACED: Sophisticated Power Calculation ---
// Calculates a realistic shot power based on distance and shot type.
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist, bool isBank) {
    // 1. Calculate the total distance the energy needs to cover.
    float totalDist = cueToGhostDist + targetToPocketDist;

    // 2. Base power on a linear scale related to table width. This provides a consistent base.
    // A shot across the table needs significant power. A short tap needs very little.
    const float min_power_base = 2.0f;
    const float max_power_base = 10.0f;
    float power = min_power_base + (totalDist / (TABLE_WIDTH * 1.5f)) * (max_power_base - min_power_base);

    // 3. Apply multipliers for specific shot types that require more force.
    if (isBank) {
        power *= 1.35f; // Bank shots lose energy at the cushion and need significantly more power.
    }

    // 4. Add a slight random variation to make the AI feel less robotic.
    power *= (0.97f + (rand() % 7) / 100.0f); // +/- 3% variation

    // 5. Clamp the final value to the game's limits.
    return std::clamp(power, 1.5f, MAX_SHOT_POWER);
}*/

/*// Enhanced power calculation for more realistic and impactful shots.
float CalculateShotPower(float cueToGhostDist, float targetToPocketDist)
{
    constexpr float TABLE_DIAG = 900.0f;

    // Base power on a combination of cue travel and object ball travel.
    float powerRatio = std::clamp(cueToGhostDist / (TABLE_WIDTH * 0.7f), 0.0f, 1.0f);
    // Give more weight to the object ball's travel distance.
    powerRatio += std::clamp(targetToPocketDist / TABLE_DIAG, 0.0f, 1.0f) * 0.8f;
    powerRatio = std::clamp(powerRatio, 0.0f, 1.0f);

    // Use a more aggressive cubic power curve. This keeps power low for taps but ramps up very fast.
    powerRatio = powerRatio * powerRatio * powerRatio;

    // Heavily bias towards high power. The AI will start shots at 50% power and go up.
    const float MIN_POWER = MAX_SHOT_POWER * 0.50f;
    float power = MIN_POWER + powerRatio * (MAX_SHOT_POWER - MIN_POWER);

    // For very long shots, just use full power to ensure the ball gets there and for break-out potential.
    if (cueToGhostDist + targetToPocketDist > TABLE_DIAG * 0.85f) {
        power = MAX_SHOT_POWER;
    }

    return std::clamp(power, 0.2f, MAX_SHOT_POWER);
}*/

//[00000]
// --- REPLACED: Advanced Shot Evaluation with Cut Angle Penalty ---
AIShotInfo EvaluateShot_8Ball(Ball* targetBall, int pocketIndex, bool isBank) {
    AIShotInfo shotInfo;
    shotInfo.possible = false;
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;
    shotInfo.isBankShot = isBank;
    shotInfo.involves8Ball = (targetBall && targetBall->id == 8);

    Ball* cue = GetCueBall();
    if (!cue || !targetBall) return shotInfo;

    // 1. Core Industry Algorithm: Pure Ghost Ball Calculation
    // We aim for the center of the pocket for maximum statistical reliability.
    D2D1_POINT_2F pocketPos = pocketPositions[pocketIndex];
    D2D1_POINT_2F targetPos = { targetBall->x, targetBall->y };

    // Ghost Ball center is exactly 2.0 radii away from target center on the aim line
    D2D1_POINT_2F ghostBallPos = CalculateGhostBallPos(targetBall, pocketPos);
    shotInfo.ghostBallPos = ghostBallPos;

    // 2. Physics-Informed Path Checks
    // Check line-of-sight from Cue to Ghost position
    if (!IsPathClear({ cue->x, cue->y }, ghostBallPos, cue->id, targetBall->id)) return shotInfo;

    // Check line-of-sight from Target Ball to Pocket (skipped for Bank shots)
    if (!isBank && !IsPathClear(targetPos, pocketPos, targetBall->id, -1)) return shotInfo;

    // 3. Cut-Angle Feasibility Analysis (Professional EV Scoring)
    // We calculate the vectors of the two legs of the shot to determine difficulty.
    D2D1_POINT_2F vCue = { ghostBallPos.x - cue->x, ghostBallPos.y - cue->y };
    D2D1_POINT_2F vObj = { pocketPos.x - targetBall->x, pocketPos.y - targetBall->y };

    float magCue = sqrtf(vCue.x * vCue.x + vCue.y * vCue.y);
    float magObj = sqrtf(vObj.x * vObj.x + vObj.y * vObj.y);

    if (magCue < 1e-4f || magObj < 1e-4f) return shotInfo;

    // Dot product to find the cosine of the cut angle
    float dot = (vCue.x * vObj.x + vCue.y * vObj.y) / (magCue * magObj);
    float cutAngle = acosf(std::clamp(dot, -1.0f, 1.0f));

    // Calculate Cut Score: 1.0 is straight-in, 0.0 is an impossible > 108 degree cut
    float cutScore = std::max(0.0f, 1.0f - cutAngle / (PI * 0.6f));

    // Pruning: Ignore shots that are physically improbable (extremely thin cuts)
    if (cutScore < 0.15f) return shotInfo;

    // 4. Final Heuristic Scoring
    // Higher score = more desirable shot. Factors in cut feasibility and distance.
    float distancePenalty = (magCue * 0.015f) + (magObj * 0.01f);
    shotInfo.score = (cutScore * 400.0f) - distancePenalty;

    // Apply adjustments for specific game scenarios
    if (isBank) shotInfo.score -= 500.0f; // Banks are higher risk
    if (shotInfo.involves8Ball) shotInfo.score += 150.0f; // Priority for the win

    // 5. Shot Parameter Generation
    shotInfo.angle = atan2f(ghostBallPos.y - cue->y, ghostBallPos.x - cue->x);
    shotInfo.power = CalculateShotPower(magCue, magObj, isBank);
    shotInfo.spinX = 0.0f;
    shotInfo.spinY = 0.0f;

    shotInfo.possible = true;
    return shotInfo;
}


// --- NEW: Straight Pool Shot Evaluation ---
// Simplified scoring: Focus on making *any* ball and leaving cue well.
AIShotInfo EvaluateShot_StraightPool(Ball* targetBall, int pocketIndex, bool isBank) {
    AIShotInfo shotInfo;
    shotInfo.possible = false;
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;
    shotInfo.isBankShot = isBank;

    Ball* cue = GetCueBall();
    if (!cue || !targetBall) return shotInfo;

    // 1. Core Industry Algorithm: Pure Ghost Ball Calculation
    D2D1_POINT_2F pocketPos = pocketPositions[pocketIndex];
    D2D1_POINT_2F targetPos = { targetBall->x, targetBall->y };
    D2D1_POINT_2F ghostBallPos = CalculateGhostBallPos(targetBall, pocketPos);
    shotInfo.ghostBallPos = ghostBallPos;

    // 2. Physics-Informed Path Checks
    if (!IsPathClear({ cue->x, cue->y }, ghostBallPos, cue->id, targetBall->id)) return shotInfo;
    if (!isBank && !IsPathClear(targetPos, pocketPos, targetBall->id, -1)) return shotInfo;

    // 3. Cut-Angle Feasibility Analysis (Professional EV Scoring)
    D2D1_POINT_2F vCue = { ghostBallPos.x - cue->x, ghostBallPos.y - cue->y };
    D2D1_POINT_2F vObj = { pocketPos.x - targetBall->x, pocketPos.y - targetBall->y };

    float magCue = sqrtf(vCue.x * vCue.x + vCue.y * vCue.y);
    float magObj = sqrtf(vObj.x * vObj.x + vObj.y * vObj.y);

    if (magCue < 1e-4f || magObj < 1e-4f) return shotInfo;

    float dot = (vCue.x * vObj.x + vCue.y * vObj.y) / (magCue * magObj);
    float cutAngle = acosf(std::clamp(dot, -1.0f, 1.0f));
    float cutScore = std::max(0.0f, 1.0f - cutAngle / (PI * 0.6f));

    // Pruning: Ignore shots that are physically improbable (extremely thin cuts)
    if (cutScore < 0.15f) return shotInfo;

    // 4. Final Heuristic Scoring
    float distPenalty = (magCue * 0.015f) + (magObj * 0.01f);
    shotInfo.score = (cutScore * 400.0f) - distPenalty;

    if (isBank) shotInfo.score -= 500.0f;

    // 5. Shot Parameter Generation (Position Play is still determined by the caller)
    shotInfo.angle = atan2f(ghostBallPos.y - cue->y, ghostBallPos.x - cue->x);
    shotInfo.power = CalculateShotPower(magCue, magObj, isBank);
    shotInfo.spinX = 0.0f;
    shotInfo.spinY = 0.0f;

    shotInfo.possible = true;
    return shotInfo;
}

// --- NEW: Straight Pool Table State Evaluation ---
// Scores based on how many easy shots are available from the cue position.
float EvaluateTableState_StraightPool(Ball* cueBall) {
    if (!cueBall) return 0.0f;
    float score = 0.0f;
    int potentialShots = 0;

    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue; // Skip cue and pocketed

        // Check for clear shots to any pocket for this ball
        for (int p = 0; p < 6; ++p) {
            D2D1_POINT_2F ghostPos = CalculateGhostBallPos(&b, pocketPositions[p]);
            if (IsPathClear({ cueBall->x, cueBall->y }, ghostPos, cueBall->id, b.id)) {
                potentialShots++;
                // Bonus for easier shots (closer)
                float dist = GetDistance(cueBall->x, cueBall->y, ghostPos.x, ghostPos.y) + GetDistance(b.x, b.y, pocketPositions[p].x, pocketPositions[p].y);
                score += std::max(0.0f, 100.0f - dist * 0.1f); // More score for closer shots
            }
        }
    }
    // [+] Advanced Pro Logic: Cluster Analysis
// Evaluate how many balls are currently 'touching' or in tight clusters.
// Winning at Straight Pool requires breaking these clusters open.
    for (size_t i = 0; i < balls.size(); ++i) {
        if (balls[i].id == 0 || balls[i].isPocketed) continue;
        for (size_t j = i + 1; j < balls.size(); ++j) {
            if (balls[j].id == 0 || balls[j].isPocketed) continue;
            float d = GetDistance(balls[i].x, balls[i].y, balls[j].x, balls[j].y);
            if (d < BALL_RADIUS * 3.0f) score -= 30.0f; // Penalty for clusters
        }
    }
    // The AI will now prefer shots that move the cue ball toward these clusters to break them.
    return score;
}

/*// Evaluates a potential shot, calculating its difficulty and the quality of the resulting cue ball position.
AIShotInfo EvaluateShot(Ball* targetBall, int pocketIndex, bool isBank) {
    AIShotInfo shotInfo;
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;
    shotInfo.isBankShot = isBank;
    shotInfo.involves8Ball = (targetBall && targetBall->id == 8);

    Ball* cue = GetCueBall();
    if (!cue || !targetBall) return shotInfo;

    // --- ROBUST AIMING: Aim deeper into the pocket along the shot line ---
    D2D1_POINT_2F pocketPos = pocketPositions[pocketIndex];
    D2D1_POINT_2F targetPos = D2D1::Point2F(targetBall->x, targetBall->y);

    // Vector from target ball TO the pocket center
    float shotLineX = pocketPos.x - targetPos.x;
    float shotLineY = pocketPos.y - targetPos.y;
    float len = sqrtf(shotLineX * shotLineX + shotLineY * shotLineY);
    if (len > 1e-6f) {
        shotLineX /= len;
        shotLineY /= len;
    }

    // Aim slightly inside the pocket mouth. This is more reliable than a fixed offset.
    float sweetSpotInset = BALL_RADIUS * 0.75f;
    D2D1_POINT_2F aimTargetPos = {
        pocketPos.x - shotLineX * sweetSpotInset,
        pocketPos.y - shotLineY * sweetSpotInset
    };

    // The ghost ball position required to send the target ball to our smart aim position.
    D2D1_POINT_2F ghostBallPos = CalculateGhostBallPos(targetBall, aimTargetPos);

    if (!IsPathClear(D2D1::Point2F(cue->x, cue->y), ghostBallPos, cue->id, targetBall->id)) {
        return shotInfo; // Path is blocked
    }
    if (!isBank && !IsPathClear(targetPos, aimTargetPos, targetBall->id, -1)) {
        return shotInfo; // Direct shot path to pocket is blocked
    }

    float dx = ghostBallPos.x - cue->x;
    float dy = ghostBallPos.y - cue->y;
    shotInfo.angle = atan2f(dy, dx);

    float cueToGhostDist = GetDistance(cue->x, cue->y, ghostBallPos.x, ghostBallPos.y);
    float targetToPocketDist = GetDistance(targetBall->x, targetBall->y, aimTargetPos.x, aimTargetPos.y);

    shotInfo.power = CalculateShotPower(cueToGhostDist, targetToPocketDist);
    shotInfo.score = 1000.0f - (cueToGhostDist + targetToPocketDist * 1.5f);
    if (isBank) shotInfo.score -= 400.0f; // Penalize difficult bank shots

    // --- Positional Play & Spin Evaluation ---
    float bestLeaveScore = -9999.0f;
    // Try different spins: -1 (draw), 0 (stun), 1 (follow)
    for (int spin_type = -1; spin_type <= 1; ++spin_type) {
        float spinY = spin_type * 0.7f;

        float powerMultiplier = 1.0f;
        if (spin_type == -1) powerMultiplier *= 1.2f;
        else if (spin_type == 1) powerMultiplier *= 1.05f;
        if (isBank) powerMultiplier *= 1.25f; // Banks need more power

        float adjustedPower = std::clamp(shotInfo.power * powerMultiplier, 0.2f, MAX_SHOT_POWER);

        // Predict cue ball end position (simplified model)
        D2D1_POINT_2F cueEndPos;
        float cueTravelDist = targetToPocketDist * 0.5f + adjustedPower * 5.0f;

        if (spin_type == 1) { // Follow
            cueEndPos.x = targetPos.x + cosf(shotInfo.angle) * cueTravelDist * 0.6f;
            cueEndPos.y = targetPos.y + sinf(shotInfo.angle) * cueTravelDist * 0.6f;
        }
        else if (spin_type == -1) { // Draw
            cueEndPos.x = ghostBallPos.x - cosf(shotInfo.angle) * cueTravelDist * 0.4f;
            cueEndPos.y = ghostBallPos.y - sinf(shotInfo.angle) * cueTravelDist * 0.4f;
        }
        else { // Stun
            float perpAngle = shotInfo.angle + PI / 2.0f;
            cueEndPos.x = ghostBallPos.x + cosf(perpAngle) * cueTravelDist * 0.3f;
            cueEndPos.y = ghostBallPos.y + sinf(perpAngle) * cueTravelDist * 0.3f;
        }

        Ball tempCue = *cue;
        tempCue.x = cueEndPos.x;
        tempCue.y = cueEndPos.y;
        float leaveScore = EvaluateTableState(2, &tempCue);

        // Heavily penalize predicted scratches
        for (int p = 0; p < 6; ++p) {
            if (IsBallInPocketBlackArea(tempCue, p)) {
                leaveScore -= 5000.0f;
                break;
            }
        }

        if (leaveScore > bestLeaveScore) {
            bestLeaveScore = leaveScore;
            shotInfo.spinY = spinY;
            shotInfo.spinX = 0;
            shotInfo.predictedCueEndPos = cueEndPos;
            shotInfo.power = adjustedPower; // Use the power for this best-leave scenario
        }
    }

    shotInfo.score += bestLeaveScore; // Add positional score
    shotInfo.possible = true;
    return shotInfo;
}*/

/*// Evaluates a potential shot, calculating its difficulty and the quality of the resulting cue ball position.
AIShotInfo EvaluateShot(Ball* targetBall, int pocketIndex, bool isBank) {
    AIShotInfo shotInfo;
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;
    shotInfo.isBankShot = isBank;
    shotInfo.involves8Ball = (targetBall && targetBall->id == 8);

    Ball* cue = GetCueBall();
    if (!cue || !targetBall) return shotInfo;

    // --- FOOLPROOF FIX 2: Aim for a "sweet spot" inside the pocket, not the dead center ---
    D2D1_POINT_2F pocketCenter = pocketPositions[pocketIndex];
    D2D1_POINT_2F tableCenter = { TABLE_LEFT + TABLE_WIDTH / 2.0f, TABLE_TOP + TABLE_HEIGHT / 2.0f };

    // Calculate a vector from the table center to the pocket center
    float dirX = pocketCenter.x - tableCenter.x;
    float dirY = pocketCenter.y - tableCenter.y;
    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len > 1e-6f) {
        dirX /= len;
        dirY /= len;
    }

    // The new target is slightly *inside* the pocket opening, away from the chamfer tips.
    // This makes the AI's shots much more reliable.
    float sweetSpotOffset = BALL_RADIUS * 1.5f; // Aim slightly inside the pocket
    D2D1_POINT_2F aimTargetPos = {
        pocketCenter.x - dirX * sweetSpotOffset,
        pocketCenter.y - dirY * sweetSpotOffset
    };
    // --- END OF TARGETING FIX ---

    //D2D1_POINT_2F pocketPos = pocketPositions[pocketIndex];
    D2D1_POINT_2F targetPos = D2D1::Point2F(targetBall->x, targetBall->y);
    //D2D1_POINT_2F ghostBallPos = CalculateGhostBallPos(targetBall, pocketIndex);
        // Calculate ghost ball position based on the new, smarter aim target
    D2D1_POINT_2F ghostBallPos = CalculateGhostBallPos(targetBall, aimTargetPos);

    // The path from cue to the ghost ball position must be clear.
    if (!IsPathClear(D2D1::Point2F(cue->x, cue->y), ghostBallPos, cue->id, targetBall->id)) {
        return shotInfo; // Path is blocked, invalid shot.
    }

    // For direct shots, the path from the target ball to the pocket must also be clear.
    if (!isBank && !IsPathClear(targetPos, aimTargetPos, targetBall->id, -1)) {
    //if (!isBank && !IsPathClear(targetPos, pocketPos, targetBall->id, -1)) {
        return shotInfo;
    }

    float dx = ghostBallPos.x - cue->x;
    float dy = ghostBallPos.y - cue->y;
    shotInfo.angle = atan2f(dy, dx);

    float cueToGhostDist = GetDistance(cue->x, cue->y, ghostBallPos.x, ghostBallPos.y);
    float targetToPocketDist = GetDistance(targetBall->x, targetBall->y, aimTargetPos.x, aimTargetPos.y);
    //float targetToPocketDist = GetDistance(targetBall->x, targetBall->y, pocketPos.x, pocketPos.y);

    shotInfo.power = CalculateShotPower(cueToGhostDist, targetToPocketDist); // Set a base power
    shotInfo.score = 1000.0f - (cueToGhostDist + targetToPocketDist * 1.5f); // Base score on shot difficulty.
    if (isBank) shotInfo.score -= 250.0f; // Bank shots are harder, so they have a score penalty.


    // --- Positional Play & English (Spin) Evaluation ---
    float bestLeaveScore = -1.0f;
    // Try different spins: -1 (draw/backspin), 0 (stun), 1 (follow/topspin)
    for (int spin_type = -1; spin_type <= 1; ++spin_type) {
        float spinY = spin_type * 0.7f; // Apply vertical spin.

        // Adjust power based on spin type. Draw shots need more power.
        float powerMultiplier = 1.0f;
        if (spin_type == -1) powerMultiplier = 1.2f;  // 20% more power for draw
        else if (spin_type == 1) powerMultiplier = 1.05f; // 5% more power for follow

        float adjustedPower = std::clamp(shotInfo.power * powerMultiplier, 0.2f, MAX_SHOT_POWER);

        // Predict where the cue ball will end up after the shot.
        D2D1_POINT_2F cueEndPos;
        float cueTravelDist = targetToPocketDist * 0.5f + adjustedPower * 5.0f;

        if (spin_type == 1) { // Follow (topspin)
            cueEndPos.x = targetPos.x + cosf(shotInfo.angle) * cueTravelDist * 0.6f;
            cueEndPos.y = targetPos.y + sinf(shotInfo.angle) * cueTravelDist * 0.6f;
        }
        else if (spin_type == -1) { // Draw (backspin)
            cueEndPos.x = ghostBallPos.x - cosf(shotInfo.angle) * cueTravelDist * 0.4f;
            cueEndPos.y = ghostBallPos.y - sinf(shotInfo.angle) * cueTravelDist * 0.4f;
        }
        else { // Stun (no vertical spin) - cue ball deflects
            float perpAngle = shotInfo.angle + PI / 2.0f;
            cueEndPos.x = ghostBallPos.x + cosf(perpAngle) * cueTravelDist * 0.3f;
            cueEndPos.y = ghostBallPos.y + sinf(perpAngle) * cueTravelDist * 0.3f;
        }

        // Create a temporary cue ball at the predicted position to evaluate the leave.
        Ball tempCue = *cue;
        tempCue.x = cueEndPos.x;
        tempCue.y = cueEndPos.y;
        float leaveScore = EvaluateTableState(2, &tempCue);

        // Penalize scratches heavily.
        for (int p = 0; p < 6; ++p) {
            float physicalPocketRadius = ((p == 1 || p == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS) * 1.05f;
            if (GetDistanceSq(cueEndPos.x, cueEndPos.y, pocketPositions[p].x, pocketPositions[p].y) < physicalPocketRadius * physicalPocketRadius * 1.5f) {
                leaveScore -= 5000.0f; // Massive penalty for a predicted scratch.
                break;
            }
        }

        // If this spin results in a better position, store it.
        if (leaveScore > bestLeaveScore) {
            bestLeaveScore = leaveScore;
            shotInfo.spinY = spinY;
            shotInfo.spinX = 0; // Focusing on top/back spin for positional play.
            shotInfo.predictedCueEndPos = cueEndPos;
            shotInfo.power = adjustedPower; // IMPORTANT: Update the shot's power to the adjusted value for this spin
        }
    }

    shotInfo.score += bestLeaveScore * 0.5f; // Add positional score to the shot's total score.
    shotInfo.possible = true;
    return shotInfo;
}*/

/*// --- REPLACED: Top-Level AI Decision Making with Foolproof 8-Ball Logic ---
AIShotInfo AIFindBestShot() {
    AIShotInfo bestShot;
    bestShot.score = -99999.0f;
    bestShot.possible = false;

    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    const BallType wantType = player2Info.assignedType;
    std::vector<AIShotInfo> possibleShots;

    // --- NEW: Priority-Based Target Selection ---
    // 1. First, identify all legal object balls and a potential 8-ball target separately.
    std::vector<Ball*> legalObjectBalls;
    Ball* eightBallTarget = nullptr;
    const bool on8 = IsPlayerOnEightBall(2);

    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue;

        if (b.id == 8) {
            if (on8) {
                eightBallTarget = &b; // The 8-ball is a potential target if the AI has cleared its group.
            }
        }
        else if (wantType == BallType::NONE || b.type == wantType) {
            legalObjectBalls.push_back(&b); // This is a standard object ball for the AI.
        }
    }

    // 2. Create the final list of targets to evaluate based on strict rules.
    std::vector<Ball*> targetsToEvaluate;
    if (!legalObjectBalls.empty()) {
        // **RULE: If there are ANY of the AI's own balls left, it MUST shoot at them.**
        targetsToEvaluate = legalObjectBalls;
    }
    else if (eightBallTarget) {
        // **RULE: Only if NO other balls are left, the AI may shoot at the 8-ball.**
        targetsToEvaluate.push_back(eightBallTarget);
    }
    // --- End Priority-Based Target Selection ---

    // 3. Evaluate direct and bank shots ONLY for the determined legal targets.
    for (Ball* targetBall : targetsToEvaluate) {
        // Evaluate direct shots to all pockets
        for (int p = 0; p < 6; ++p) {
            AIShotInfo cand = EvaluateShot(targetBall, p, false);
            if (cand.possible) {
                possibleShots.push_back(cand);
            }
        }
        // Evaluate bank shots if difficulty is not Easy
        if (aiDifficulty > EASY) {
            AIFindBankShots(targetBall, possibleShots);
        }
    }

    // 4. Select the highest-scoring shot from all valid possibilities.
    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (const auto& shot : possibleShots) {
            if (shot.score > bestShot.score) {
                bestShot = shot;
            }
        }
    }
    else {
        // 5. If no shots are possible, play the best available safety shot.
        bestShot.possible = true;
        bestShot.score = -99999.0f;
        bestShot.spinX = bestShot.spinY = 0.0f;
        bestShot.pocketIndex = -1;

        std::vector<Ball*> legalSafetyTargets = targetsToEvaluate.empty() ? legalObjectBalls : targetsToEvaluate;

        if (!legalSafetyTargets.empty()) {
            // Safety: Gently hit the easiest-to-reach legal ball towards the center of a rail.
            Ball* safetyTarget = legalSafetyTargets[0];
            float minDistSq = GetDistanceSq(cue->x, cue->y, safetyTarget->x, safetyTarget->y);
            for (size_t i = 1; i < legalSafetyTargets.size(); ++i) {
                float distSq = GetDistanceSq(cue->x, cue->y, legalSafetyTargets[i]->x, legalSafetyTargets[i]->y);
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    safetyTarget = legalSafetyTargets[i];
                }
            }
            bestShot.targetBall = safetyTarget;
            D2D1_POINT_2F safetyAimPoint = { TABLE_LEFT + TABLE_WIDTH / 2.0f, TABLE_TOP + INNER_RIM_THICKNESS };
            D2D1_POINT_2F ghostPos = CalculateGhostBallPos(safetyTarget, safetyAimPoint);
            bestShot.angle = atan2f(ghostPos.y - cue->y, ghostPos.x - cue->x);
            bestShot.power = MAX_SHOT_POWER * 0.3f;
        }
        else {
            // Absolute fallback (should never happen): a random soft hit.
            bestShot.angle = (float)(rand() % 314) / 100.0f;
            bestShot.power = MAX_SHOT_POWER * 0.2f;
        }
    }
    return bestShot;
}*/

// --- REPLACED: Top-Level AI Decision Making with Foolproof 8-Ball & Safety Logic (v2) ---
// --- RENAME existing AIFindBestShot to AIFindBestShot_8Ball ---
AIShotInfo AIFindBestShot_8Ball() {
    AIShotInfo bestShot;
    bestShot.score = -99999.0f;
    bestShot.possible = false;

    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    const BallType wantType = player2Info.assignedType;
    std::vector<AIShotInfo> possibleShots;

    // --- Priority-Based Target Selection ---
    std::vector<Ball*> legalObjectBalls;
    Ball* eightBallTarget = nullptr;
    const bool on8 = IsPlayerOnEightBall(2);

    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue;
        if (b.id == 8) {
            if (on8) eightBallTarget = &b;
        }
        else if (wantType == BallType::NONE || b.type == wantType) {
            legalObjectBalls.push_back(&b);
        }
    }

    std::vector<Ball*> targetsToEvaluate;
    if (!legalObjectBalls.empty()) {
        targetsToEvaluate = legalObjectBalls;
    }
    else if (eightBallTarget) {
        targetsToEvaluate.push_back(eightBallTarget);
    }
    // --- End Priority-Based Target Selection ---

    for (Ball* targetBall : targetsToEvaluate) {
        for (int p = 0; p < 6; ++p) {
            AIShotInfo cand = EvaluateShot_8Ball(targetBall, p, false);
            // For very thin cuts, the score might be low. Give a small bonus to make them considerable.
            if (cand.possible) {
                D2D1_POINT_2F ghostPos = CalculateGhostBallPos(targetBall, pocketPositions[p]);
                float cutAmountSq = GetDistanceSq(ghostPos.x, ghostPos.y, targetBall->x, targetBall->y);
                // If the cut is very thin (ghost ball is far from target center), it's a "tipshot".
                if (cutAmountSq > (BALL_RADIUS * 1.9f) * (BALL_RADIUS * 1.9f)) {
                    cand.score += 50.0f; // Add a small bonus to make it a candidate
                }
            }
            if (cand.possible) possibleShots.push_back(cand);
        }
        if (aiDifficulty > EASY) {
            AIFindBankShots(targetBall, possibleShots);
        }
    }

    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (const auto& shot : possibleShots) {
            if (shot.score > bestShot.score) {
                bestShot = shot;
            }
        }
    }
    else {
        // --- NEW: INTELLIGENT SAFETY SHOT ---
        // If no makeable shots exist, find the easiest legal ball to hit and tap it softly.
        bestShot.possible = true;
        bestShot.score = -99999.0f;
        bestShot.spinX = bestShot.spinY = 0.0f;
        bestShot.pocketIndex = -1;

        Ball* bestSafetyTarget = nullptr;
        float minAngleToHit = 1000.0f; // Find the ball that requires the least "cut" to hit.

        // Use the same target list as for scoring shots.
        std::vector<Ball*> legalSafetyTargets = targetsToEvaluate.empty() ? legalObjectBalls : targetsToEvaluate;

        for (Ball* target : legalSafetyTargets) {
            D2D1_POINT_2F ghostPos = { target->x, target->y }; // Simplified aim for safety, just touch the ball.
            if (IsPathClear({ cue->x, cue->y }, ghostPos, cue->id, target->id)) {
                // Calculate angle difference (a proxy for how easy it is to hit)
                float angleToBall = atan2f(target->y - cue->y, target->x - cue->x);
                float angleDiff = fabsf(angleToBall - cueAngle); // Using global cueAngle as a reference
                if (angleDiff < minAngleToHit) {
                    minAngleToHit = angleDiff;
                    bestSafetyTarget = target;
                }
            }
        }

        if (bestSafetyTarget) {
            // Aim to just clip the edge of the easiest-to-hit legal ball.
            float angleToTarget = atan2f(bestSafetyTarget->y - cue->y, bestSafetyTarget->x - cue->x);
            // Aim slightly to the side to avoid a direct hit
            D2D1_POINT_2F ghostPos = {
                bestSafetyTarget->x - cosf(angleToTarget + 0.1f) * (BALL_RADIUS * 2.0f),
                bestSafetyTarget->y - sinf(angleToTarget + 0.1f) * (BALL_RADIUS * 2.0f)
            };
            bestShot.targetBall = bestSafetyTarget;
            bestShot.angle = atan2f(ghostPos.y - cue->y, ghostPos.x - cue->x);
            bestShot.power = MAX_SHOT_POWER * 0.2f; // Very soft tap
        }
        else {
            // Absolute fallback: a random soft hit (should be extremely rare now).
            bestShot.angle = (float)(rand() % 314) / 100.0f;
            bestShot.power = MAX_SHOT_POWER * 0.2f;
        }
    }
    return bestShot;
}

// --- NEW AI Strategy Function for Straight Pool ---
AIShotInfo AIFindBestShot_StraightPool() {
    AIShotInfo bestShot;
    bestShot.score = -99999.0f; bestShot.possible = false;
    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    std::vector<AIShotInfo> possibleShots;

    // Target Selection: Any numbered ball (1-15) on the table
    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue; // Skip cue and pocketed balls

        // Evaluate direct shots to all pockets for this ball
        for (int p = 0; p < 6; ++p) {
            // Use a modified EvaluateShot for Straight Pool scoring
            AIShotInfo cand = EvaluateShot_StraightPool(&b, p, false);
            if (cand.possible) {
                possibleShots.push_back(cand);
            }
        }
        // Evaluate bank shots (optional, maybe only for higher difficulty?)
        if (aiDifficulty > EASY) {
            // Need AIFindBankShots_StraightPool or adapt existing one
            // For now, let's skip banks to simplify
        }
    }

    // Select the best shot
    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (const auto& shot : possibleShots) {
            if (shot.score > bestShot.score) {
                bestShot = shot;
            }
        }
    }
    else {
        // Safety Shot Logic (Simplified: hit easiest ball softly)
        bestShot.possible = true; // Mark as possible (safety is always possible)
        bestShot.score = -99999.0f; // Very low score
        bestShot.spinX = 0; bestShot.spinY = 0; bestShot.pocketIndex = -1;

        Ball* easiestTarget = nullptr;
        float minHitDistSq = 1e10f;

        for (Ball& b : balls) {
            if (b.isPocketed || b.id == 0) continue;
            // Simplified ghost pos = ball center for safety check
            if (IsPathClear({ cue->x, cue->y }, { b.x, b.y }, cue->id, b.id)) {
                float distSq = GetDistanceSq(cue->x, cue->y, b.x, b.y);
                if (distSq < minHitDistSq) {
                    minHitDistSq = distSq;
                    easiestTarget = &b;
                }
            }
        }

        if (easiestTarget) {
            // Aim slightly off-center to just tap it
            float angleToTarget = atan2f(easiestTarget->y - cue->y, easiestTarget->x - cue->x);
            D2D1_POINT_2F ghostPos = {
                easiestTarget->x - cosf(angleToTarget + 0.1f) * BALL_RADIUS * 2.0f,
                easiestTarget->y - sinf(angleToTarget + 0.1f) * BALL_RADIUS * 2.0f
            };
            bestShot.targetBall = easiestTarget;
            bestShot.angle = atan2f(ghostPos.y - cue->y, ghostPos.x - cue->x);
            bestShot.power = MAX_SHOT_POWER * 0.2f; // Soft tap
        }
        else {
            // Absolute fallback (hit randomly, softly)
            bestShot.angle = (float)(rand() % 314) / 100.0f;
            bestShot.power = MAX_SHOT_POWER * 0.2f;
            bestShot.targetBall = nullptr; // No specific target
        }
    }
    return bestShot;
}

// --- NEW: 9-Ball Table State Evaluation (for positional leave) ---
float EvaluateTableState_NineBall(Ball* cueBall) {
    if (!cueBall) return 0.0f;
    float score = 0.0f;

    // Find what the *next* lowest ball on the table would be
    int nextLowestBall = -1;
    for (int id = lowestBallOnTable + 1; id <= 9; ++id) {
        Ball* b = GetBallById(id);
        if (b && !b->isPocketed) {
            nextLowestBall = id;
            break;
        }
    }

    if (nextLowestBall == -1) { // No other balls left, must be on the 9-ball
        nextLowestBall = 9;
    }

    Ball* nextTarget = GetBallById(nextLowestBall);
    if (!nextTarget) return 0.0f; // Should not happen

    // Score is based on how easy it is to shoot the *next* target
    for (int p = 0; p < 6; ++p) {
        D2D1_POINT_2F ghostPos = CalculateGhostBallPos(nextTarget, pocketPositions[p]);
        if (IsPathClear({ cueBall->x, cueBall->y }, ghostPos, cueBall->id, nextTarget->id)) {
            float dist = GetDistance(cueBall->x, cueBall->y, ghostPos.x, ghostPos.y) + GetDistance(nextTarget->x, nextTarget->y, pocketPositions[p].x, pocketPositions[p].y);
            score += std::max(0.0f, 200.0f - dist * 0.2f); // High bonus for an easy next shot
        }
    }
    return score;
}

// --- NEW: 9-Ball Shot Evaluation (similar to Straight Pool but simpler leave eval) ---
// --- NEW: 9-Ball Shot Evaluation (UPGRADED with 8-Ball Aiming Logic and Positional TODO) ---
AIShotInfo EvaluateShot_NineBall(Ball* targetBall, int pocketIndex, bool isBank, bool isCombo) {
    AIShotInfo shotInfo;
    shotInfo.possible = false;
    shotInfo.targetBall = targetBall;
    shotInfo.pocketIndex = pocketIndex;
    shotInfo.isBankShot = isBank;
    shotInfo.involves8Ball = (targetBall && targetBall->id == 9);

    Ball* cue = GetCueBall();
    if (!cue || !targetBall) return shotInfo;

    // 1. Core Industry Algorithm: Pure Ghost Ball Calculation
    D2D1_POINT_2F pocketPos = pocketPositions[pocketIndex];
    D2D1_POINT_2F targetPos = { targetBall->x, targetBall->y };

    // Ghost Ball center is exactly 2.0 radii away from target center on the aim line
    D2D1_POINT_2F ghostBallPos = CalculateGhostBallPos(targetBall, pocketPos);
    shotInfo.ghostBallPos = ghostBallPos;

    // 2. Physics-Informed Path Checks
    if (isCombo) {
        // For a combo, we only check the object ball's path to the pocket
        if (!isBank && !IsPathClear(targetPos, pocketPos, targetBall->id, -1)) return shotInfo;
    }
    else {
        // For a direct shot, check Cue to Ghost and Target to Pocket
        if (!IsPathClear({ cue->x, cue->y }, ghostBallPos, cue->id, targetBall->id)) return shotInfo;
        if (!isBank && !IsPathClear(targetPos, pocketPos, targetBall->id, -1)) return shotInfo;
    }

    // 3. Cut-Angle Feasibility Analysis (Professional EV Scoring)
    D2D1_POINT_2F vCue = { ghostBallPos.x - cue->x, ghostBallPos.y - cue->y };
    D2D1_POINT_2F vObj = { pocketPos.x - targetBall->x, pocketPos.y - targetBall->y };

    float magCue = sqrtf(vCue.x * vCue.x + vCue.y * vCue.y);
    float magObj = sqrtf(vObj.x * vObj.x + vObj.y * vObj.y);

    if (magCue < 1e-4f || magObj < 1e-4f) return shotInfo;

    // Note: When isCombo is true, magCue is the distance from the Lowest Ball (acting as cue) to the Ghost Ball
    float dot = (vCue.x * vObj.x + vCue.y * vObj.y) / (magCue * magObj);
    float cutAngle = acosf(std::clamp(dot, -1.0f, 1.0f));
    float cutScore = std::max(0.0f, 1.0f - cutAngle / (PI * 0.6f));

    // Pruning: Ignore shots that are physically improbable (extremely thin cuts)
    if (cutScore < 0.15f) return shotInfo;

    // 4. Final Heuristic Scoring
    float distPenalty = (magCue * 0.015f) + (magObj * 0.01f);
    shotInfo.score = (cutScore * 400.0f) - distPenalty;

    // Apply adjustments for 9-Ball strategy
    if (isBank) shotInfo.score -= 500.0f;
    if (isCombo) shotInfo.score += 500.0f; // Combos are high-value
    if (targetBall->id == lowestBallOnTable) shotInfo.score += 1000.0f; // Must hit this ball
    if (targetBall->id == 9) shotInfo.score += 10000.0f; // Max priority for the win

    // 5. Shot Parameter Generation
    shotInfo.angle = atan2f(ghostBallPos.y - cue->y, ghostBallPos.x - cue->x);
    shotInfo.power = CalculateShotPower(magCue, magObj, isBank);
    shotInfo.spinX = 0.0f;
    shotInfo.spinY = 0.0f;

    shotInfo.possible = true;
    return shotInfo;
}

// --- NEW: 9-Ball AI Top-Level Decision (with Push-Out Tweak) ---
AIShotInfo AIFindBestShot_NineBall() {
    AIShotInfo bestShot;
    bestShot.score = -99999.0f; bestShot.possible = false;
    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    Ball* targetBall = GetBallById(lowestBallOnTable);
    if (!targetBall) { /* All balls pocketed? Should be game over. */ return bestShot; }

    std::vector<AIShotInfo> possibleShots;

    // --- Push-Out Decision (Simplified) ---
    if (isPushOutAvailable) {
        // Simple rule: If AI can't see the lowest ball, or sees it but has no good shot, push out.
        bool canSeeTarget = IsPathClear({ cue->x, cue->y }, { targetBall->x, targetBall->y }, cue->id, targetBall->id);
        bool hasGoodShot = false;
        if (canSeeTarget) {
            for (int p = 0; p < 6; ++p) {
                // Use the (now much smarter) evaluation function
                AIShotInfo cand = EvaluateShot_NineBall(targetBall, p, false, false);
                // --- TWEAKED: Lowered threshold from 1000.0 to 500.0 ---
                // Now that eval is better, any decent shot is worth taking.
                if (cand.possible && cand.score > 500.0f) {
                    hasGoodShot = true;
                    possibleShots.push_back(cand); // Add it as a candidate
                    break;
                }
            }
        }

        if (hasGoodShot) {
            // AI elects to shoot.
            isPushOutAvailable = false;
            // Proceed to normal shot evaluation...
        }
        else {
            // AI elects to push out.
            isPushOutAvailable = false;
            isPushOutShot = true; // Flag this shot as a push-out
            // Find a safe spot to tap the cue ball to.
            bestShot.possible = true;
            bestShot.score = 0; // Not a "bad" shot
            bestShot.angle = atan2f((TABLE_TOP + TABLE_HEIGHT / 2.0f) - cue->y, (TABLE_LEFT + TABLE_WIDTH / 2.0f) - cue->x); // Aim to center
            bestShot.power = MAX_SHOT_POWER * 0.2f; // Soft tap
            return bestShot;
        }
    }

    // --- Standard Shot Evaluation ---

    // 1. Evaluate direct shots on the lowest ball
    for (int p = 0; p < 6; ++p) {
        AIShotInfo cand = EvaluateShot_NineBall(targetBall, p, false, false);
        if (cand.possible) possibleShots.push_back(cand);
    }

    // 2. Potent Deep Combo Search (IDS Replacement)
// Instead of manual math, we leverage the upgraded EvaluateShot_NineBall recursively.
    for (Ball& intermediate : balls) {
        // Only check combos on balls that are NOT the cue or the current lowest ball
        if (intermediate.isPocketed || intermediate.id == 0 || intermediate.id == targetBall->id) continue;

        for (int p = 0; p < 6; ++p) {
            // Step A: Evaluate the difficulty of pocketing the 'intermediate' ball
            AIShotInfo comboCand = EvaluateShot_NineBall(&intermediate, p, false, true); // true = isCombo

            if (comboCand.possible) {
                // Step B: If the intermediate ball can be pocketed, can the 'lowest ball' reach the required ghost position?
                // comboCand.ghostBallPos is the exact contact point calculated by our upgraded algorithm
                if (IsPathClear({ targetBall->x, targetBall->y }, comboCand.ghostBallPos, targetBall->id, intermediate.id)) {

                    // Step C: If clear, calculate where the CUE ball must hit the LOWEST ball to send it there
                    D2D1_POINT_2F finalGhostForCue = CalculateGhostBallPos(targetBall, comboCand.ghostBallPos);

                    // Step D: Path check for the Cue Ball
                    if (IsPathClear({ cue->x, cue->y }, finalGhostForCue, cue->id, targetBall->id)) {
                        // Success! Update shot details for the cue strike
                        comboCand.angle = atan2f(finalGhostForCue.y - cue->y, finalGhostForCue.x - cue->x);

                        // Score adjustment: combos are high-reward but higher-risk
                        comboCand.score += 1000.0f;
                        possibleShots.push_back(comboCand);
                    }
                }
            }
        }
    }

    // 3. Evaluate bank shots on lowest ball
    if (aiDifficulty > EASY) {
        AIFindBankShots(targetBall, possibleShots); // This now uses the 12-segment rim model
    }

    // 4. Select best shot
    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (const auto& shot : possibleShots) {
            if (shot.score > bestShot.score) {
                bestShot = shot;
            }
        }
    }
    else {
        // 5. No shots found, play safety (the "nudge" you were seeing)
        // This will happen much less often now.
        bestShot.possible = true;
        bestShot.score = -99999.0f;
        bestShot.spinX = 0; bestShot.spinY = 0; bestShot.pocketIndex = -1;
        float angleToTarget = atan2f(targetBall->y - cue->y, targetBall->x - cue->x);
        D2D1_POINT_2F ghostPos = {
            targetBall->x - cosf(angleToTarget + 0.1f) * BALL_RADIUS * 2.0f,
            targetBall->y - sinf(angleToTarget + 0.1f) * BALL_RADIUS * 2.0f
        };
        bestShot.targetBall = targetBall;
        bestShot.angle = atan2f(ghostPos.y - cue->y, ghostPos.x - cue->x);
        bestShot.power = MAX_SHOT_POWER * 0.2f;
    }
    return bestShot;
}

//[00000]
/*// --- REPLACED: Top-Level AI Decision Making ---
AIShotInfo AIFindBestShot() {
    AIShotInfo bestShot;
    bestShot.score = -99999.0f;
    bestShot.possible = false;

    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    const bool on8 = IsPlayerOnEightBall(2);
    const BallType wantType = player2Info.assignedType;
    std::vector<AIShotInfo> possibleShots;

    // 1. Find all legal balls the AI can target.
    std::vector<Ball*> legalTargets;
    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue;
        bool isLegalTarget = on8 ? (b.id == 8) : (wantType == BallType::NONE || b.type == wantType);
        if (isLegalTarget) {
            legalTargets.push_back(&b);
        }
    }

    // 2. Evaluate direct and bank shots for all legal targets.
    for (Ball* targetBall : legalTargets) {
        // Evaluate direct shots to all pockets
        for (int p = 0; p < 6; ++p) {
            AIShotInfo cand = EvaluateShot(targetBall, p, false);
            if (cand.possible) {
                possibleShots.push_back(cand);
            }
        }

        // Evaluate bank shots if difficulty is not Easy
        if (aiDifficulty > EASY) {
            AIFindBankShots(targetBall, possibleShots);
        }
    }

    // 3. Select the highest-scoring shot from all possibilities.
    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (const auto& shot : possibleShots) {
            if (shot.score > bestShot.score) {
                bestShot = shot;
            }
        }
    }
    else {
        // 4. If no shots are possible, play the best available safety.
        bestShot.possible = true;
        bestShot.score = -99999.0f;
        bestShot.spinX = bestShot.spinY = 0.0f;
        bestShot.pocketIndex = -1;

        if (!legalTargets.empty()) {
            // Safety: Gently hit the easiest-to-reach legal ball towards the center of a rail.
            Ball* safetyTarget = legalTargets[0];
            float minDist = GetDistanceSq(cue->x, cue->y, safetyTarget->x, safetyTarget->y);
            for (size_t i = 1; i < legalTargets.size(); ++i) {
                float dist = GetDistanceSq(cue->x, cue->y, legalTargets[i]->x, legalTargets[i]->y);
                if (dist < minDist) {
                    minDist = dist;
                    safetyTarget = legalTargets[i];
                }
            }

            bestShot.targetBall = safetyTarget;
            D2D1_POINT_2F safetyAimPoint = { TABLE_LEFT + TABLE_WIDTH / 2.0f, TABLE_TOP + INNER_RIM_THICKNESS };
            D2D1_POINT_2F ghostPos = CalculateGhostBallPos(safetyTarget, safetyAimPoint);
            bestShot.angle = atan2f(ghostPos.y - cue->y, ghostPos.x - cue->x);
            bestShot.power = MAX_SHOT_POWER * 0.3f; // Soft safety tap
        }
        else {
            // Absolute fallback (should never happen): a random soft hit.
            bestShot.angle = (float)(rand() % 314) / 100.0f;
            bestShot.power = MAX_SHOT_POWER * 0.2f;
        }
    }
    return bestShot;
}*/

/*// High-level planner that decides which shot to take.
AIShotInfo AIFindBestShot() {
    AIShotInfo bestShot;
    bestShot.score = -99999.0f; // Initialize with a very low score
    bestShot.possible = false;

    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    const bool on8 = IsPlayerOnEightBall(2);
    const BallType wantType = player2Info.assignedType;
    std::vector<AIShotInfo> possibleShots;

    // 1. Evaluate all possible direct and bank shots on legal targets.
    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue;

        bool isLegalTarget = on8 ? (b.id == 8) : (wantType == BallType::NONE || b.type == wantType);
        if (!isLegalTarget) continue;

        // A. Evaluate direct shots to all pockets
        for (int p = 0; p < 6; ++p) {
            AIShotInfo cand = EvaluateShot(&b, p, false); // false = not a bank shot
            if (cand.possible) {
                possibleShots.push_back(cand);
            }
        }

        // B. Evaluate bank shots if difficulty is not Easy
        if (aiDifficulty > EASY) {
            AIFindBankShots(&b, possibleShots);
        }
    }

    // 2. Find the best shot from the list of possibilities.
    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (size_t i = 1; i < possibleShots.size(); ++i) {
            if (possibleShots[i].score > bestShot.score) {
                bestShot = possibleShots[i];
            }
        }
    }
    else {
        // 3. If no makeable shots, play a defensive "safety" shot.
        bestShot.possible = true;
        bestShot.score = -99999.0f;
        bestShot.spinX = bestShot.spinY = 0.0f;
        bestShot.pocketIndex = -1;
        bestShot.targetBall = nullptr;

        Ball* legalSafetyTarget = nullptr;
        for (Ball& b : balls) {
            if (b.isPocketed || b.id == 0) continue;
            bool isLegalTarget = on8 ? (b.id == 8) : (wantType == BallType::NONE || b.type == wantType);
            if (isLegalTarget) { legalSafetyTarget = &b; break; }
        }

        if (legalSafetyTarget) {
            // Smart safety: Hit the legal ball softly toward a rail, away from pockets.
            float dx = legalSafetyTarget->x - (TABLE_LEFT + TABLE_WIDTH / 2.0f);
            float dy = legalSafetyTarget->y - (TABLE_TOP + TABLE_HEIGHT / 2.0f);
            float angleToCenter = atan2f(dy, dx);

            // Aim to hit the ball on the side, perpendicular to the line to the center
            float hitAngle = angleToCenter + PI / 2.0f;

            D2D1_POINT_2F ghostPos = {
                legalSafetyTarget->x - cosf(hitAngle) * BALL_RADIUS * 2.0f,
                legalSafetyTarget->y - sinf(hitAngle) * BALL_RADIUS * 2.0f
            };

            bestShot.angle = atan2f(ghostPos.y - cue->y, ghostPos.x - cue->x);
            bestShot.power = MAX_SHOT_POWER * 0.25f; // A soft tap
        }
        else {
            // Fallback safety (should be rare): hit in a random safe-ish direction
            bestShot.angle = static_cast<float>(rand()) / RAND_MAX * PI; // Aim towards top half
            bestShot.power = MAX_SHOT_POWER * 0.2f;
        }
    }

    return bestShot;
}*/

// High-level planner that decides which shot to take.
/*AIShotInfo AIFindBestShot() {
    AIShotInfo bestShot;
    bestShot.possible = false;

    Ball* cue = GetCueBall();
    if (!cue) return bestShot;

    const bool on8 = IsPlayerOnEightBall(2);
    const BallType wantType = player2Info.assignedType;
    std::vector<AIShotInfo> possibleShots;

    // 1. Evaluate all possible direct shots on legal targets.
    for (Ball& b : balls) {
        if (b.isPocketed || b.id == 0) continue;

        bool isLegalTarget = on8 ? (b.id == 8) : (wantType == BallType::NONE || b.type == wantType);
        if (!isLegalTarget) continue;

        for (int p = 0; p < 6; ++p) {
            AIShotInfo cand = EvaluateShot(&b, p, false); // false = not a bank shot
            if (cand.possible) {
                possibleShots.push_back(cand);
            }
        }
    }

    // TODO: Add evaluation for bank shots here if desired.

    // 2. Find the best shot from the list of possibilities.
    if (!possibleShots.empty()) {
        bestShot = possibleShots[0];
        for (size_t i = 1; i < possibleShots.size(); ++i) {
            if (possibleShots[i].score > bestShot.score) {
                bestShot = possibleShots[i];
            }
        }
    }
    else {
        // 3. If no makeable shots are found, play a defensive "safety" shot.
        // This involves hitting a legal ball softly to a safe location.
        bestShot.possible = true;
        bestShot.angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * PI;
        bestShot.power = MAX_SHOT_POWER * 0.25f; // A soft safety hit.
        bestShot.spinX = bestShot.spinY = 0.0f;
        bestShot.targetBall = nullptr;
        bestShot.score = -99999.0f; // Safety is a last resort.
        bestShot.pocketIndex = -1;
    }

    return bestShot;
}*/

// The top-level function that orchestrates the AI's turn.
void AIMakeDecision() {
    aiPlannedShotDetails.isValid = false;
    Ball* cueBall = GetCueBall();
    if (!cueBall || !isPlayer2AI || currentPlayer != 2) return;

    AIShotInfo bestShot;
    // --- Branch AI strategy based on Game Type ---
    if (currentGameType == GameType::EIGHT_BALL_MODE) {
        bestShot = AIFindBestShot_8Ball(); // Use existing 8-ball logic (potentially rename func)
    }
    else if (currentGameType == GameType::NINE_BALL) {
        bestShot = AIFindBestShot_NineBall(); // Use new 9-ball logic
    }
    else { // STRAIGHT_POOL
        bestShot = AIFindBestShot_StraightPool(); // Call new Straight Pool logic
    }

    if (bestShot.possible) {
        // Call pocket only needed in 8-Ball
        if (currentGameType == GameType::EIGHT_BALL_MODE && bestShot.involves8Ball) { // 8-ball is ID 8
            calledPocketP2 = bestShot.pocketIndex;
        }
        else {
            calledPocketP2 = -1; // Ensure it's reset otherwise
        }

        // Load the chosen shot parameters into the game state.
        aiPlannedShotDetails.angle = bestShot.angle;
        aiPlannedShotDetails.power = bestShot.power;
        aiPlannedShotDetails.spinX = bestShot.spinX;
        aiPlannedShotDetails.spinY = bestShot.spinY;
        aiPlannedShotDetails.isValid = true;

        // Set global aiming variables for visualization
        // Use high-precision vector injection to ensure zero drift
        cueAngle = aiPlannedShotDetails.angle;
        shotPower = aiPlannedShotDetails.power;
        cueSpinX = aiPlannedShotDetails.spinX;
        cueSpinY = aiPlannedShotDetails.spinY;
        aiIsDisplayingAim = true;
        aiAimDisplayFramesLeft = AI_AIM_DISPLAY_DURATION_FRAMES;

    }
    else {
        // No valid shot found (rare, fallback to safety handled within FindBestShot funcs)
         // If FindBestShot returned possible=false (absolute failure), switch turns
        aiPlannedShotDetails.isValid = false;
        SwitchTurns(); // Concede turn if no shot or safety is possible
    }
}

// --- END: ENHANCED AI LOGIC ---



//  Estimate the power that will carry the cue-ball to the ghost position
//  *and* push the object-ball the remaining distance to the pocket.
//
//  • cueToGhostDist    – pixels from cue to ghost-ball centre
//  • targetToPocketDist– pixels from object-ball to chosen pocket
//
//  The function is fully deterministic (good for AI search) yet produces
//  human-looking power levels.
//


// ------------------------------------------------------------------
//  Return the ghost-ball centre needed for the target ball to roll
//  straight into the chosen pocket.
// ------------------------------------------------------------------
// --- REPLACED: Highly Accurate Ghost Ball Calculation ---
// Calculates the precise point of contact (ghost ball center) for any given shot.
/*D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, D2D1_POINT_2F pocketPos) {
    if (!targetBall) return { 0, 0 };
    D2D1_POINT_2F targetCenter = { targetBall->x, targetBall->y };

    // Vector from target to pocket
    float dx = pocketPos.x - targetCenter.x;
    float dy = pocketPos.y - targetCenter.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 1e-6f) return targetCenter;

    // Professional Pivot: Aim 1.2x radius deep to ensure the ball clears the "horn" of the chamfer
    float pocketInset = BALL_RADIUS * 1.2f;
    float targetX = pocketPos.x + (dx / dist) * pocketInset;
    float targetY = pocketPos.y + (dy / dist) * pocketInset;

    // Recalculate direction to the new 'deep' target
    dx = targetX - targetCenter.x;
    dy = targetY - targetCenter.y;
    dist = sqrtf(dx * dx + dy * dy);

    // The ghost ball is exactly 2.0 radii away on the line of centers
    return { targetCenter.x - (dx / dist) * (BALL_RADIUS * 2.0f),
             targetCenter.y - (dy / dist) * (BALL_RADIUS * 2.0f) };
}*/
D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, D2D1_POINT_2F pocketPos) {
    if (!targetBall) return { 0, 0 };

    // Industry Standard Ghost Ball Algorithm:
    // The ghost ball center is exactly 2.0 radii away from the target ball's center,
    // aligned perfectly with the line connecting the pocket center to the target center.
    float dx = targetBall->x - pocketPos.x;
    float dy = targetBall->y - pocketPos.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return { targetBall->x, targetBall->y };

    return {
        targetBall->x + (dx / len) * BALL_RADIUS * 2.0f,
        targetBall->y + (dy / len) * BALL_RADIUS * 2.0f
    };
}

/*D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, D2D1_POINT_2F pocketPos)
{
    if (!targetBall) return D2D1::Point2F(0, 0);

    float vx = pocketPos.x - targetBall->x;
    float vy = pocketPos.y - targetBall->y;
    float L = sqrtf(vx * vx + vy * vy);
    if (L < 1.0f) L = 1.0f;                // safety
    vx /= L;   vy /= L;

    // compute pocket visual radius from the nearest pocket position (if you have pocket index available prefer that)
    // But since this function receives pocketPos, we will pick a conservative inset factor consistent with GhostPos.
    const float ghostOffset = BALL_RADIUS * 2.2f;

    return D2D1::Point2F(
        targetBall->x - vx * ghostOffset,
        targetBall->y - vy * ghostOffset);
}*/

/*D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, D2D1_POINT_2F pocketPos)
//D2D1_POINT_2F CalculateGhostBallPos(Ball* targetBall, int pocketIndex)
{
    if (!targetBall) return D2D1::Point2F(0, 0);

    //D2D1_POINT_2F P = pocketPositions[pocketIndex];

    float vx = pocketPos.x - targetBall->x;
    float vy = pocketPos.y - targetBall->y;
    //float vx = P.x - targetBall->x;
    //float vy = P.y - targetBall->y;
    float L = sqrtf(vx * vx + vy * vy);
    if (L < 1.0f) L = 1.0f;                // safety

    vx /= L;   vy /= L;

    return D2D1::Point2F(
        targetBall->x - vx * (BALL_RADIUS * 2.0f),
        targetBall->y - vy * (BALL_RADIUS * 2.0f));
}*/

// Calculate the position the cue ball needs to hit for the target ball to go towards the pocket
// ────────────────────────────────────────────────────────────────
//   2.  Shot evaluation & search
// ────────────────────────────────────────────────────────────────

//  Calculate ghost-ball position so that cue hits target towards pocket
static inline D2D1_POINT_2F GhostPos(const Ball* tgt, int pocketIdx)
{
    // Compute an aiming ghost that references the pocket *opening* not merely the pocket center.
    // This pushes the ghost slightly deeper so AI doesn't graze chamfer tips.
    D2D1_POINT_2F P = pocketPositions[pocketIdx];
    float vx = P.x - tgt->x;
    float vy = P.y - tgt->y;
    float L = sqrtf(vx * vx + vy * vy);
    if (L < 1e-6f) L = 1.0f;
    vx /= L;  vy /= L;

    // Determine pocket visual radius (middle pockets are larger)
    float pocketR = (pocketIdx == 1 || pocketIdx == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
    // Aim a bit inside the pocket opening (a fraction of the pocket radius) so shots go into the mouth.
    const float pocketInsetFactor = 0.55f; // 0..1, how deep from pocket center we consider the "opening center"
    D2D1_POINT_2F pocketEdge = D2D1::Point2F(P.x - vx * (pocketR * pocketInsetFactor), P.y - vy * (pocketR * pocketInsetFactor));

    // Ghost offset: distance behind the target ball where the cue ball should contact it.
    // Make this slightly larger than 2*BALL_RADIUS to avoid grazing the chamfer.
    const float ghostOffset = BALL_RADIUS * 2.2f;

    // Return the ghost position relative to the target ball along the direction to the pocketEdge
    return D2D1::Point2F(tgt->x - vx * ghostOffset, tgt->y - vy * ghostOffset);
}

/*static inline D2D1_POINT_2F GhostPos(const Ball* tgt, int pocketIdx)
{
    D2D1_POINT_2F P = pocketPositions[pocketIdx];
    float vx = P.x - tgt->x;
    float vy = P.y - tgt->y;
    float L = sqrtf(vx * vx + vy * vy);
    vx /= L;  vy /= L;
    return D2D1::Point2F(tgt->x - vx * (BALL_RADIUS * 2.0f),
        tgt->y - vy * (BALL_RADIUS * 2.0f));
}*/

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
//[00000]

//  Test if the capsule [ start … end ] (radius = BALL_RADIUS)
//  intersects any ball except the ids we want to ignore.
bool IsPathClear(D2D1_POINT_2F start, D2D1_POINT_2F end, int ignoredBallId1, int ignoredBallId2) {
    // --- FOOLPROOF FIX: Corrected collision radius check ---
    // The path is clear only if the distance from the aim line to the center of any
    // other ball is greater than the sum of their radii (2 * BALL_RADIUS).
    /*const float requiredSeparation = BALL_RADIUS * 2.0f;
    const float requiredSeparationSq = requiredSeparation * requiredSeparation;*/

    // Industry Standard: Path is clear if no obstructing ball center is within
    // a 1.05x ball radius margin of the line segment.
    const float clearanceRadius = BALL_RADIUS * 1.05f;
    const float clearanceRadiusSq = clearanceRadius * clearanceRadius;

    // 1. Ball obstruction check
    for (const Ball& b : balls) {
        if (b.isPocketed) continue;
        if (b.id == ignoredBallId1 || b.id == ignoredBallId2) continue;

        // Use a slightly larger radius for the check to be more conservative and avoid grazing shots.
        //if (PointToLineSegmentDistanceSq(D2D1::Point2F(b.x, b.y), start, end) < requiredSeparationSq * 1.1f) {
        if (PointToLineSegmentDistanceSq(D2D1::Point2F(b.x, b.y), start, end) < clearanceRadiusSq) {
            return false; // Path is blocked by another ball
        }
    }

    // 2. Rim intersection check (no changes needed for this part)
    D2D1_POINT_2F vertices[12] = {
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE }, { TABLE_LEFT + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS },
        { TABLE_RIGHT - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CHAMFER_SIZE },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE }, { TABLE_RIGHT - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS },
        { TABLE_LEFT + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CHAMFER_SIZE },
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2 }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2 },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + CHAMFER_SIZE / 2 }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - CHAMFER_SIZE / 2 }
    };
    D2D1_POINT_2F segments[12][2] = {
        { vertices[1], vertices[2] }, { vertices[2], vertices[3] },
        { vertices[3], vertices[11] }, { vertices[11], vertices[10] },
        { vertices[10], vertices[4] }, { vertices[4], vertices[5] },
        { vertices[5], vertices[6] }, { vertices[6], vertices[7] },
        { vertices[7], vertices[9] }, { vertices[9], vertices[8] },
        { vertices[8], vertices[0] }, { vertices[0], vertices[1] }
    };

    D2D1_POINT_2F intersection;
    for (int j = 0; j < 12; ++j) {
        if (LineSegmentIntersection(start, end, segments[j][0], segments[j][1], intersection)) {
            bool insidePocketMouth = false;
            for (int p = 0; p < 6; ++p) {
                float acceptanceRadius = GetPocketVisualRadius(p) + 8.0f; // Allow path to pass near pocket
                if (GetDistanceSq(intersection.x, intersection.y, pocketPositions[p].x, pocketPositions[p].y) < acceptanceRadius * acceptanceRadius) {
                    insidePocketMouth = true;
                    break;
                }
            }
            if (!insidePocketMouth) return false; // Blocked by a solid part of the rim
        }
    }

    return true; // Path is clear
}

/*//  Test if the capsule [ start … end ] (radius = BALL_RADIUS)
//  intersects any ball except the ids we want to ignore.
bool IsPathClear(D2D1_POINT_2F start, D2D1_POINT_2F end, int ignoredBallId1, int ignoredBallId2) {
    // --- FOOLPROOF FIX: Check for collisions against other balls AND the V-Cut inner rim ---

    // 1) Ball obstruction check (capsule vs centers)
    // Use a conservative radius so lines grazing balls are considered blocked.
    const float capsuleRadius = BALL_RADIUS * 1.05f; // a touch bigger than ball radius
    const float capsuleRadiusSq = capsuleRadius * capsuleRadius;
    for (const Ball& b : balls) {
        if (b.isPocketed) continue;
        if (b.id == ignoredBallId1 || b.id == ignoredBallId2) continue;
        float d2 = PointToLineSegmentDistanceSq(D2D1::Point2F(b.x, b.y), start, end);
        if (d2 < capsuleRadiusSq) return false;
    }

    // 2) Rim intersection check — allow intersections that occur inside pocket mouths.
    const float vCutDepth = 35.0f; // keep in sync with CheckCollisions()
    D2D1_POINT_2F vertices[12] = {
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + vCutDepth }, { TABLE_LEFT + vCutDepth, TABLE_TOP + INNER_RIM_THICKNESS },
        { TABLE_RIGHT - vCutDepth, TABLE_TOP + INNER_RIM_THICKNESS }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + vCutDepth },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - vCutDepth }, { TABLE_RIGHT - vCutDepth, TABLE_BOTTOM - INNER_RIM_THICKNESS },
        { TABLE_LEFT + vCutDepth, TABLE_BOTTOM - INNER_RIM_THICKNESS }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - vCutDepth },
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - vCutDepth / 2 }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + vCutDepth / 2 },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + vCutDepth / 2 }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - vCutDepth / 2 }
    };
    D2D1_POINT_2F segments[12][2] = {
        { vertices[1], vertices[2] }, { vertices[2], vertices[3] },
        { vertices[3], vertices[11] }, { vertices[11], vertices[10] },
        { vertices[10], vertices[4] }, { vertices[4], vertices[5] },
        { vertices[5], vertices[6] }, { vertices[6], vertices[7] },
        { vertices[7], vertices[9] }, { vertices[9], vertices[8] },
        { vertices[8], vertices[0] }, { vertices[0], vertices[1] }
    };

    D2D1_POINT_2F intersection;
    for (int j = 0; j < 12; ++j) {
        if (LineSegmentIntersection(start, end, segments[j][0], segments[j][1], intersection)) {
            // If intersection point is inside/near a pocket opening, treat as non-blocking.
            bool insidePocket = false;
            for (int p = 0; p < 6; ++p) {
                float pr = (p == 1 || p == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
                const float pocketTolerance = 6.0f; // forgiveness so slightly off-center shots count as into the mouth
                float acceptance = pr + pocketTolerance;
                if (GetDistanceSq(intersection.x, intersection.y, pocketPositions[p].x, pocketPositions[p].y) <= (acceptance * acceptance)) {
                    insidePocket = true;
                    break;
                }
            }
            if (insidePocket) continue; // allow the line through the pocket mouth
            return false; // real rim hit outside any pocket mouth — blocked
        }
    }

    // No blocking ball or rim intersection
    return true;
}*/

/*bool IsPathClear(D2D1_POINT_2F start, D2D1_POINT_2F end, int ignoredBallId1, int ignoredBallId2) {
    // --- FOOLPROOF FIX 1: Check for collisions against other balls AND the new V-Cut rim ---

    // 1. Check for obstructing balls (this part of the logic is correct and preserved)
    for (const Ball& b : balls) {
        if (b.isPocketed) continue;
        if (b.id == ignoredBallId1 || b.id == ignoredBallId2) continue;

        float distSq = PointToLineSegmentDistanceSq(D2D1::Point2F(b.x, b.y), start, end);
        if (distSq < (BALL_RADIUS * 2.0f) * (BALL_RADIUS * 2.0f)) {
            return false; // Path is blocked by another ball
        }
    }

    // 2. NEW: Check if the shot path intersects with any of the 12 inner rim segments.
    // This uses the exact same geometry as the physics engine for perfect accuracy.
    const float vCutDepth = 35.0f; // This MUST match the value in CheckCollisions
    D2D1_POINT_2F vertices[12] = {
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + vCutDepth }, { TABLE_LEFT + vCutDepth, TABLE_TOP + INNER_RIM_THICKNESS },
        { TABLE_RIGHT - vCutDepth, TABLE_TOP + INNER_RIM_THICKNESS }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + vCutDepth },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - vCutDepth }, { TABLE_RIGHT - vCutDepth, TABLE_BOTTOM - INNER_RIM_THICKNESS },
        { TABLE_LEFT + vCutDepth, TABLE_BOTTOM - INNER_RIM_THICKNESS }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - vCutDepth },
        { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - vCutDepth / 2 }, { TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + vCutDepth / 2 },
        { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 + vCutDepth / 2 }, { TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + TABLE_HEIGHT / 2 - vCutDepth / 2 }
    };
    D2D1_POINT_2F segments[12][2] = {
        { vertices[1], vertices[2] }, { vertices[2], vertices[3] },
        { vertices[3], vertices[11] }, { vertices[11], vertices[10] },
        { vertices[10], vertices[4] }, { vertices[4], vertices[5] },
        { vertices[5], vertices[6] }, { vertices[6], vertices[7] },
        { vertices[7], vertices[9] }, { vertices[9], vertices[8] },
        { vertices[8], vertices[0] }, { vertices[0], vertices[1] }
    };

    D2D1_POINT_2F intersection;
    for (int j = 0; j < 12; ++j) {
        if (LineSegmentIntersection(start, end, segments[j][0], segments[j][1], intersection)) {
            return false; // Path intersects with the inner rim.
        }
    }

    return true; // Path is clear of both balls and the rim.
}*/

/*bool IsPathClear(D2D1_POINT_2F start,
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
}*/

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
// midi func
void PlayMidiInBackground(HWND hwnd, std::wstring midiPath) {
    // This self-contained loop handles playback and looping until isMusicPlaying is false.
    while (isMusicPlaying.load()) {
        MCI_OPEN_PARMS mciOpen = { 0 };
        mciOpen.lpstrDeviceType = TEXT("sequencer");
        mciOpen.lpstrElementName = midiPath.c_str();

        // Try to open the MIDI device for playback.
        if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)&mciOpen) != 0) {
            // If opening fails, wait a second before retrying to avoid errors.
            Sleep(1000);
            continue; // Go to the next iteration of the while loop.
        }

        // Store the device ID. It's only valid for this single playback cycle.
        midiDeviceID = mciOpen.wDeviceID;

        // Play the sound without the MCI_NOTIFY flag. We will check its status ourselves.
        MCI_PLAY_PARMS mciPlay = { 0 };
        mciSendCommand(midiDeviceID, MCI_PLAY, 0, (DWORD_PTR)&mciPlay);

        // This is the "polling" loop. It waits here until the music finishes
        // or until the user mutes the sound.
        MCI_STATUS_PARMS mciStatus = { 0 };
        mciStatus.dwItem = MCI_STATUS_MODE;
        do {
            // If the user mutes the music, isMusicPlaying will become false.
            // We must break out immediately to stop the music.
            if (!isMusicPlaying.load()) {
                break;
            }
            Sleep(500); // Wait for half a second before checking the status again.
            mciSendCommand(midiDeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&mciStatus);
        } while (mciStatus.dwReturn == MCI_MODE_PLAY); // Loop as long as the status is "playing".

        // CRITICAL: Stop and Close the device after each playback.
        // This ensures the next loop starts cleanly from the beginning.
        mciSendCommand(midiDeviceID, MCI_STOP, 0, NULL);
        mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
        midiDeviceID = 0;
    }
}
/*// --- Robust looping MIDI playback (thread owns device & loops until signalled to stop) ---
// Thread function: owns midiPath (copy) and the opened device
// ----------------------------
// Robust MIDI looping implementation
// Runs in a background thread. Opens the device, loops playback by polling
// MCI_STATUS until the track ends, then seeks to start and plays again.
// Stops cleanly when isMusicPlaying becomes false.
// ----------------------------
void PlayMidiInBackground(HWND hwnd, std::wstring midiPath) {
    // Open the MIDI device
    MCI_OPEN_PARMS mciOpen = { 0 };
    mciOpen.lpstrDeviceType = TEXT("sequencer");
    mciOpen.lpstrElementName = midiPath.c_str();
    if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)&mciOpen) != 0) {
        return; // failed to open device
    }

    midiDeviceID = mciOpen.wDeviceID;

    // Loop while requested
    while (isMusicPlaying.load()) {
        // Start playing (synchronous PLAY - we will poll status)
        MCI_PLAY_PARMS play = { 0 };
        if (mciSendCommand(midiDeviceID, MCI_PLAY, 0, (DWORD_PTR)&play) != 0) {
            // can't play -> break the loop and cleanup
            break;
        }

        // Poll playback status until it stops or we are asked to stop
        MCI_STATUS_PARMS status = { 0 };
        status.dwItem = MCI_STATUS_MODE;
        while (isMusicPlaying.load()) {
            if (mciSendCommand(midiDeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&status) != 0) {
                // status query failed -> abort playback loop
                break;
            }
            if (status.dwReturn != MCI_MODE_PLAY) break; // playback finished
            Sleep(100); // small sleep to avoid busy-looping
        }

        if (!isMusicPlaying.load()) break;

        // Seek to start for next iteration (safe to call)
        MCI_SEEK_PARMS seek = { 0 };
        mciSendCommand(midiDeviceID, MCI_SEEK, MCI_SEEK_TO_START, (DWORD_PTR)&seek);
        // next loop iteration issues PLAY again
    }

    // Cleanup - make sure device is stopped and closed
    if (midiDeviceID != 0) {
        mciSendCommand(midiDeviceID, MCI_STOP, 0, NULL);
        mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
        midiDeviceID = 0;
    }
}*/
/*void PlayMidiInBackground(HWND hwnd, std::wstring midiPath) {
    // This function now runs in a short-lived thread.
    // Its only job is to open the device and start the first playback.
    // The device will remain open until StopMidi() is called.

    MCI_OPEN_PARMS mciOpen = { 0 };
    mciOpen.lpstrDeviceType = TEXT("sequencer");
    mciOpen.lpstrElementName = midiPath.c_str();

    // Attempt to open the MIDI device.
    if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)&mciOpen) == 0) {
        // Success! Store the device ID globally.
        midiDeviceID = mciOpen.wDeviceID;

        // Start the first playback, requesting a notification message when it finishes.
        MCI_PLAY_PARMS mciPlay = { 0 };
        mciPlay.dwCallback = (DWORD_PTR)hwnd; // Send MM_MCINOTIFY to our main window
        mciSendCommand(midiDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&mciPlay);
    }
    // The thread will now exit, but the music continues playing in the background.
}*/
// PlayMidiInBackground now takes ownership of the path (std::wstring copied into the thread)
/*void PlayMidiInBackground(HWND hwnd, std::wstring midiPath) {
        // use midiPath.c_str() when calling MCI functions
        MCI_OPEN_PARMS mciOpen = { 0 };
        mciOpen.lpstrDeviceType = TEXT("sequencer");
        mciOpen.lpstrElementName = midiPath.c_str();
        if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)&mciOpen) != 0) {
            return; // Failed to open device, exit thread.
        }
        midiDeviceID = mciOpen.wDeviceID;

        MCI_PLAY_PARMS mciPlay = { 0 };
        mciPlay.dwCallback = (DWORD_PTR)hwnd;
        mciSendCommand(midiDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&mciPlay);

        while (isMusicPlaying) {
            Sleep(250);
        }

        mciSendCommand(midiDeviceID, MCI_STOP, 0, NULL);
        mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
        midiDeviceID = 0;
    }*/
    /*// --- FOOLPROOF MCI LOGIC ---
    // 1. Open the MIDI device once at the start of the thread.
    MCI_OPEN_PARMS mciOpen = { 0 };
    mciOpen.lpstrDeviceType = TEXT("sequencer");
    mciOpen.lpstrElementName = midiPath;
    if (mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD_PTR)&mciOpen) != 0) {
        return; // Failed to open device, exit thread.
    }
    midiDeviceID = mciOpen.wDeviceID;

    // 2. Start the first playback. MCI_NOTIFY will send a message to our window when it's done.
    MCI_PLAY_PARMS mciPlay = { 0 };
    mciPlay.dwCallback = (DWORD_PTR)hwnd;
    mciSendCommand(midiDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD_PTR)&mciPlay);

    // 3. The thread now just waits peacefully until it's told to stop.
    while (isMusicPlaying) {
        Sleep(250); // Sleep in intervals to avoid busy-waiting.
    }

    // 4. When isMusicPlaying becomes false, the loop breaks. Clean up the device.
    mciSendCommand(midiDeviceID, MCI_STOP, 0, NULL);
    mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
    midiDeviceID = 0;*/
    /*while (isMusicPlaying) {
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
    }*/
    //}

    // StartMidi (std::wstring) - primary start function
void StartMidi(HWND hwnd, const std::wstring& midiPath) {
    // If already playing, do nothing
    if (isMusicPlaying.load()) return;

    // Ensure any prior thread finished
    if (musicThread.joinable()) {
        musicThread.join();
    }

    // Mark playing and start thread (std::wstring copied into thread)
    isMusicPlaying.store(true);
    musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);
    // do NOT detach — keep joinable so StopMidi can wait on it
}

// Convenience overload for const TCHAR* call sites
void StartMidi(HWND hwnd, const TCHAR* midiPath) {
    if (!midiPath) return;
    StartMidi(hwnd, std::wstring(midiPath));
}
/*void StartMidi(HWND hwnd, const std::wstring& midiPath) {
    // If music is already playing (or a device is open), do nothing.
    if (isMusicPlaying) {
        return;
    }
    isMusicPlaying = true;
    // Launch a new, short-lived thread to begin playback.
    musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);
    // Detach the thread, allowing it to run independently and exit when done.
    musicThread.detach();
}*/
/*if (musicThread.joinable()) {
    musicThread.join();
}
isMusicPlaying = true;
// pass midiPath by value into the thread (std::thread will copy the std::wstring)
musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);*/
/*if (musicThread.joinable()) {
    musicThread.join();
}
isMusicPlaying = true;
// pass midiPath by value into the thread (std::thread will copy the std::wstring)
musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);*/
/*// Before starting a new thread, ensure the old one (if any) is finished.
if (musicThread.joinable()) {
    musicThread.join();
}
// Set the flag to true so the new thread's loop will execute.
isMusicPlaying = true;
// Create and launch the new background thread.
musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);*/
/*if (isMusicPlaying) {
    StopMidi();
}
isMusicPlaying = true;
musicThread = std::thread(PlayMidiInBackground, hwnd, midiPath);*/
//}

void StopMidi() {
    // Signal the thread to stop
    if (!isMusicPlaying.load()) return;
    isMusicPlaying.store(false);

    // If a device is open, ask it to stop immediately (thread may also close it)
    if (midiDeviceID != 0) {
        mciSendCommand(midiDeviceID, MCI_STOP, 0, NULL);
    }

    // Wait for background thread to finish and clean up
    if (musicThread.joinable()) {
        musicThread.join();
    }

    // Ensure device closed
    if (midiDeviceID != 0) {
        mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
        midiDeviceID = 0;
    }
}
/*void StopMidi() {
    // Check if music is supposed to be playing.
    if (isMusicPlaying) {
        isMusicPlaying = false; // Signal to the WndProc loop to stop re-playing.

        // If a device is open, stop playback and close it.
        if (midiDeviceID != 0) {
            mciSendCommand(midiDeviceID, MCI_STOP, 0, NULL);
            mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
            midiDeviceID = 0;
        }
    }
}*/
/*if (isMusicPlaying) {
    isMusicPlaying = false; // Signal the background thread to terminate its loop.
    if (musicThread.joinable()) {
        musicThread.join(); // Wait for the thread to finish completely and clean up MCI.
    }
}*/
/*if (isMusicPlaying) {
    isMusicPlaying = false;
    if (musicThread.joinable()) musicThread.join();
    if (midiDeviceID != 0) {
        mciSendCommand(midiDeviceID, MCI_CLOSE, 0, NULL);
        midiDeviceID = 0;
    }
}*/
//}

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

// --- REPLACED: Professional Path Geometry Drawing for High-Fidelity Trails ---
void DrawBallTrails(ID2D1RenderTarget* pRT, ID2D1Factory* pFactory) {
    if (!g_tracePaths || !pRT || !pFactory) return;

    ID2D1SolidColorBrush* pTrailBrush = nullptr;
    HRESULT hr = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.5f), &pTrailBrush);
    if (FAILED(hr)) return;

    // Helper Lambda to render a set of trails using Path Geometries
    auto RenderTrails = [&](const std::map<int, std::vector<D2D1_POINT_2F>>& trails) {
        for (auto const& item : trails) {
            int ballId = item.first;
            const std::vector<D2D1_POINT_2F>& points = item.second;

            // Need at least 2 points to draw a path
            if (points.size() < 2) continue;

            // Create a Path Geometry for this ball's trail
            ID2D1PathGeometry* pPathGeo = nullptr;
            hr = pFactory->CreatePathGeometry(&pPathGeo);
            if (SUCCEEDED(hr) && pPathGeo) {
                ID2D1GeometrySink* pSink = nullptr;
                hr = pPathGeo->Open(&pSink);
                if (SUCCEEDED(hr) && pSink) {
                    // Start at the first recorded point
                    pSink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_HOLLOW);

                    // Add ALL subsequent points as a continuous polyline.
                    // This ensures sharp corners at rebounds (the "nooks and crannies").
                    pSink->AddLines(&points[1], (UINT32)(points.size() - 1));

                    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                    pSink->Close();
                    SafeRelease(&pSink);

                    // Set Color: Colored for object balls, White for Cue
                    D2D1_COLOR_F ballColor = GetBallColor(ballId);
                    // Make it slightly transparent so it looks like a chalk mark
                    ballColor.a = (ballId == 0) ? 0.4f : 0.6f;
                    pTrailBrush->SetColor(ballColor);

                    // Draw the smooth geometry
                    pRT->DrawGeometry(pPathGeo, pTrailBrush, 1.5f); // 1.5f thickness
                }
                SafeRelease(&pPathGeo);
            }
        }
    };

    // 1. Draw trails from the PREVIOUS shot (History)
    RenderTrails(g_lastShotTrails);

    // 2. Draw trails for the CURRENT moving shot (Real-Time)
    RenderTrails(g_tempShotTrails);

    SafeRelease(&pTrailBrush);
}

void DrawScene(ID2D1RenderTarget* pRT) {
    if (!pRT) return;

    //pRT->Clear(D2D1::ColorF(D2D1::ColorF::LightGray)); // Background color
    // Set background color to #ffffcd (RGB: 255, 255, 205)
    pRT->Clear(D2D1::ColorF(0.3686f, 0.5333f, 0.3882f)); // Clear with light yellow background NEWCOLOR 1.0f, 1.0f, 0.803f => (0.3686f, 0.5333f, 0.3882f)
    //pRT->Clear(D2D1::ColorF(1.0f, 1.0f, 0.803f)); // Clear with light yellow background NEWCOLOR 1.0f, 1.0f, 0.803f => (0.3686f, 0.5333f, 0.3882f)

    DrawTable(pRT, pFactory);
    // [+] MOVED & UPDATED: Draw trails ON THE FELT, passing pFactory
    DrawBallTrails(pRT, pFactory);
    DrawPocketSelectionIndicator(pRT); // Draw arrow over selected/called pocket
    DrawBalls(pRT);
    // Draw the cue stick right before/after drawing balls:
    DrawCueStick(pRT);
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
    if (!pRT || !pFactory) return;

    // --- Layer 0: Black Pocket Backdrops (Optional, but makes pockets look deeper) ---
    ID2D1SolidColorBrush* pPocketBackdropBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pPocketBackdropBrush);
    if (pPocketBackdropBrush) {
        for (int i = 0; i < 6; ++i) {
            float currentVisualRadius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
            D2D1_ELLIPSE backdrop = D2D1::Ellipse(pocketPositions[i], currentVisualRadius + 5.0f, currentVisualRadius + 5.0f);
            pRT->FillEllipse(&backdrop, pPocketBackdropBrush);
        }
        SafeRelease(&pPocketBackdropBrush);
    }

    // --- Layer 1: Base Felt Surface ---
    ID2D1SolidColorBrush* pFeltBrush = nullptr;
    pRT->CreateSolidColorBrush(TABLE_COLOR, &pFeltBrush);
    if (!pFeltBrush) return;
    D2D1_RECT_F tableRect = D2D1::RectF(TABLE_LEFT, TABLE_TOP, TABLE_RIGHT, TABLE_BOTTOM);
    pRT->FillRectangle(&tableRect, pFeltBrush);
    SafeRelease(&pFeltBrush); // Release now, it's not needed for the rest of the function

    // --- Layer 2: Center Spotlight Effect ---
    // This is your original, preserved spotlight code.
    /* {
        ID2D1RadialGradientBrush* pSpot = nullptr;
        ID2D1GradientStopCollection* pStops = nullptr;
        D2D1_COLOR_F centreClr = D2D1::ColorF(
            std::min(1.f, TABLE_COLOR.r * 1.60f),
            std::min(1.f, TABLE_COLOR.g * 1.60f),
            std::min(1.f, TABLE_COLOR.b * 1.60f));
        const D2D1_GRADIENT_STOP gs[3] =
        {
            { 0.0f, D2D1::ColorF(centreClr.r, centreClr.g, centreClr.b, 0.95f) },
            { 0.8f, D2D1::ColorF(TABLE_COLOR.r, TABLE_COLOR.g, TABLE_COLOR.b, 0.55f) }, // Pushed gradient mid-point out
            { 1.0f, D2D1::ColorF(TABLE_COLOR.r, TABLE_COLOR.g, TABLE_COLOR.b, 0.0f) }
        };
        pRT->CreateGradientStopCollection(gs, 3, &pStops);
        if (pStops)
        {
            D2D1_RECT_F rc = tableRect;
            const float PAD = 18.0f;
            rc.left += PAD;  rc.top += PAD;
            rc.right -= PAD;  rc.bottom -= PAD;
            D2D1_POINT_2F centre = D2D1::Point2F(
                (rc.left + rc.right) / 2.0f,
                (rc.top + rc.bottom) / 2.0f);
            float rx = (rc.right - rc.left) * 0.9f; // Increased spotlight radius
            float ry = (rc.bottom - rc.top) * 0.9f; // Increased spotlight radius
            D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::RadialGradientBrushProperties(
                    centre,
                    D2D1::Point2F(0, 0),
                    rx, ry);
            pRT->CreateRadialGradientBrush(props, pStops, &pSpot);
            pStops->Release();
        }
        if (pSpot)
        {
            const float RADIUS = 20.0f;
            D2D1_ROUNDED_RECT spotlightRR =
                D2D1::RoundedRect(tableRect, RADIUS, RADIUS);
            pRT->FillRoundedRectangle(&spotlightRR, pSpot);
            pSpot->Release();
        }
    }*/
    // --- Layer 2: Center Spotlight Effect (INTENSIFIED) ---
    {
        ID2D1RadialGradientBrush* pSpot = nullptr;
        ID2D1GradientStopCollection* pStops = nullptr;

        // stronger center color multiplier => brighter highlight
        const float CENTER_MULT = 2.20f; // was 1.60f
        D2D1_COLOR_F centreClr = D2D1::ColorF(
            std::min(1.f, TABLE_COLOR.r * CENTER_MULT),
            std::min(1.f, TABLE_COLOR.g * CENTER_MULT),
            std::min(1.f, TABLE_COLOR.b * CENTER_MULT));

        // more intense center alpha, larger bright mid area
        const D2D1_GRADIENT_STOP gs[3] =
        {
            { 0.0f, D2D1::ColorF(centreClr.r, centreClr.g, centreClr.b, 1.00f) }, // stronger center alpha
            { 0.60f, D2D1::ColorF(TABLE_COLOR.r, TABLE_COLOR.g, TABLE_COLOR.b, 0.80f) }, // mid point moved in & brighter
            { 1.0f, D2D1::ColorF(TABLE_COLOR.r, TABLE_COLOR.g, TABLE_COLOR.b, 0.0f) }
        };

        pRT->CreateGradientStopCollection(gs, 3, &pStops);
        if (pStops)
        {
            D2D1_RECT_F rc = tableRect;
            const float PAD = 8.0f; // smaller inset -> spotlight visually larger
            rc.left += PAD;  rc.top += PAD;
            rc.right -= PAD;  rc.bottom -= PAD;

            D2D1_POINT_2F centre = D2D1::Point2F(
                (rc.left + rc.right) * 0.5f,
                (rc.top + rc.bottom) * 0.5f);

            // increase radius slightly to expand overall spot
            float rx = (rc.right - rc.left) * 1.05f; // was 0.9f
            float ry = (rc.bottom - rc.top) * 1.05f; // was 0.9f

            D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::RadialGradientBrushProperties(centre, D2D1::Point2F(0, 0), rx, ry);

            pRT->CreateRadialGradientBrush(props, pStops, &pSpot);
            pStops->Release();
        }

        if (pSpot)
        {
            const float RADIUS = 12.0f; // corner rounding only; lower is fine
            D2D1_ROUNDED_RECT spotlightRR = D2D1::RoundedRect(tableRect, RADIUS, RADIUS);
            pRT->FillRoundedRectangle(&spotlightRR, pSpot);
            pSpot->Release();
        }
    }

    // --- Layer 3: Grainy Felt Texture ---
    // This is your original, preserved grain code.
    ID2D1SolidColorBrush* pGrainBrush = nullptr;
    pRT->CreateSolidColorBrush(TABLE_COLOR, &pGrainBrush);
    if (pGrainBrush)
    {
        const int NUM_GRAINS = 20000;
        for (int i = 0; i < NUM_GRAINS; ++i)
        {
            float x = TABLE_LEFT + (rand() % (int)TABLE_WIDTH);
            float y = TABLE_TOP + (rand() % (int)TABLE_HEIGHT);
            float colorFactor = 0.85f + (rand() % 31) / 100.0f;
            D2D1_COLOR_F grainColor = D2D1::ColorF(
                std::clamp(TABLE_COLOR.r * colorFactor, 0.0f, 1.0f),
                std::clamp(TABLE_COLOR.g * colorFactor, 0.0f, 1.0f),
                std::clamp(TABLE_COLOR.b * colorFactor, 0.0f, 1.0f),
                0.75f
            );
            pGrainBrush->SetColor(grainColor);
            pRT->FillRectangle(D2D1::RectF(x, y, x + 1.0f, y + 1.0f), pGrainBrush);
        }
        SafeRelease(&pGrainBrush);
    }

    // --- Layer 4: Black Pocket Holes ---
    // These are drawn on top of the felt but will be underneath the cushions.
    ID2D1SolidColorBrush* pPocketBrush = nullptr;
    ID2D1SolidColorBrush* pRimBorderBrush = nullptr; // <<< ADD: Brush for the inner gray border
    const float rimBorderAlpha = 0.5f; // <<< DEFINE: Alpha constant here (0.0 to 1.0)
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pPocketBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray, rimBorderAlpha), &pRimBorderBrush); // <<< ADD: Light gray, semi-transparent
    //if (pPocketBrush) {
    if (pPocketBrush && pRimBorderBrush) { // <<< MODIFY: Check both brushes
        for (int i = 0; i < 6; ++i) {
            float radius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
            pRT->FillEllipse(D2D1::Ellipse(pocketPositions[i], radius, radius), pPocketBrush);

            // --- ADD: Draw the light-gray inner border ---
            float borderPadding = 3.5f; // How far inside the black edge the border starts
            float borderThickness = 4.5f; // How thick the gray line is
            float borderInnerRadius = radius - borderPadding - (borderThickness / 2.0f); // Radius for the center of the border line

            if (borderInnerRadius > 0) { // Only draw if the radius is positive
                pRT->DrawEllipse(D2D1::Ellipse(pocketPositions[i], borderInnerRadius, borderInnerRadius), pRimBorderBrush, borderThickness);
            }
            // --- END ADD ---
        }
        SafeRelease(&pRimBorderBrush); // <<< ADD: Release the new brush
        //SafeRelease(&pPocketBrush);
    }
    // --- ADD: Handle case where only one brush was created ---
    else {
        SafeRelease(&pPocketBrush);
        SafeRelease(&pRimBorderBrush);
    }

    // --- FOOLPROOF FIX: Draw Unified Rails and V-Cut Inner Rim ---
// This new block draws both the outer wooden rails and the inner gradient rim
// with the correct 45-degree V-Cut chamfers at the pockets.

    // Part 1: Draw the Outer Wooden Rails (Gradient 3D Look)
    // We'll create a small multi-stop gradient that reads like polished wood.
    {
        // Gradient stops for wood: bright strip (edge highlight) -> mid wood -> dark outer edge
        ID2D1GradientStopCollection* pRailStops = nullptr;
        D2D1_GRADIENT_STOP railStops[3] = {
            { 0.0f, D2D1::ColorF(0.78f, 0.50f, 0.28f, 1.0f) }, // warm highlight
            { 0.5f, D2D1::ColorF(0.45f, 0.25f, 0.13f, 1.0f) }, // mid wood
            { 1.0f, D2D1::ColorF(0.18f, 0.09f, 0.04f, 1.0f) }  // dark edge
        };
        if (SUCCEEDED(pRT->CreateGradientStopCollection(railStops, 3, &pRailStops)) && pRailStops) {
            // Create a vertical gradient brush for top & bottom rails (light at inner edge -> dark at outer edge)
            ID2D1LinearGradientBrush* pRailTopBrush = nullptr;
            ID2D1LinearGradientBrush* pRailBottomBrush = nullptr;
            pRT->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, TABLE_TOP - WOODEN_RAIL_THICKNESS), D2D1::Point2F(0, TABLE_TOP)),
                pRailStops, &pRailTopBrush);
            pRT->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, TABLE_BOTTOM), D2D1::Point2F(0, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS)),
                pRailStops, &pRailBottomBrush);

            // Create a horizontal gradient brush for left & right rails (light at inner edge -> dark at outer edge)
            ID2D1LinearGradientBrush* pRailLeftBrush = nullptr;
            ID2D1LinearGradientBrush* pRailRightBrush = nullptr;
            pRT->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(TABLE_LEFT - WOODEN_RAIL_THICKNESS, 0), D2D1::Point2F(TABLE_LEFT, 0)),
                pRailStops, &pRailLeftBrush);
            pRT->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(TABLE_RIGHT, 0), D2D1::Point2F(TABLE_RIGHT + WOODEN_RAIL_THICKNESS, 0)),
                pRailStops, &pRailRightBrush);

            // Draw top rails (split by middle pocket) using gradient brushes
            if (pRailTopBrush) {
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP - WOODEN_RAIL_THICKNESS, TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP), pRailTopBrush);
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP - WOODEN_RAIL_THICKNESS, TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP), pRailTopBrush);
            }

            // Draw bottom rails
            if (pRailBottomBrush) {
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM, TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS), pRailBottomBrush);
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM, TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS), pRailBottomBrush);
            }

            // Draw side rails
            if (pRailLeftBrush) {
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT - WOODEN_RAIL_THICKNESS, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS, TABLE_LEFT, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS), pRailLeftBrush);
            }
            if (pRailRightBrush) {
                pRT->FillRectangle(D2D1::RectF(TABLE_RIGHT, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS, TABLE_RIGHT + WOODEN_RAIL_THICKNESS, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS), pRailRightBrush);
            }

            // Small top/bottom outer edge highlight lines to sell the 3D look (thin bright strip)
            ID2D1SolidColorBrush* pEdgeHighlight = nullptr;
            pRT->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.78f, 0.60f, 0.25f), &pEdgeHighlight);
            if (pEdgeHighlight) {
                // Draw highlights as two segments (left/right) so they do NOT cross the middle pocket.
                // Top - left
                pRT->FillRectangle(
                    D2D1::RectF(
                        TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS,
                        TABLE_TOP - 4.0f,
                        TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS,
                        TABLE_TOP - 2.0f
                    ),
                    pEdgeHighlight
                );
                // Top - right
                pRT->FillRectangle(
                    D2D1::RectF(
                        TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS,
                        TABLE_TOP - 4.0f,
                        TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS,
                        TABLE_TOP - 2.0f
                    ),
                    pEdgeHighlight
                );
                // Bottom - left
                pRT->FillRectangle(
                    D2D1::RectF(
                        TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS,
                        TABLE_BOTTOM + 2.0f,
                        TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS,
                        TABLE_BOTTOM + 4.0f
                    ),
                    pEdgeHighlight
                );
                // Bottom - right
                pRT->FillRectangle(
                    D2D1::RectF(
                        TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS,
                        TABLE_BOTTOM + 2.0f,
                        TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS,
                        TABLE_BOTTOM + 4.0f
                    ),
                    pEdgeHighlight
                );
                SafeRelease(&pEdgeHighlight);
            }
            /*if (pEdgeHighlight) {
                // Thin strip along inner rail edge (top)
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP - 4.0f, TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP - 2.0f), pEdgeHighlight);
                // Thin strip along inner rail edge (bottom)
                pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM + 2.0f, TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM + 4.0f), pEdgeHighlight);
                SafeRelease(&pEdgeHighlight);
            }*/

            // Release gradient brushes and stops
            SafeRelease(&pRailTopBrush);
            SafeRelease(&pRailBottomBrush);
            SafeRelease(&pRailLeftBrush);
            SafeRelease(&pRailRightBrush);
            SafeRelease(&pRailStops);
        }
    }

    // start gpt fallthrough final
/*// Part 1: Draw the Outer Wooden Rails (Solid Rectangles)
    ID2D1SolidColorBrush* pRailBrush = nullptr;
    pRT->CreateSolidColorBrush(CUSHION_COLOR, &pRailBrush); // Using your existing cushion color
    if (pRailBrush) {
        // Top Rails (split by middle pocket)
        pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP - WOODEN_RAIL_THICKNESS, TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP), pRailBrush);
        pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP - WOODEN_RAIL_THICKNESS, TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP), pRailBrush);
        // Bottom Rails (split by middle pocket)
        pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM, TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS), pRailBrush);
        pRT->FillRectangle(D2D1::RectF(TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM, TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS), pRailBrush);
        // Side Rails
        pRT->FillRectangle(D2D1::RectF(TABLE_LEFT - WOODEN_RAIL_THICKNESS, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS, TABLE_LEFT, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS), pRailBrush);
        pRT->FillRectangle(D2D1::RectF(TABLE_RIGHT, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS, TABLE_RIGHT + WOODEN_RAIL_THICKNESS, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS), pRailBrush);
        SafeRelease(&pRailBrush);
    }*/
    // end gpt fallthrough final

    //newlyomitted-added

                // --- NEW: Silver rivets on wooden rails (avoid pocket mouths) ---
    {
        ID2D1SolidColorBrush* pRivetBrush = nullptr;
        ID2D1SolidColorBrush* pRivetHighlight = nullptr;
        // silver color + subtle rim shading
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.78f, 0.78f, 0.8f, 1.0f), &pRivetBrush);
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f), &pRivetHighlight);

        if (pRivetBrush && pRivetHighlight) {
            const float rivetSpacing = 85.0f;   // distance between rivets along a rail 28.0
            const float rivetRadius = 3.0f;     // rivet radius
            const float avoidMargin = 8.0f;     // extra margin around pockets to avoid placing rivets

            // helper: return true if candidate (x,y) is too close to any pocket
            auto IsNearPocket = [&](float x, float y)->bool {
                for (int pi = 0; pi < 6; ++pi) {
                    float dx = x - pocketPositions[pi].x;
                    float dy = y - pocketPositions[pi].y;
                    float r = GetPocketVisualRadius(pi) + avoidMargin;
                    if (dx * dx + dy * dy <= r * r) return true;
                }
                return false;
            };

            // TOP rails: two segments (left and right of middle pocket)
            float topY = TABLE_TOP - (WOODEN_RAIL_THICKNESS * 0.5f);
            float seg1_start = TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS + rivetSpacing * 0.5f;
            float seg1_end = TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS - rivetSpacing * 0.5f;
            for (float x = seg1_start; x <= seg1_end; x += rivetSpacing) {
                if (!IsNearPocket(x, topY)) {
                    D2D1_ELLIPSE riv = D2D1::Ellipse(D2D1::Point2F(x, topY), rivetRadius, rivetRadius);
                    pRT->FillEllipse(&riv, pRivetBrush);
                    // small highlight
                    D2D1_ELLIPSE h = D2D1::Ellipse(D2D1::Point2F(x - rivetRadius * 0.4f, topY - rivetRadius * 0.4f), rivetRadius * 0.45f, rivetRadius * 0.45f);
                    pRT->FillEllipse(&h, pRivetHighlight);
                }
            }
            float seg2_start = TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS + rivetSpacing * 0.5f;
            float seg2_end = TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS - rivetSpacing * 0.5f;
            for (float x = seg2_start; x <= seg2_end; x += rivetSpacing) {
                if (!IsNearPocket(x, topY)) {
                    D2D1_ELLIPSE riv = D2D1::Ellipse(D2D1::Point2F(x, topY), rivetRadius, rivetRadius);
                    pRT->FillEllipse(&riv, pRivetBrush);
                    D2D1_ELLIPSE h = D2D1::Ellipse(D2D1::Point2F(x - rivetRadius * 0.4f, topY - rivetRadius * 0.4f), rivetRadius * 0.45f, rivetRadius * 0.45f);
                    pRT->FillEllipse(&h, pRivetHighlight);
                }
            }

            // BOTTOM rails
            float botY = TABLE_BOTTOM + (WOODEN_RAIL_THICKNESS * 0.5f);
            seg1_start = TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS + rivetSpacing * 0.5f;
            seg1_end = TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS - rivetSpacing * 0.5f;
            for (float x = seg1_start; x <= seg1_end; x += rivetSpacing) {
                if (!IsNearPocket(x, botY)) {
                    D2D1_ELLIPSE riv = D2D1::Ellipse(D2D1::Point2F(x, botY), rivetRadius, rivetRadius);
                    pRT->FillEllipse(&riv, pRivetBrush);
                    D2D1_ELLIPSE h = D2D1::Ellipse(D2D1::Point2F(x - rivetRadius * 0.4f, botY - rivetRadius * 0.4f), rivetRadius * 0.45f, rivetRadius * 0.45f);
                    pRT->FillEllipse(&h, pRivetHighlight);
                }
            }
            seg2_start = TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS + rivetSpacing * 0.5f;
            seg2_end = TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS - rivetSpacing * 0.5f;
            for (float x = seg2_start; x <= seg2_end; x += rivetSpacing) {
                if (!IsNearPocket(x, botY)) {
                    D2D1_ELLIPSE riv = D2D1::Ellipse(D2D1::Point2F(x, botY), rivetRadius, rivetRadius);
                    pRT->FillEllipse(&riv, pRivetBrush);
                    D2D1_ELLIPSE h = D2D1::Ellipse(D2D1::Point2F(x - rivetRadius * 0.4f, botY - rivetRadius * 0.4f), rivetRadius * 0.45f, rivetRadius * 0.45f);
                    pRT->FillEllipse(&h, pRivetHighlight);
                }
            }

            // LEFT rail (vertical)
            float leftX = TABLE_LEFT - (WOODEN_RAIL_THICKNESS * 0.5f);
            float leftStartY = TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS + rivetSpacing * 0.5f;
            float leftEndY = TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS - rivetSpacing * 0.5f;
            for (float y = leftStartY; y <= leftEndY; y += rivetSpacing) {
                if (!IsNearPocket(leftX, y)) {
                    D2D1_ELLIPSE riv = D2D1::Ellipse(D2D1::Point2F(leftX, y), rivetRadius, rivetRadius);
                    pRT->FillEllipse(&riv, pRivetBrush);
                    D2D1_ELLIPSE h = D2D1::Ellipse(D2D1::Point2F(leftX - rivetRadius * 0.4f, y - rivetRadius * 0.4f), rivetRadius * 0.45f, rivetRadius * 0.45f);
                    pRT->FillEllipse(&h, pRivetHighlight);
                }
            }

            // RIGHT rail (vertical)
            float rightX = TABLE_RIGHT + (WOODEN_RAIL_THICKNESS * 0.5f);
            float rightStartY = TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS + rivetSpacing * 0.5f;
            float rightEndY = TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS - rivetSpacing * 0.5f;
            for (float y = rightStartY; y <= rightEndY; y += rivetSpacing) {
                if (!IsNearPocket(rightX, y)) {
                    D2D1_ELLIPSE riv = D2D1::Ellipse(D2D1::Point2F(rightX, y), rivetRadius, rivetRadius);
                    pRT->FillEllipse(&riv, pRivetBrush);
                    D2D1_ELLIPSE h = D2D1::Ellipse(D2D1::Point2F(rightX - rivetRadius * 0.4f, y - rivetRadius * 0.4f), rivetRadius * 0.45f, rivetRadius * 0.45f);
                    pRT->FillEllipse(&h, pRivetHighlight);
                }
            }
            //}

            SafeRelease(&pRivetBrush);
            SafeRelease(&pRivetHighlight);
        }
    }
    //}

    // Part 2: Draw the Inner Gradient Rim as 6 Polygons with Correct V-Cuts
    //const float rimWidth = 20.0f;       // Defines the thickness of the inner rim overlay 15=>20
    //const float chamferSize = 25.0f;    // Defines the size of the 45-degree V-cut

    // Create a single Gradient Stop Collection to be reused for the 3D effect
    ID2D1GradientStopCollection* pStops = nullptr;
    D2D1_GRADIENT_STOP stops[2] = { {0.0f, Lighten(TABLE_COLOR, 1.5f)}, {1.0f, Lighten(TABLE_COLOR, 0.7f)} };
    pRT->CreateGradientStopCollection(stops, 2, &pStops);

    if (pStops) {
        // --- [+] DEFINE RIM POLYGONS AGAIN HERE (for the actual rims) ---
// This is a copy of the array used for the shadows.
        D2D1_POINT_2F rim_pieces[6][4] = {
            // Top Rail (Left Part) - CORRECT
            { {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS}, {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS} },
            // Top Rail (Right Part) - CORRECT
            { {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS}, {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS} },
            // Bottom Rail (Left Part) - CORRECT
            { {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM}, {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM} },
            // Bottom Rail (Right Part) - CORRECT
            { {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM}, {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM} },
            // Left Rail - CORRECT
            { {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE}, {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE}, {TABLE_LEFT, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS}, {TABLE_LEFT, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS} },
            // Right Rail - RECALCULATED AND CORRECTED
            { {TABLE_RIGHT, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS}, {TABLE_RIGHT, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS}, {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE}, {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE} }
        };
        // Define the vertices for all 6 rim/cushion polygons with CORRECTED outward-facing cuts
        // OUTWARD-facing chamfer tips: short edge is OUTSIDE (at wood), long edge is INSIDE (at felt)
                // --- FOOLPROOF FIX: Recalculated vertices for correct, non-bloated, outward-facing V-cuts ---
                // --- FOOLPROOF FIX: Recalculated vertices for REVERSED, non-bloated, outward-facing V-cuts ---
        // This version places the short edge against the rail and the long edge facing the felt.
                // --- FINAL FOOLPROOF FIX: Corrected vertices for ALL rails, including Left and Right ---
                // --- FINAL, DEFINITIVE FIX: Corrected vertices for the Right Rail ---
        /* D2D1_POINT_2F rim_pieces[6][4] = { //newlyomitted
            // Top Rail (Left Part) - CORRECT
            { {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS}, {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS} },
            // Top Rail (Right Part) - CORRECT
            { {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_TOP}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS}, {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_TOP + INNER_RIM_THICKNESS} },
            // Bottom Rail (Left Part) - CORRECT
            { {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_LEFT + TABLE_WIDTH / 2.f - MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM}, {TABLE_LEFT + CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM} },
            // Bottom Rail (Right Part) - CORRECT
            { {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS + CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE, TABLE_BOTTOM - INNER_RIM_THICKNESS}, {TABLE_RIGHT - CORNER_HOLE_VISUAL_RADIUS, TABLE_BOTTOM}, {TABLE_LEFT + TABLE_WIDTH / 2.f + MIDDLE_HOLE_VISUAL_RADIUS, TABLE_BOTTOM} },
            // Left Rail - CORRECT
            { {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE}, {TABLE_LEFT + INNER_RIM_THICKNESS, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE}, {TABLE_LEFT, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS}, {TABLE_LEFT, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS} },
            // Right Rail - RECALCULATED AND CORRECTED
            { {TABLE_RIGHT, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS}, {TABLE_RIGHT, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS}, {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_BOTTOM - CORNER_HOLE_VISUAL_RADIUS - CHAMFER_SIZE}, {TABLE_RIGHT - INNER_RIM_THICKNESS, TABLE_TOP + CORNER_HOLE_VISUAL_RADIUS + CHAMFER_SIZE} }
        };*/

        // --- Layer 5: Inner Rim Dropshadow (CENTROID → TABLE CENTER, single pass) ---
        // --- Uniform Outward Dropshadow for Inner Rims ---

        // --- Layer: Inner Rim Dropshadow (Uniform Outward Shadow for All Rims) ---
        /* {
            ID2D1SolidColorBrush* pShadowBrush = nullptr;
            pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.18f), &pShadowBrush); // Slightly stronger alpha for visibility
            if (pShadowBrush) {
                const float shadowOffset = 5.0f;   // How far to offset the shadow outward
                const float featherOffset = 10.0f; // How far to feather the shadow
                const float featherAlpha = 0.08f;  // Alpha for feathered shadow

                for (int i = 0; i < 6; ++i) {
                    // For each rim piece (4-point polygon)
                    D2D1_POINT_2F* poly = rim_pieces[i];

                    // Compute the outward normal for this rim piece (average of edge normals)
                    float nx = 0, ny = 0;
                    for (int j = 0; j < 4; ++j) {
                        int k = (j + 1) % 4;
                        float dx = poly[k].x - poly[j].x;
                        float dy = poly[k].y - poly[j].y;
                        // Outward normal (perpendicular, right-hand rule)
                        nx += dy;
                        ny -= dx;
                    }
                    float len = sqrtf(nx * nx + ny * ny);
                    if (len < 1e-6f) { nx = 0; ny = 1; len = 1; }
                    nx /= len; ny /= len;

                    // Offset all points outward for shadow
                    D2D1_POINT_2F shadowPoly[4];
                    for (int j = 0; j < 4; ++j) {
                        shadowPoly[j].x = poly[j].x + nx * shadowOffset;
                        shadowPoly[j].y = poly[j].y + ny * shadowOffset;
                    }

                    // Draw the shadow polygon
                    ID2D1PathGeometry* pShadowGeom = nullptr;
                    pFactory->CreatePathGeometry(&pShadowGeom);
                    if (pShadowGeom) {
                        ID2D1GeometrySink* sink = nullptr;
                        if (SUCCEEDED(pShadowGeom->Open(&sink))) {
                            sink->BeginFigure(shadowPoly[0], D2D1_FIGURE_BEGIN_FILLED);
                            sink->AddLines(&shadowPoly[1], 3);
                            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                            sink->Close();
                        }
                        SafeRelease(&sink);
                        pRT->FillGeometry(pShadowGeom, pShadowBrush);
                        SafeRelease(&pShadowGeom);
                    }

                    // Feathered shadow (softer, further out, lighter alpha)
                    D2D1_POINT_2F featherPoly[4];
                    for (int j = 0; j < 4; ++j) {
                        featherPoly[j].x = poly[j].x + nx * featherOffset;
                        featherPoly[j].y = poly[j].y + ny * featherOffset;
                    }
                    ID2D1SolidColorBrush* pFeatherBrush = nullptr;
                    pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, featherAlpha), &pFeatherBrush);
                    if (pFeatherBrush) {
                        ID2D1PathGeometry* pFeatherGeom = nullptr;
                        pFactory->CreatePathGeometry(&pFeatherGeom);
                        if (pFeatherGeom) {
                            ID2D1GeometrySink* sink = nullptr;
                            if (SUCCEEDED(pFeatherGeom->Open(&sink))) {
                                sink->BeginFigure(featherPoly[0], D2D1_FIGURE_BEGIN_FILLED);
                                sink->AddLines(&featherPoly[1], 3);
                                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                                sink->Close();
                            }
                            SafeRelease(&sink);
                            pRT->FillGeometry(pFeatherGeom, pFeatherBrush);
                            SafeRelease(&pFeatherGeom);
                        }
                        SafeRelease(&pFeatherBrush);
                    }
                }
                SafeRelease(&pShadowBrush);
            }
        }*/
        // --- Layer 5: Inner Rim Dropshadow (soft, chamfered, inward only) ---
        // 3-layer hand-painted soft shadow, pure vector, no bitmap, no blur effect
        {
            ID2D1SolidColorBrush* pShadowBrush = nullptr;
            pRT->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), &pShadowBrush);

            if (pShadowBrush)
            {
                // Hard-coded inward directions (unit vectors pointing toward table center)
                const D2D1_POINT_2F inwardDir[6] = {
                    { 0.0f,  1.0f },  // top-left rail
                    { 0.0f,  1.0f },  // top-right rail
                    { 0.0f, -1.0f },  // bottom-left rail
                    { 0.0f, -1.0f },  // bottom-right rail
                    { 1.0f,  0.0f },  // left rail
                    {-1.0f,  0.0f }   // right rail
                };

                // 3-layer soft shadow — farther = lighter & larger offset
                const float offsets[3] = { 10.0f, 6.0f, 3.0f };   // far → near
                const float alphas[3] = { 0.11f, 0.22f, 0.38f };  // far → near (not overly dark)

                for (int layer = 0; layer < 3; ++layer)               // draw far layer first
                {
                    float offset = offsets[layer];
                    float alpha = alphas[layer];
                    pShadowBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha));

                    for (int i = 0; i < 6; ++i)
                    {
                        D2D1_POINT_2F* base = rim_pieces[i];
                        float dx = inwardDir[i].x * offset;
                        float dy = inwardDir[i].y * offset;

                        D2D1_POINT_2F shadowPoly[4] = {
                            { base[0].x + dx, base[0].y + dy },
                            { base[1].x + dx, base[1].y + dy },
                            { base[2].x + dx, base[2].y + dy },
                            { base[3].x + dx, base[3].y + dy }
                        };

                        ID2D1PathGeometry* pGeom = nullptr;
                        if (SUCCEEDED(pFactory->CreatePathGeometry(&pGeom)) && pGeom)
                        {
                            ID2D1GeometrySink* pSink = nullptr;
                            if (SUCCEEDED(pGeom->Open(&pSink)) && pSink)
                            {
                                pSink->BeginFigure(shadowPoly[0], D2D1_FIGURE_BEGIN_FILLED);
                                pSink->AddLines(&shadowPoly[1], 3);
                                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                                pSink->Close();
                            }
                            SafeRelease(&pSink);
                            pRT->FillGeometry(pGeom, pShadowBrush);
                            SafeRelease(&pGeom);
                        }
                    }
                }
                SafeRelease(&pShadowBrush);
            }
        }


        // Draw the actual rim geometry as before, on top of the shadow


        // Create a unique gradient brush for each of the 4 orientations
        ID2D1LinearGradientBrush* pTopBrush = nullptr, * pBottomBrush = nullptr, * pLeftBrush = nullptr, * pRightBrush = nullptr;
        pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ 0, TABLE_TOP }, { 0, TABLE_TOP + INNER_RIM_THICKNESS }), pStops, &pTopBrush);
        pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ 0, TABLE_BOTTOM - INNER_RIM_THICKNESS }, { 0, TABLE_BOTTOM }), pStops, &pBottomBrush);
        pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ TABLE_LEFT, 0 }, { TABLE_LEFT + INNER_RIM_THICKNESS, 0 }), pStops, &pLeftBrush);
        pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ TABLE_RIGHT - INNER_RIM_THICKNESS, 0 }, { TABLE_RIGHT, 0 }), pStops, &pRightBrush);

        ID2D1LinearGradientBrush* brushes[] = { pTopBrush, pTopBrush, pBottomBrush, pBottomBrush, pLeftBrush, pRightBrush };

        // Draw each rim piece as a filled polygon with its corresponding gradient
        for (int i = 0; i < 6; ++i) {
            if (!brushes[i]) continue;
            ID2D1PathGeometry* pPath = nullptr;
            pFactory->CreatePathGeometry(&pPath);
            if (pPath) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pPath->Open(&pSink))) {
                    pSink->BeginFigure(rim_pieces[i][0], D2D1_FIGURE_BEGIN_FILLED);
                    pSink->AddLines(&rim_pieces[i][1], 3);
                    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    pSink->Close();
                }
                SafeRelease(&pSink);
                pRT->FillGeometry(pPath, brushes[i]);
                SafeRelease(&pPath);
            }
        }

        // --- [+] FIX: Create a new, dedicated SILVER gradient for liners ---
        ID2D1GradientStopCollection* pSilverStops = nullptr;
        D2D1_GRADIENT_STOP silverStops[3] = {
            { 0.0f, D2D1::ColorF(D2D1::ColorF::White, 0.8f) },
            { 0.5f, D2D1::ColorF(D2D1::ColorF::Silver, 0.9f) },
            { 1.0f, D2D1::ColorF(D2D1::ColorF::Gray, 0.8f) }
        };
        pRT->CreateGradientStopCollection(silverStops, 3, &pSilverStops);

        ID2D1LinearGradientBrush* pSilverTopBrush = nullptr, * pSilverBottomBrush = nullptr, * pSilverLeftBrush = nullptr, * pSilverRightBrush = nullptr;
        if (pSilverStops) {
            pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ 0, TABLE_TOP - 5.0f }, { 0, TABLE_TOP + 5.0f }), pSilverStops, &pSilverTopBrush);
            pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ 0, TABLE_BOTTOM + 5.0f }, { 0, TABLE_BOTTOM - 5.0f }), pSilverStops, &pSilverBottomBrush);
            pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ TABLE_LEFT - 5.0f, 0 }, { TABLE_LEFT + 5.0f, 0 }), pSilverStops, &pSilverLeftBrush);
            pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ TABLE_RIGHT + 5.0f, 0 }, { TABLE_RIGHT - 5.0f, 0 }), pSilverStops, &pSilverRightBrush);
        }

        // --- Layer 5.5: Silver Pocket Liners ---
// [+] Draw silver gradient borders around the pocket openings, using the same rim brushes for consistency.
        const float silverBorderThickness = 3.0f;
        for (int i = 0; i < 6; ++i) {
            float radius = (i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;
            // Draw the ellipse slightly larger than the black hole
            float drawRadius = radius + silverBorderThickness / 2.0f;
            D2D1_ELLIPSE silverEllipse = D2D1::Ellipse(pocketPositions[i], drawRadius, drawRadius);

            // Pick the appropriate gradient brush based on the pocket's location
            ID2D1Brush* pSilverBrush = nullptr;
            // [+] FIX: Use the new SILVER brushes
            if (i == 1) pSilverBrush = pSilverTopBrush; // Top-Middle
            else if (i == 4) pSilverBrush = pSilverBottomBrush; // Bottom-Middle
            else if (i == 0 || i == 3) pSilverBrush = pSilverLeftBrush; // Left-Side
            else pSilverBrush = pSilverRightBrush; // Right-Side

            if (pSilverBrush) {
                pRT->DrawEllipse(silverEllipse, pSilverBrush, silverBorderThickness);
            }
        }
        // --- End Silver Pocket Liners ---

        // FIX: Release all the new silver brushes and stops
        SafeRelease(&pSilverTopBrush);
        SafeRelease(&pSilverBottomBrush);
        SafeRelease(&pSilverLeftBrush);
        SafeRelease(&pSilverRightBrush);
        SafeRelease(&pSilverStops);

        SafeRelease(&pTopBrush); SafeRelease(&pBottomBrush); SafeRelease(&pLeftBrush); SafeRelease(&pRightBrush);
        SafeRelease(&pStops);
    }

    // --- Layer 6: Headstring and Kitchen Arc ---
    // This is drawn on top of the felt and cushions.
    ID2D1SolidColorBrush* pLineBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.4235f, 0.5647f, 0.1765f, 1.0f), &pLineBrush);
    if (pLineBrush) {
        pRT->DrawLine(
            D2D1::Point2F(HEADSTRING_X, TABLE_TOP),
            D2D1::Point2F(HEADSTRING_X, TABLE_BOTTOM),
            pLineBrush,
            1.0f
        );

        // This is your original, preserved kitchen semicircle code.
        ID2D1PathGeometry* pGeometry = nullptr;
        HRESULT hr = pFactory->CreatePathGeometry(&pGeometry);
        if (SUCCEEDED(hr) && pGeometry)
        {
            ID2D1GeometrySink* pSink = nullptr;
            hr = pGeometry->Open(&pSink);
            if (SUCCEEDED(hr) && pSink)
            {
                float radius = 60.0f;
                D2D1_POINT_2F center = D2D1::Point2F(HEADSTRING_X, (TABLE_TOP + TABLE_BOTTOM) / 2.0f);
                D2D1_POINT_2F startPoint = D2D1::Point2F(center.x, center.y - radius);
                pSink->BeginFigure(startPoint, D2D1_FIGURE_BEGIN_HOLLOW);
                D2D1_ARC_SEGMENT arc = {};
                arc.point = D2D1::Point2F(center.x, center.y + radius);
                arc.size = D2D1::SizeF(radius, radius);
                arc.rotationAngle = 0.0f;
                arc.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                arc.arcSize = D2D1_ARC_SIZE_SMALL;
                pSink->AddArc(&arc);
                pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                pSink->Close();
                SafeRelease(&pSink);

                pRT->DrawGeometry(pGeometry, pLineBrush, 1.5f);
            }
            SafeRelease(&pGeometry);
        }
        SafeRelease(&pLineBrush);
    }
}


// ----------------------------------------------
//  Helper : clamp to [0,1] and lighten a colour
// ----------------------------------------------
static D2D1_COLOR_F Lighten(const D2D1_COLOR_F& c, float factor = 1.25f)
{
    return D2D1::ColorF(
        std::min(1.0f, c.r * factor),
        std::min(1.0f, c.g * factor),
        std::min(1.0f, c.b * factor),
        c.a);
}

// ------------------------------------------------
//  NEW  DrawBalls – radial-gradient “spot-light”
// ------------------------------------------------
void DrawBalls(ID2D1RenderTarget* pRT)
{
    if (!pRT) return;

    // Create a single shadow brush outside the loop for performance
    ID2D1SolidColorBrush* pShadowBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.3f), &pShadowBrush); //0.35->0.7->0.4->0.3

    ID2D1SolidColorBrush* pStripeBrush = nullptr;    // white stripe
    ID2D1SolidColorBrush* pBorderBrush = nullptr;    // black ring
    ID2D1SolidColorBrush* pNumWhite = nullptr;       // white decal circle
    ID2D1SolidColorBrush* pNumBlack = nullptr;       // digit colour

    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pStripeBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pNumWhite);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pNumBlack);

    for (const Ball& b : balls)
    {
        if (b.isPocketed) continue;

        // --- ADD: Draw Ball Dropshadow ---
        // This is drawn *before* the ball and its rotation transform
        // so the shadow stays offset in world-space.
        /*if (pShadowBrush) {
            const float shadowOffsetX = 2.5f;
            const float shadowOffsetY = 3.5f;
            D2D1_ELLIPSE shadowEllipse = D2D1::Ellipse(
                D2D1::Point2F(b.x + shadowOffsetX, b.y + shadowOffsetY),
                BALL_RADIUS, BALL_RADIUS
            );
            pRT->FillEllipse(&shadowEllipse, pShadowBrush);
        }*/
        /*if (pShadowBrush) {
            // tweak these two values to control shadow spacing & softness visually
            const float shadowScale = 1.4f;         // <-- set x2 to double spread 2.0->1.3->1.4
            const float shadowOffsetX = 1.0f;// * shadowScale;    // moved a bit further 2.5->1.0
            const float shadowOffsetY = 1.0f;// * shadowScale;    // 3.5->1.0

            // enlarge ellipse radii by same scale -> bigger, more spread shadow
            D2D1_ELLIPSE shadowEllipse = D2D1::Ellipse(
                D2D1::Point2F(b.x + shadowOffsetX, b.y + shadowOffsetY),
                BALL_RADIUS * shadowScale, BALL_RADIUS * shadowScale
            );
            pRT->FillEllipse(&shadowEllipse, pShadowBrush);
        }*/
        {
            const float shadowScale = 2.0f;
            const float offsetX = 2.5f * shadowScale;
            const float offsetY = 3.5f * shadowScale;
            const float shadowRadius = BALL_RADIUS * shadowScale * 1.25f;

            D2D1_GRADIENT_STOP sgs[3];
            sgs[0].position = 0.0f;  sgs[0].color = D2D1::ColorF(0, 0, 0, 0.40f);
            sgs[1].position = 0.6f;  sgs[1].color = D2D1::ColorF(0, 0, 0, 0.20f);
            sgs[2].position = 1.0f;  sgs[2].color = D2D1::ColorF(0, 0, 0, 0.0f);

            ID2D1GradientStopCollection* pShadowStops = nullptr;
            ID2D1RadialGradientBrush* pShadowRad = nullptr;

            if (SUCCEEDED(pRT->CreateGradientStopCollection(sgs, 3, &pShadowStops)))
            {
                D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                    D2D1::RadialGradientBrushProperties(
                        D2D1::Point2F(offsetX, offsetY),   // center in local brush coords
                        D2D1::Point2F(0, 0),
                        shadowRadius,
                        shadowRadius);

                if (SUCCEEDED(pRT->CreateRadialGradientBrush(props, pShadowStops, &pShadowRad)))
                {
                    // IMPORTANT: move the gradient to the ellipse position in world space:
                    pShadowRad->SetTransform(
                        D2D1::Matrix3x2F::Translation(b.x - offsetX, b.y - offsetY)
                    );

                    D2D1_ELLIPSE shadowEllipse =
                        D2D1::Ellipse(D2D1::Point2F(b.x + offsetX, b.y + offsetY),
                            shadowRadius, shadowRadius);

                    pRT->FillEllipse(&shadowEllipse, pShadowRad);
                }
            }

            SafeRelease(&pShadowStops);
            SafeRelease(&pShadowRad);
        }


        // --- END ADD ---

        // Save the original transform for restoration at the end of this ball.
        D2D1::Matrix3x2F originalTransform;
        pRT->GetTransform(&originalTransform);

        // Build the radial gradient for THIS ball
        ID2D1GradientStopCollection* pStops = nullptr;
        ID2D1RadialGradientBrush* pRad = nullptr;

        D2D1_GRADIENT_STOP gs[3];
        gs[0].position = 0.0f;  gs[0].color = D2D1::ColorF(1, 1, 1, 0.95f);   // bright spot
        gs[1].position = 0.35f; gs[1].color = Lighten(b.color);               // transitional
        gs[2].position = 1.0f;  gs[2].color = b.color;                        // base colour

        pRT->CreateGradientStopCollection(gs, 3, &pStops);
        if (pStops)
        {
            // Place the hot-spot slightly towards top-left to look more 3-D,
            // then nudge it along the rotationAxis so the highlight follows travel.
            // This gives the illusion of 'spinning on the side'.
            float baseHotX = b.x - BALL_RADIUS * 0.4f;
            float baseHotY = b.y - BALL_RADIUS * 0.4f;

            // small dynamic offset based on rotationAxis and rotationAngle magnitude
            float axis = b.rotationAxis; // radians, perpendicular to travel
            float spinInfluence = 0.14f; // tweak: how much spin moves the highlight (0..0.4 typical)
            float offsetMag = (fmodf(fabsf(b.rotationAngle), 2.0f * PI) / (2.0f * PI)) * (BALL_RADIUS * spinInfluence);
            D2D1_POINT_2F origin = D2D1::Point2F(
                baseHotX + cosf(axis) * offsetMag,
                baseHotY + sinf(axis) * offsetMag
            );

            D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::RadialGradientBrushProperties(
                    origin,
                    D2D1::Point2F(0, 0),
                    BALL_RADIUS * 1.3f,
                    BALL_RADIUS * 1.3f);

            /*// Place the hotspot slightly towards top-left (in world coords)
            D2D1_POINT_2F origin = D2D1::Point2F(b.x - BALL_RADIUS * 0.4f,
                b.y - BALL_RADIUS * 0.4f);
            D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::RadialGradientBrushProperties(
                    origin,                        // gradientOrigin
                    D2D1::Point2F(0, 0),           // offset (unused)
                    BALL_RADIUS * 1.3f,            // radiusX
                    BALL_RADIUS * 1.3f);           // radiusY*/

            pRT->CreateRadialGradientBrush(props, pStops, &pRad);
            SafeRelease(&pStops); // safe to release stops after brush creation
        }

        // Compute rotation transform (convert radians -> degrees)
        float angleDeg = (b.rotationAngle * 180.0f) / PI;
        D2D1::Matrix3x2F rot = D2D1::Matrix3x2F::Rotation(angleDeg, D2D1::Point2F(b.x, b.y));
        // Apply rotation on top of existing transform
        pRT->SetTransform(rot * originalTransform);

        // Draw the ball (body)
        D2D1_ELLIPSE body = D2D1::Ellipse(D2D1::Point2F(b.x, b.y), BALL_RADIUS, BALL_RADIUS);
        if (pRad) pRT->FillEllipse(&body, pRad);

        // Stripes overlay (if applicable)
        if (b.type == BallType::STRIPE && pStripeBrush)
        {
            D2D1_RECT_F stripe = D2D1::RectF(
                b.x - BALL_RADIUS,
                b.y - BALL_RADIUS * 0.40f,
                b.x + BALL_RADIUS,
                b.y + BALL_RADIUS * 0.40f);
            pRT->FillRectangle(&stripe, pStripeBrush);

            if (pRad)
            {
                D2D1_ELLIPSE inner = D2D1::Ellipse(D2D1::Point2F(b.x, b.y),
                    BALL_RADIUS * 0.60f, BALL_RADIUS * 0.60f);
                pRT->FillEllipse(&inner, pRad);
            }
        }

        // Draw number decal (skip cue ball)
        if (b.id != 0 && pBallNumFormat && pNumWhite && pNumBlack)
        {
            const float decalR = (b.type == BallType::STRIPE) ? BALL_RADIUS * 0.40f : BALL_RADIUS * 0.45f;
            D2D1_ELLIPSE decal = D2D1::Ellipse(D2D1::Point2F(b.x, b.y), decalR, decalR);
            pRT->FillEllipse(&decal, pNumWhite);
            pRT->DrawEllipse(&decal, pNumBlack, 0.8f);

            // Use std::wstring to avoid _snwprintf_s/wcslen and literal issues.
            std::wstring numText = std::to_wstring(b.id);
            D2D1_RECT_F layout = D2D1::RectF(
                b.x - decalR, b.y - decalR,
                b.x + decalR, b.y + decalR);

            pRT->DrawTextW(
                numText.c_str(),
                static_cast<UINT32>(numText.length()),
                pBallNumFormat,
                &layout,
                pNumBlack);
        }

        // Draw black border for the ball
        if (pBorderBrush)
            pRT->DrawEllipse(&body, pBorderBrush, 1.5f);

        // restore the render target transform for next objects
        pRT->SetTransform(originalTransform);

        // cleanup per-ball D2D resources
        SafeRelease(&pRad);
    } // end for each ball

    SafeRelease(&pStripeBrush);
    SafeRelease(&pBorderBrush);
    SafeRelease(&pNumWhite);
    SafeRelease(&pNumBlack);
}

/*void DrawBalls(ID2D1RenderTarget* pRT) {
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
}*/

void DrawCueStick(ID2D1RenderTarget* pRT)
{
    // --- Logic to determine if the cue stick should be visible ---
    // This part of your code is correct and is preserved.
    bool shouldDrawStick = false;
    if (aiIsDisplayingAim) {
        shouldDrawStick = true;
    }
    else if (currentPlayer == 1 || !isPlayer2AI) {
        switch (currentGameState) {
        case AIMING:
        case BREAKING:
        case PLAYER1_TURN:
        case PLAYER2_TURN:
        case CHOOSING_POCKET_P1:
        case CHOOSING_POCKET_P2:
            shouldDrawStick = true;
            break;
        }
    }
    if (!shouldDrawStick) return;
    // --- End visibility logic ---

    Ball* cue = GetCueBall();
    if (!cue) return;

    // --- FIX: Increased dimensions and added retraction logic ---

    // --- FOOLPROOF FIX: Scaled dimensions for a 150% larger cue stick ---

    // 1. Define the new, proportionally larger dimensions for the cue stick.
    const float scale = 1.5f; // 150%
    const float tipLength = 30.0f * scale;
    const float buttLength = 60.0f * scale;
    const float totalLength = 260.0f * scale; //220.0f
    const float buttWidth = 9.0f * scale;
    const float shaftWidth = 5.0f * scale;

    /*// 1. Define the new, larger dimensions for the cue stick.
    const float tipLength = 30.0f;
    const float buttLength = 60.0f;      // Increased from 50.0f
    const float totalLength = 220.0f;    // Increased from 200.0f
    const float buttWidth = 9.0f;        // Increased from 8.0f
    const float shaftWidth = 5.0f;       // Increased from 4.0f
    */

    // 2. Determine the correct angle and power to draw with.
    float angleToDraw = cueAngle;
    float powerToDraw = shotPower;
    if (aiIsDisplayingAim) { // If AI is showing its aim, use its planned shot.
        angleToDraw = aiPlannedShotDetails.angle;
        powerToDraw = aiPlannedShotDetails.power;
    }

    // 3. Calculate the "power offset" based on the current shot strength.
    // This is the logic from your old code that makes the stick pull back.
    float powerOffset = 0.0f;
    if ((isAiming || isDraggingStick) || aiIsDisplayingAim) {
        // The multiplier controls how far the stick pulls back.
        powerOffset = powerToDraw * 5.0f;
    }

    // 4. Calculate the start and end points of the cue stick, applying the offset.
    float theta = angleToDraw + PI;
    D2D1_POINT_2F base = D2D1::Point2F(
        cue->x + cosf(theta) * (buttLength + powerOffset),
        cue->y + sinf(theta) * (buttLength + powerOffset)
    );
    D2D1_POINT_2F tip = D2D1::Point2F(
        cue->x + cosf(theta) * (totalLength + powerOffset),
        cue->y + sinf(theta) * (totalLength + powerOffset)
    );
    // --- END OF FIX ---

    // --- The rest of your tapered drawing logic is preserved and will now use the new points ---
    ID2D1SolidColorBrush* pButtBrush = nullptr;
    ID2D1SolidColorBrush* pShaftBrush = nullptr;
    ID2D1SolidColorBrush* pTipBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &pButtBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.85f, 0.60f), &pShaftBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &pTipBrush);

    auto buildRect = [&](D2D1_POINT_2F p1, D2D1_POINT_2F p2, float w1, float w2) {
        D2D1_POINT_2F perp = { -sinf(theta) * 0.5f, cosf(theta) * 0.5f };
        perp.x *= w1; perp.y *= w1;
        D2D1_POINT_2F a = { p1.x + perp.x, p1.y + perp.y };
        D2D1_POINT_2F b = { p2.x + perp.x * (w2 / w1), p2.y + perp.y * (w2 / w1) };
        D2D1_POINT_2F c = { p2.x - perp.x * (w2 / w1), p2.y - perp.y * (w2 / w1) };
        D2D1_POINT_2F d = { p1.x - perp.x, p1.y - perp.y };
        ID2D1PathGeometry* geom = nullptr;
        if (SUCCEEDED(pFactory->CreatePathGeometry(&geom))) {
            ID2D1GeometrySink* sink = nullptr;
            if (SUCCEEDED(geom->Open(&sink))) {
                sink->BeginFigure(a, D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(b);
                sink->AddLine(c);
                sink->AddLine(d);
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
            }
            SafeRelease(&sink);
        }
        return geom;
    };

    // --- FIX: These points are now also offset by the power ---
    D2D1_POINT_2F mid1 = { cue->x + cosf(theta) * (buttLength * 0.5f + powerOffset), cue->y + sinf(theta) * (buttLength * 0.5f + powerOffset) };
    D2D1_POINT_2F mid2 = { cue->x + cosf(theta) * (totalLength - tipLength + powerOffset), cue->y + sinf(theta) * (totalLength - tipLength + powerOffset) };
    // --- END OF FIX ---

    auto buttGeom = buildRect(base, mid1, buttWidth, shaftWidth);
    if (buttGeom) { pRT->FillGeometry(buttGeom, pButtBrush); SafeRelease(&buttGeom); }

    auto shaftGeom = buildRect(mid1, mid2, shaftWidth, shaftWidth);
    if (shaftGeom) { pRT->FillGeometry(shaftGeom, pShaftBrush); SafeRelease(&shaftGeom); }

    auto tipGeom = buildRect(mid2, tip, shaftWidth, shaftWidth);
    if (tipGeom) { pRT->FillGeometry(tipGeom, pTipBrush); SafeRelease(&tipGeom); }

    SafeRelease(&pButtBrush);
    SafeRelease(&pShaftBrush);
    SafeRelease(&pTipBrush);
}



/*void DrawAimingAids(ID2D1RenderTarget* pRT) {
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
    D2D1_POINT_2F rayEnd = D2D1::Point2F(rayStart.x + cosA * rayLength, rayStart.y + sinA * rayLength);*/

void DrawAimingAids(ID2D1RenderTarget* pRT) {
    // Determine if aiming aids should be drawn.
    bool isHumanInteracting = (!isPlayer2AI || currentPlayer == 1) &&
        (currentGameState == PLAYER1_TURN || currentGameState == PLAYER2_TURN ||
            currentGameState == BREAKING || currentGameState == AIMING ||
            currentGameState == CHOOSING_POCKET_P1 || currentGameState == CHOOSING_POCKET_P2);

    // FOOLPROOF FIX: This is the new condition to show the AI's aim.
    bool isAiVisualizingShot = (isPlayer2AI && currentPlayer == 2 && aiIsDisplayingAim);

    // --- MODIFIED: Allow drawing if Debug Mode is ON, even if not strictly "aiming" state ---
    if (!isHumanInteracting && !isAiVisualizingShot && !g_debugMode) {
        return;
    }
    // ---------------------------------------------------------------------------------------

    Ball* cueBall = GetCueBall();
    if (!cueBall || cueBall->isPocketed) return;

    // --- Brush and Style Creation (No changes here) ---
    ID2D1SolidColorBrush* pBrush = nullptr;
    ID2D1SolidColorBrush* pGhostBrush = nullptr;
    ID2D1StrokeStyle* pDashedStyle = nullptr;
    ID2D1SolidColorBrush* pCueBrush = nullptr;
    ID2D1SolidColorBrush* pReflectBrush = nullptr;
    ID2D1SolidColorBrush* pCyanBrush = nullptr;
    ID2D1SolidColorBrush* pPurpleBrush = nullptr;
    pRT->CreateSolidColorBrush(AIM_LINE_COLOR, &pBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.5f), &pGhostBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.4f, 0.2f), &pCueBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightCyan, 0.6f), &pReflectBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Cyan), &pCyanBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Purple), &pPurpleBrush);
    if (pFactory) {
        D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties();
        strokeProps.dashStyle = D2D1_DASH_STYLE_DASH;
        pFactory->CreateStrokeStyle(&strokeProps, nullptr, 0, &pDashedStyle);
    }
    // --- End Brush Creation ---

    // --- FOOLPROOF FIX: Use the AI's planned angle and power for drawing ---
    float angleToDraw = cueAngle;
    float powerToDraw = shotPower;

    if (isAiVisualizingShot) {
        // When the AI is showing its aim, force the drawing to use its planned shot details.
        angleToDraw = aiPlannedShotDetails.angle;
        powerToDraw = aiPlannedShotDetails.power;
    }
    // --- End AI Aiming Fix ---

  //=========================================================================================
  // [+] KIBITZER / DEBUG MODE IMPLEMENTATION
  //=========================================================================================
    if (g_debugMode && (shotPower > 0.0f || isAiVisualizingShot)) {
        // 1. Create a local simulation of the table state
        std::vector<Ball> simBalls = balls;
        Ball* simCue = nullptr;
        for (auto& b : simBalls) { if (b.id == 0) simCue = &b; }

        // 2. Apply the shot to the simulated cue ball
        if (simCue) {
            simCue->vx = cosf(angleToDraw) * powerToDraw;
            simCue->vy = sinf(angleToDraw) * powerToDraw;
            // Apply simplified spin physics to velocity
            float sx = (isAiVisualizingShot) ? aiPlannedShotDetails.spinX : cueSpinX;
            float sy = (isAiVisualizingShot) ? aiPlannedShotDetails.spinY : cueSpinY;
            simCue->vx += sinf(angleToDraw) * sy * 0.5f;
            simCue->vy -= cosf(angleToDraw) * sy * 0.5f;
            simCue->vx -= cosf(angleToDraw) * sx * 0.5f;
            simCue->vy -= sinf(angleToDraw) * sx * 0.5f;
        }

        // 3. Trajectory Recording
        // Map of Ball ID -> List of Points
        std::map<int, std::vector<D2D1_POINT_2F>> trajectories;
        for (const auto& b : simBalls) {
            if (!b.isPocketed) trajectories[b.id].push_back(D2D1::Point2F(b.x, b.y));
        }

        // 4. Run Physics Simulation Loop (Discrete steps)
        // Run for approx 4 seconds of game time (240 ticks) or until motion stops
        const int SIM_STEPS = 240;

        for (int step = 0; step < SIM_STEPS; ++step) {
            bool anyMoving = false;

            // A. Move Balls & Friction
            for (auto& b : simBalls) {
                if (b.isPocketed) continue;
                b.x += b.vx;
                b.y += b.vy;
                b.vx *= FRICTION;
                b.vy *= FRICTION;
                if (GetDistanceSq(b.vx, b.vy, 0, 0) > MIN_VELOCITY_SQ) anyMoving = true;

                // Record path every few frames to save memory/performance
                if (step % 3 == 0) {
                    trajectories[b.id].push_back(D2D1::Point2F(b.x, b.y));
                }
            }

            if (!anyMoving) break; // Optimization: Stop if everything stopped

            // B. Check Collisions (Simplified version of UpdatePhysics logic)
            // Wall Collisions
            for (auto& b : simBalls) {
                if (b.isPocketed) continue;
                // Use simplified bounds for debug speed
                if (b.x < TABLE_LEFT + BALL_RADIUS) { b.x = TABLE_LEFT + BALL_RADIUS; b.vx *= -1; }
                if (b.x > TABLE_RIGHT - BALL_RADIUS) { b.x = TABLE_RIGHT - BALL_RADIUS; b.vx *= -1; }
                if (b.y < TABLE_TOP + BALL_RADIUS) { b.y = TABLE_TOP + BALL_RADIUS; b.vy *= -1; }
                if (b.y > TABLE_BOTTOM - BALL_RADIUS) { b.y = TABLE_BOTTOM - BALL_RADIUS; b.vy *= -1; }
            }

            // Ball-Ball Collisions
            for (size_t i = 0; i < simBalls.size(); ++i) {
                for (size_t j = i + 1; j < simBalls.size(); ++j) {
                    Ball& b1 = simBalls[i];
                    Ball& b2 = simBalls[j];
                    if (b1.isPocketed || b2.isPocketed) continue;

                    float dx = b2.x - b1.x; float dy = b2.y - b1.y;
                    float distSq = dx * dx + dy * dy;
                    float minDist = BALL_RADIUS * 2.0f;

                    if (distSq < minDist * minDist) {
                        float dist = sqrtf(distSq);
                        float overlap = minDist - dist;
                        float nx = dx / dist; float ny = dy / dist;
                        b1.x -= overlap * 0.5f * nx; b1.y -= overlap * 0.5f * ny;
                        b2.x += overlap * 0.5f * nx; b2.y += overlap * 0.5f * ny;

                        float rvx = b1.vx - b2.vx; float rvy = b1.vy - b2.vy;
                        float velAlongNormal = rvx * nx + rvy * ny;

                        if (velAlongNormal > 0) {
                            b1.vx -= velAlongNormal * nx; b1.vy -= velAlongNormal * ny;
                            b2.vx += velAlongNormal * nx; b2.vy += velAlongNormal * ny;
                        }
                    }
                }
            }

            // C. Check Pockets (Simplified)
            for (auto& b : simBalls) {
                if (b.isPocketed) continue;
                for (int p = 0; p < 6; ++p) {
                    // Using simple distance check for speed in debug mode
                    if (GetDistanceSq(b.x, b.y, pocketPositions[p].x, pocketPositions[p].y) < CORNER_HOLE_VISUAL_RADIUS * CORNER_HOLE_VISUAL_RADIUS) {
                        b.isPocketed = true;
                        b.vx = 0; b.vy = 0;
                        // Record final pocket position
                        trajectories[b.id].push_back(D2D1::Point2F(b.x, b.y));
                    }
                }
            }
        }

        // 5. Draw the Trajectories
        // Create a specific brush for debug lines
        ID2D1SolidColorBrush* pDebugLineBrush = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Lime, 0.6f), &pDebugLineBrush);

        if (pDebugLineBrush) {
            for (auto const& [ballId, points] : trajectories) {
                if (points.size() < 2) continue;

                // Pick color: White for Cue, Colored for others
                if (ballId == 0) pDebugLineBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 0.8f));
                else pDebugLineBrush->SetColor(GetBallColor(ballId));
                pDebugLineBrush->SetOpacity(0.6f);

                // Draw path lines
                for (size_t i = 0; i < points.size() - 1; ++i) {
                    pRT->DrawLine(points[i], points[i + 1], pDebugLineBrush, 1.5f);
                }

                // Draw "Ghost Ball" at the END of the path if it moved significantly
                D2D1_POINT_2F endPos = points.back();
                D2D1_POINT_2F startPos = points.front();
                if (GetDistanceSq(startPos.x, startPos.y, endPos.x, endPos.y) > 10.0f) {
                    D2D1_ELLIPSE endCircle = D2D1::Ellipse(endPos, BALL_RADIUS, BALL_RADIUS);
                    // If in pocket, draw filled, else draw wireframe
                    bool endedInPocket = false;
                    for (int p = 0; p < 6; ++p) {
                        if (GetDistanceSq(endPos.x, endPos.y, pocketPositions[p].x, pocketPositions[p].y) < CORNER_HOLE_VISUAL_RADIUS * CORNER_HOLE_VISUAL_RADIUS) {
                            endedInPocket = true; break;
                        }
                    }

                    if (endedInPocket) {
                        pDebugLineBrush->SetOpacity(0.3f);
                        pRT->FillEllipse(&endCircle, pDebugLineBrush);
                        pDebugLineBrush->SetOpacity(1.0f);
                        pRT->DrawEllipse(&endCircle, pDebugLineBrush, 1.0f);
                    }
                    else {
                        pDebugLineBrush->SetOpacity(0.4f);
                        pRT->DrawEllipse(&endCircle, pDebugLineBrush, 1.0f);
                    }
                }
            }
            SafeRelease(&pDebugLineBrush);
        }
    }
    //=========================================================================================
    // [-] END KIBITZER MODE
    //=========================================================================================

      // --- Cue Stick Drawing ---
      /*const float baseStickLength = 150.0f;
      const float baseStickThickness = 4.0f;
      float stickLength = baseStickLength * 1.4f;
      float stickThickness = baseStickThickness * 1.5f;
      float stickAngle = angleToDraw + PI; // Use the angle we determined
      float powerOffset = 0.0f;
      if ((isAiming || isDraggingStick) || isAiVisualizingShot) {
          powerOffset = powerToDraw * 5.0f; // Use the power we determined
      }
      D2D1_POINT_2F cueStickEnd = D2D1::Point2F(cueBall->x + cosf(stickAngle) * (stickLength + powerOffset), cueBall->y + sinf(stickAngle) * (stickLength + powerOffset));
      D2D1_POINT_2F cueStickTip = D2D1::Point2F(cueBall->x + cosf(stickAngle) * (powerOffset + 5.0f), cueBall->y + sinf(stickAngle) * (powerOffset + 5.0f));
      pRT->DrawLine(cueStickTip, cueStickEnd, pCueBrush, stickThickness);*/

      // --- Projection Line Calculation ---
    float cosA = cosf(angleToDraw); // Use the angle we determined
    float sinA = sinf(angleToDraw);
    float rayLength = TABLE_WIDTH + TABLE_HEIGHT;
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

    //new code start for visual feedback targetting wrong player balls (robust, non-destructive)
    if (!g_isPracticeMode && hitBall && IsWrongTargetForCurrentPlayer(hitBall)) {
        // Use dedicated brushes for the overlay so we don't change or reuse pBrush/pTextFormat state.
        ID2D1SolidColorBrush* pOverlayFill = nullptr;   // semi-opaque black backdrop for readability
        ID2D1SolidColorBrush* pOverlayRed = nullptr;    // red outline + text
        ID2D1SolidColorBrush* pOverlaySpec = nullptr;   // tiny spec highlight

        HRESULT hr1 = pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.55f), &pOverlayFill);
        HRESULT hr2 = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red, 0.98f), &pOverlayRed);
        HRESULT hr3 = pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), &pOverlaySpec);

        if (SUCCEEDED(hr1) && SUCCEEDED(hr2) && SUCCEEDED(hr3) && pOverlayFill && pOverlayRed && pOverlaySpec) {
            // Outline circle (thin) so the user still sees the ball
            D2D1_ELLIPSE outlineEllipse = D2D1::Ellipse(D2D1::Point2F(hitBall->x, hitBall->y),
                BALL_RADIUS + 3.0f, BALL_RADIUS + 3.0f);
            pRT->DrawEllipse(&outlineEllipse, pOverlayRed, 2.6f);

            // Prepare text string and layout rect above the ball
            const wchar_t* overlayText = L"WRONG BALL";
            const UINT32 overlayLen = (UINT32)wcslen(overlayText);
            float txtW = 92.0f;   // text box width
            float txtH = 20.0f;   // text box height
            D2D1_RECT_F textRect = D2D1::RectF(
                hitBall->x - txtW / 2.0f,
                hitBall->y - BALL_RADIUS - txtH - 6.0f,
                hitBall->x + txtW / 2.0f,
                hitBall->y - BALL_RADIUS - 6.0f
            );

            // Draw semi-opaque rounded rectangular backdrop to improve readability
            D2D1_ROUNDED_RECT backdrop = D2D1::RoundedRect(textRect, 4.0f, 4.0f);
            pRT->FillRoundedRectangle(&backdrop, pOverlayFill);
            pRT->DrawRoundedRectangle(&backdrop, pOverlayRed, 1.2f);

            // Draw the overlay text centered in the box using existing pTextFormat (no modifications)
            if (pTextFormat) {
                // Save original alignment and temporarily center the text (do not modify global format objects)
                // We'll use DrawText with a temporary local copy of DWRITE alignment by creating a local format
                // to avoid changing pTextFormat's global state. Creating a tiny local bold format is cheap.
                IDWriteTextFormat* pLocalFormat = nullptr;
                if (pDWriteFactory) {
                    if (SUCCEEDED(pDWriteFactory->CreateTextFormat(
                        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                        12.0f, L"en-us", &pLocalFormat)))
                    {
                        pLocalFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                        pLocalFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    }
                }

                // draw text using the local format if available, otherwise fallback to global pTextFormat
                IDWriteTextFormat* pUseFmt = pLocalFormat ? pLocalFormat : pTextFormat;
                pRT->DrawTextW(overlayText, overlayLen, pUseFmt, &textRect, pOverlayRed);

                SafeRelease(&pLocalFormat);
            }

            // small white spec highlight inside the red outline to give a metal/flash feel
            D2D1_ELLIPSE spec = D2D1::Ellipse(D2D1::Point2F(hitBall->x + 6.0f, hitBall->y - BALL_RADIUS - 6.0f),
                2.0f, 2.0f);
            pRT->FillEllipse(&spec, pOverlaySpec);
        }

        SafeRelease(&pOverlayFill);
        SafeRelease(&pOverlayRed);
        SafeRelease(&pOverlaySpec);
    }
    //new code end for visual feedback targetting wrong player balls

    /*//new code start for visual feedback targetting wrong player balls
    if (hitBall && IsWrongTargetForCurrentPlayer(hitBall)) {
        // Create red brush for outline and text
        ID2D1SolidColorBrush* pRedBrush = nullptr;
        HRESULT hr = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &pRedBrush);
        if (SUCCEEDED(hr) && pRedBrush) {
            // Draw red outline circle slightly larger than ball radius
            D2D1_ELLIPSE outlineEllipse = D2D1::Ellipse(D2D1::Point2F(hitBall->x, hitBall->y), BALL_RADIUS + 3.0f, BALL_RADIUS + 3.0f);
            pRT->DrawEllipse(&outlineEllipse, pRedBrush, 3.0f);

            // Draw "WRONG BALL" text above the ball
            if (pTextFormat) {
                // Define a rectangle above the ball for the text
                D2D1_RECT_F textRect = D2D1::RectF(
                    hitBall->x - 40.0f,
                    hitBall->y - BALL_RADIUS - 30.0f,
                    hitBall->x + 40.0f,
                    hitBall->y - BALL_RADIUS - 10.0f
                );
                pRT->DrawText(L"WRONG", 5, pTextFormat, &textRect, pRedBrush);
                //pRT->DrawText(L"WRONG BALL", 10, pTextFormat, &textRect, pRedBrush);
            }

            SafeRelease(&pRedBrush);
        }
    }*/
    //new code end for visual feedback targetting wrong player balls

    // Find the first rail hit by the aiming ray
    D2D1_POINT_2F railHitPoint = rayEnd; // Default to far end if no rail hit
    float minRailDistSq = rayLength * rayLength;
    int hitRailIndex = -1; // 0:Left, 1:Right, 2:Top, 3:Bottom

        // --- Use inner chamfered rim segments (avoid pocket openings) for aim-bounce intersection checks ---
    //const float rimWidth = 15.0f; // must match rimWidth used elsewhere in the project 15
    const float rimLeft = TABLE_LEFT + INNER_RIM_THICKNESS;
    const float rimRight = TABLE_RIGHT - INNER_RIM_THICKNESS;
    const float rimTop = TABLE_TOP + INNER_RIM_THICKNESS;
    const float rimBottom = TABLE_BOTTOM - INNER_RIM_THICKNESS;

    D2D1_POINT_2F currentIntersection;
    const float gapMargin = 6.0f; // space to keep away from pocket mouth visuals

    // LEFT inner rim segment (vertical), skip the corner pocket areas
    {
        D2D1_POINT_2F segA = D2D1::Point2F(rimLeft, rimTop + CORNER_HOLE_VISUAL_RADIUS + gapMargin);
        D2D1_POINT_2F segB = D2D1::Point2F(rimLeft, rimBottom - CORNER_HOLE_VISUAL_RADIUS - gapMargin);
        // Only test if segment is valid
        if (segB.y > segA.y) {
            if (LineSegmentIntersection(rayStart, rayEnd, segA, segB, currentIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
                if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 0; }
            }
        }
    }

    // RIGHT inner rim segment (vertical)
    {
        D2D1_POINT_2F segA = D2D1::Point2F(rimRight, rimTop + CORNER_HOLE_VISUAL_RADIUS + gapMargin);
        D2D1_POINT_2F segB = D2D1::Point2F(rimRight, rimBottom - CORNER_HOLE_VISUAL_RADIUS - gapMargin);
        if (segB.y > segA.y) {
            if (LineSegmentIntersection(rayStart, rayEnd, segA, segB, currentIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
                if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 1; }
            }
        }
    }

    // TOP inner rim — split into left and right segments around the top-middle pocket
    {
        float midX = pocketPositions[1].x;
        float midRadius = GetPocketVisualRadius(1);
        float leftStartX = rimLeft + CORNER_HOLE_VISUAL_RADIUS + gapMargin;
        float leftEndX = midX - midRadius - gapMargin;
        if (leftEndX > leftStartX) {
            D2D1_POINT_2F segA = D2D1::Point2F(leftStartX, rimTop);
            D2D1_POINT_2F segB = D2D1::Point2F(leftEndX, rimTop);
            if (LineSegmentIntersection(rayStart, rayEnd, segA, segB, currentIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
                if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 2; }
            }
        }

        float rightStartX = midX + midRadius + gapMargin;
        float rightEndX = rimRight - CORNER_HOLE_VISUAL_RADIUS - gapMargin;
        if (rightEndX > rightStartX) {
            D2D1_POINT_2F segA = D2D1::Point2F(rightStartX, rimTop);
            D2D1_POINT_2F segB = D2D1::Point2F(rightEndX, rimTop);
            if (LineSegmentIntersection(rayStart, rayEnd, segA, segB, currentIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
                if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 2; }
            }
        }
    }

    // BOTTOM inner rim — split into left and right around bottom-middle pocket
    {
        float midX = pocketPositions[4].x; // bottom-middle pocket index is 4
        float midRadius = GetPocketVisualRadius(4);
        float leftStartX = rimLeft + CORNER_HOLE_VISUAL_RADIUS + gapMargin;
        float leftEndX = midX - midRadius - gapMargin;
        if (leftEndX > leftStartX) {
            D2D1_POINT_2F segA = D2D1::Point2F(leftStartX, rimBottom);
            D2D1_POINT_2F segB = D2D1::Point2F(leftEndX, rimBottom);
            if (LineSegmentIntersection(rayStart, rayEnd, segA, segB, currentIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
                if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 3; }
            }
        }

        float rightStartX = midX + midRadius + gapMargin;
        float rightEndX = rimRight - CORNER_HOLE_VISUAL_RADIUS - gapMargin;
        if (rightEndX > rightStartX) {
            D2D1_POINT_2F segA = D2D1::Point2F(rightStartX, rimBottom);
            D2D1_POINT_2F segB = D2D1::Point2F(rightEndX, rimBottom);
            if (LineSegmentIntersection(rayStart, rayEnd, segA, segB, currentIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, currentIntersection.x, currentIntersection.y);
                if (distSq < minRailDistSq) { minRailDistSq = distSq; railHitPoint = currentIntersection; hitRailIndex = 3; }
            }
        }
    }

    /*// Define table edge segments for intersection checks
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
    }*/

    //gpt5code start

    // --- FALLBACK: If the aiming ray did NOT hit any inner-rim segment (i.e. it goes through a pocket),
// clamp the aiming line to the table felt perimeter so it never draws past the felt/pocket holes.
// We compute intersections with the table rectangle edges and pick the nearest one.
    if (railHitPoint.x == rayEnd.x && railHitPoint.y == rayEnd.y) {
        // table outer edges (felt perimeter)
        D2D1_POINT_2F edgeA[4] = {
            { TABLE_LEFT,  TABLE_TOP },    // top-left
            { TABLE_RIGHT, TABLE_TOP },    // top-right
            { TABLE_RIGHT, TABLE_BOTTOM }, // bottom-right
            { TABLE_LEFT,  TABLE_BOTTOM }  // bottom-left
        };
        D2D1_POINT_2F edgeB[4] = {
            { TABLE_RIGHT, TABLE_TOP },    // top
            { TABLE_RIGHT, TABLE_BOTTOM }, // right
            { TABLE_LEFT,  TABLE_BOTTOM }, // bottom
            { TABLE_LEFT,  TABLE_TOP }     // left
        };

        D2D1_POINT_2F tableIntersection;
        for (int ei = 0; ei < 4; ++ei) {
            if (LineSegmentIntersection(rayStart, rayEnd, edgeA[ei], edgeB[ei], tableIntersection)) {
                float distSq = GetDistanceSq(rayStart.x, rayStart.y, tableIntersection.x, tableIntersection.y);
                if (distSq < minRailDistSq) {
                    minRailDistSq = distSq;
                    railHitPoint = tableIntersection;
                    // Map edge index to hitRailIndex semantics (0:left,1:right,2:top,3:bottom)
                    if (ei == 0) hitRailIndex = 2; // top
                    else if (ei == 1) hitRailIndex = 1; // right
                    else if (ei == 2) hitRailIndex = 3; // bottom
                    else hitRailIndex = 0; // left
                }
            }
        }
    }
    //gpt5code end

    // --- Determine final aim line end point ---
    D2D1_POINT_2F finalLineEnd = railHitPoint; // Assume rail hit first
    bool aimingAtRail = true;

    if (hitBall && firstHitDistSq < minRailDistSq) {
        // Ball collision is closer than rail collision
        finalLineEnd = ballCollisionPoint; // End line at the point of contact on the ball
        aimingAtRail = false;
    }

    // --- Draw Primary Aiming Line ---
     // --- CONTEXT: This is the start of the NEW, ACCURATE code block ---

        // Draw primary cue ball path (the white dashed line to the contact point)
        // --- Draw Primary Aiming Line & Trajectories ---
    pRT->DrawLine(rayStart, finalLineEnd, pBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

    // If the line hits a ball, draw the ghost ball and the accurate Purple/Cyan trajectory lines
    if (!aimingAtRail && hitBall) {
        // 1. Draw the ghost ball at the point of contact
        D2D1_ELLIPSE ghostCue = D2D1::Ellipse(ballCollisionPoint, BALL_RADIUS, BALL_RADIUS);
        pRT->DrawEllipse(ghostCue, pGhostBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

        // --- HARD-ANGLED CUT SHOT VISUALIZATION ---
        D2D1_POINT_2F targetCenter = { hitBall->x, hitBall->y };
        D2D1_POINT_2F ghostCenter = ballCollisionPoint;

        // **CRITICAL**: Check if the shot is physically possible. The aiming line will disappear
        // if you aim past the ball's edge for an impossible cut.
        float distToCenterSq = GetDistanceSq(ghostCenter.x, ghostCenter.y, targetCenter.x, targetCenter.y);
        const float MAX_CONTACT_DIST_SQ = (BALL_RADIUS * 2.0f) * (BALL_RADIUS * 2.0f);

        if (distToCenterSq <= MAX_CONTACT_DIST_SQ) {
            // The shot is possible, so draw the trajectory lines.

            // PATH 1: Object Ball Trajectory (Purple Line)
            float purpleAngle = atan2f(targetCenter.y - ghostCenter.y, targetCenter.x - ghostCenter.x);
            D2D1_POINT_2F purpleStart = targetCenter;
            D2D1_POINT_2F purpleEnd = {
                purpleStart.x + cosf(purpleAngle) * rayLength,
                purpleStart.y + sinf(purpleAngle) * rayLength
            };

            // PATH 2: Cue Ball Trajectory (Cyan Line) - Stun Shot
            float cyanAngle;
            float straightShotEpsilonSq = 0.1f;
            if (distToCenterSq < straightShotEpsilonSq) {
                cyanAngle = -1000; // Invalid angle to prevent drawing for straight-on shots
            }
            else {
                // For a cut shot, the cue ball deflects perpendicularly.
                D2D1_POINT_2F vInitial = { ghostCenter.x - rayStart.x, ghostCenter.y - rayStart.y };
                D2D1_POINT_2F vToTarget = { targetCenter.x - rayStart.x, targetCenter.y - rayStart.y };
                float cross_product_z = vInitial.x * vToTarget.y - vInitial.y * vToTarget.x;
                cyanAngle = (cross_product_z > 0) ? (purpleAngle - PI / 2.0f) : (purpleAngle + PI / 2.0f);
            }

            D2D1_POINT_2F cyanStart = ghostCenter;
            D2D1_POINT_2F cyanEnd = {
                cyanStart.x + cosf(cyanAngle) * rayLength,
                cyanStart.y + sinf(cyanAngle) * rayLength
            };

            // Draw the lines (unclipped as requested)
            pRT->DrawLine(purpleStart, purpleEnd, pPurpleBrush, 2.0f);
            if (cyanAngle > -999) {
                pRT->DrawLine(cyanStart, cyanEnd, pCyanBrush, 2.0f);
            }
        }
    }
    else if (aimingAtRail && hitRailIndex != -1) {
        // This is the missing rail reflection logic.
        float reflectAngle = angleToDraw;
        if (hitRailIndex == 0 || hitRailIndex == 1) { // Left or Right rail
            reflectAngle = PI - angleToDraw;
        }
        else { // Top or Bottom rail
            reflectAngle = -angleToDraw;
        }
        while (reflectAngle > PI) reflectAngle -= 2 * PI;
        while (reflectAngle <= -PI) reflectAngle += 2 * PI;

        float reflectionLength = 150.0f;
        D2D1_POINT_2F reflectionEnd = {
            finalLineEnd.x + cosf(reflectAngle) * reflectionLength,
            finalLineEnd.y + sinf(reflectAngle) * reflectionLength
        };
        pRT->DrawLine(finalLineEnd, reflectionEnd, pReflectBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);
    }
    // If the shot is impossible (aiming too far to the side), no lines are drawn.  
    /*// 1. Draw the ghost ball at the point of contact
    D2D1_ELLIPSE ghostCue = D2D1::Ellipse(ballCollisionPoint, BALL_RADIUS, BALL_RADIUS);
    pRT->DrawEllipse(ghostCue, pGhostBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

    // --- ACCURATE FORWARD-FACING DUAL TRAJECTORY CALCULATION ---

    // Define key points for clarity
    D2D1_POINT_2F targetCenter = { hitBall->x, hitBall->y };
    D2D1_POINT_2F ghostCenter = ballCollisionPoint; // This is the impact point

    // --- PATH 1: Object Ball Trajectory (Purple Line) ---
    // Physics: The object ball travels along the line from the ghost ball's center through its own center.
    float purpleAngle = atan2f(targetCenter.y - ghostCenter.y, targetCenter.x - ghostCenter.x);
    D2D1_POINT_2F purpleStart = targetCenter;
    D2D1_POINT_2F purpleEnd = {
        purpleStart.x + cosf(purpleAngle) * rayLength,
        purpleStart.y + sinf(purpleAngle) * rayLength
    };

    // --- PATH 2: Cue Ball Trajectory After Impact (Cyan Line) - Stun Shot Approximation ---
            // --- PATH 2: Cue Ball Trajectory After Impact (Cyan Line) - Follow Shot Approximation ---
    // This model shows the cue ball deflecting but also moving forward, which is intuitive for a default shot.

    // First, get the direction for a pure "stun" shot (perpendicular to the object ball's path).
    D2D1_POINT_2F cueInitialDir = { cosA, sinA }; // The direction the cue ball was initially traveling.
    D2D1_POINT_2F impactLineDir = { cosf(purpleAngle), sinf(purpleAngle) }; // The direction of force transfer.
    float projection = cueInitialDir.x * impactLineDir.x + cueInitialDir.y * impactLineDir.y;
    D2D1_POINT_2F cueStunDir = { // This is the perpendicular component of the initial velocity.
        cueInitialDir.x - projection * impactLineDir.x,
        cueInitialDir.y - projection * impactLineDir.y
    };

    // The "follow" direction is a blend of the initial forward motion and the stun deflection.
    // For a straight shot, stun_dir is (0,0), so the cue follows forward.
    // For a cut shot, it's a mix, showing both deflection and forward roll.
    D2D1_POINT_2F cueFollowDir = {
        (cueInitialDir.x * 0.5f) + (cueStunDir.x * 0.5f), // Blend the two directions
        (cueInitialDir.y * 0.5f) + (cueStunDir.y * 0.5f)
    };

    // Normalize the final blended direction vector
    float cueFollowDirLen = sqrtf(cueFollowDir.x * cueFollowDir.x + cueFollowDir.y * cueFollowDir.y);
    if (cueFollowDirLen > 1e-6f) {
        cueFollowDir.x /= cueFollowDirLen;
        cueFollowDir.y /= cueFollowDirLen;
    }

    float cyanAngle = atan2f(cueFollowDir.y, cueFollowDir.x);
    D2D1_POINT_2F cyanStart = ghostCenter;
    D2D1_POINT_2F cyanEnd = {
        cyanStart.x + cosf(cyanAngle) * rayLength,
        cyanStart.y + sinf(cyanAngle) * rayLength
    };*/

    /*// Physics: For a stun shot, the cue ball's path is perpendicular to the object ball's path.
    // We determine the direction of deflection ("left" or "right") using a cross product.
    D2D1_POINT_2F vInitial = { ghostCenter.x - rayStart.x, ghostCenter.y - rayStart.y };
    D2D1_POINT_2F vToTarget = { targetCenter.x - rayStart.x, targetCenter.y - rayStart.y };
    float cross_product_z = vInitial.x * vToTarget.y - vInitial.y * vToTarget.x;

    // If target is to the "left" of the cue's path (cross > 0), cue deflects "right" (-90 deg).
    // If target is to the "right" (cross < 0), cue deflects "left" (+90 deg).
    float cyanAngle = (cross_product_z > 0) ? (purpleAngle - PI / 2.0f) : (purpleAngle + PI / 2.0f);

    D2D1_POINT_2F cyanStart = ghostCenter;
    D2D1_POINT_2F cyanEnd = {
        cyanStart.x + cosf(cyanAngle) * rayLength,
        cyanStart.y + sinf(cyanAngle) * rayLength
    };*/

    // --- Shorten lines to the first rail they hit for a clean look ---
    /*D2D1_POINT_2F intersection;
    if (FindFirstRailIntersection(purpleStart, purpleEnd, intersection)) {
        purpleEnd = intersection;
    }
    if (FindFirstRailIntersection(cyanStart, cyanEnd, intersection)) {
        cyanEnd = intersection;
    }

    // --- Draw the accurate, forward-facing lines ---
    pRT->DrawLine(purpleStart, purpleEnd, pPurpleBrush, 2.0f);
    pRT->DrawLine(cyanStart, cyanEnd, pCyanBrush, 2.0f);
}*/

/*// Draw primary cue ball path (white dashed line)
pRT->DrawLine(rayStart, finalLineEnd, pBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

// If the line hits a ball, draw the ghost ball and trajectory lines
if (!aimingAtRail && hitBall) {
    // 1. Draw the ghost ball at the point of contact
    D2D1_ELLIPSE ghostCue = D2D1::Ellipse(ballCollisionPoint, BALL_RADIUS, BALL_RADIUS);
    pRT->DrawEllipse(ghostCue, pGhostBrush, 1.0f, pDashedStyle ? pDashedStyle : NULL);

    // --- ACCURATE DUAL TRAJECTORY CALCULATION (PURPLE & CYAN LINES) ---

    // Define key points for clarity
    D2D1_POINT_2F targetCenter = { hitBall->x, hitBall->y };
    D2D1_POINT_2F ghostCenter = ballCollisionPoint; // This is the impact point

    // 2. OBJECT BALL TRAJECTORY (PURPLE LINE)
    // Physics: The object ball ALWAYS travels along the line connecting its center to the impact point.
    float objectBallDx = ghostCenter.x - targetCenter.x;
    float objectBallDy = ghostCenter.y - targetCenter.y;
    float objectBallAngle = atan2f(objectBallDy, objectBallDx);

    D2D1_POINT_2F purpleStart = targetCenter;
    D2D1_POINT_2F purpleEnd = {
        purpleStart.x + cosf(objectBallAngle) * rayLength,
        purpleStart.y + sinf(objectBallAngle) * rayLength
    };

    // 3. CUE BALL TRAJECTORY AFTER IMPACT (CYAN LINE) - Stun Shot Approximation
    // Physics: For a stun shot, the cue ball's final velocity is perpendicular to the object ball's path.
    // The cue ball's new direction is its initial direction minus the component parallel to the impact line.
    D2D1_POINT_2F cueInitialDir = { cosA, sinA };
    D2D1_POINT_2F impactLineDir = { cosf(objectBallAngle), sinf(objectBallAngle) };

    // Find how much of the cue's initial direction is along the impact line using the dot product.
    float projection = cueInitialDir.x * impactLineDir.x + cueInitialDir.y * impactLineDir.y;

    // Subtract this component to get the perpendicular (tangent) direction.
    D2D1_POINT_2F cueFinalDir = {
        cueInitialDir.x - projection * impactLineDir.x,
        cueInitialDir.y - projection * impactLineDir.y
    };

    // Normalize the final direction vector
    float cueFinalDirLen = sqrtf(cueFinalDir.x * cueFinalDir.x + cueFinalDir.y * cueFinalDir.y);
    if (cueFinalDirLen > 1e-6f) {
        cueFinalDir.x /= cueFinalDirLen;
        cueFinalDir.y /= cueFinalDirLen;
    }

    D2D1_POINT_2F cyanStart = ghostCenter;
    D2D1_POINT_2F cyanEnd = {
        cyanStart.x + cueFinalDir.x * rayLength,
        cyanStart.y + cueFinalDir.y * rayLength
    };

    // --- Shorten lines to the first rail they hit for a clean look ---
    D2D1_POINT_2F intersection;
    if (FindFirstRailIntersection(purpleStart, purpleEnd, intersection)) {
        purpleEnd = intersection;
    }
    if (FindFirstRailIntersection(cyanStart, cyanEnd, intersection)) {
        cyanEnd = intersection;
    }

    // 4. Draw the accurate trajectory lines
    pRT->DrawLine(purpleStart, purpleEnd, pPurpleBrush, 2.0f, pDashedStyle ? pDashedStyle : NULL);
    pRT->DrawLine(cyanStart, cyanEnd, pCyanBrush, 2.0f, pDashedStyle ? pDashedStyle : NULL);
}*/
// --- CONTEXT: This is the end of the NEW, ACCURATE code block ---
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

//}
/*else if (aimingAtRail && hitRailIndex != -1) {
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
}*/

// Release resources
    SafeRelease(&pBrush);
    SafeRelease(&pGhostBrush);
    SafeRelease(&pCueBrush);
    SafeRelease(&pReflectBrush); // Release new brush
    SafeRelease(&pCyanBrush);
    SafeRelease(&pPurpleBrush);
    SafeRelease(&pDashedStyle);
}

// --- NEW: Function to re-rack balls in Practice Mode ---
void ReRackPracticeMode() {
    // This function is ONLY for practice mode.
    if (!g_isPracticeMode) return;

    // 1. Find all object balls and reset them
    std::vector<Ball*> objectBallsToRack;
    // --- FIX: Set correct ball count based on game mode ---
    int ballsToRackCount = 0;
    if (currentGameType == GameType::NINE_BALL) {
        ballsToRackCount = 9;
    }
    else if (currentGameType == GameType::EIGHT_BALL_MODE) {
        ballsToRackCount = 15;
    }
    else if (currentGameType == GameType::STRAIGHT_POOL) {
        // Follow official 14.1 rules:
        // - When only 1 ball remains, re-rack exactly 14 balls
        // - Leave the penultimate ball on the table
        if (ballsOnTableCount == 1) {
            ballsToRackCount = 14; // Always 14 per 14.1 rules
        }
        else {
            ballsToRackCount = 15; // Full rack for initial break
        }
    }
    else {
        ballsToRackCount = 15; // Fallback
    }
    ballsOnTableCount = ballsToRackCount; // Reset the global count
    // --- END FIX ---

    for (int id = 1; id <= ballsToRackCount; ++id) {
        Ball* b = GetBallById(id);
        if (b) {
            objectBallsToRack.push_back(b);
        }
    }

    // 2. Define rack positions
    float spacingX = BALL_RADIUS * 2.0f * 0.866f;
    float spacingY = BALL_RADIUS * 2.0f * 1.0f;

    if (currentGameType == GameType::NINE_BALL) {
        // --- 9-Ball Racking (Diamond Shape) ---
        D2D1_POINT_2F rackPositions[9];
        rackPositions[0] = D2D1::Point2F(RACK_POS_X, RACK_POS_Y); // 1-ball (Apex)
        rackPositions[1] = D2D1::Point2F(RACK_POS_X + spacingX, RACK_POS_Y - spacingY / 2.0f); // Row 2
        rackPositions[2] = D2D1::Point2F(RACK_POS_X + spacingX, RACK_POS_Y + spacingY / 2.0f);
        rackPositions[3] = D2D1::Point2F(RACK_POS_X + 2 * spacingX, RACK_POS_Y - spacingY); // Row 3
        rackPositions[4] = D2D1::Point2F(RACK_POS_X + 2 * spacingX, RACK_POS_Y); // 9-ball (Center)
        rackPositions[5] = D2D1::Point2F(RACK_POS_X + 2 * spacingX, RACK_POS_Y + spacingY);
        rackPositions[6] = D2D1::Point2F(RACK_POS_X + 3 * spacingX, RACK_POS_Y - spacingY / 2.0f); // Row 4
        rackPositions[7] = D2D1::Point2F(RACK_POS_X + 3 * spacingX, RACK_POS_Y + spacingY / 2.0f);
        rackPositions[8] = D2D1::Point2F(RACK_POS_X + 4 * spacingX, RACK_POS_Y); // Row 5 (Back)

        // Find 1-ball and 9-ball
        Ball* ball1 = GetBallById(1);
        Ball* ball9 = GetBallById(9);
        std::vector<Ball*> otherBalls;
        for (int id = 2; id <= 8; ++id) {
            otherBalls.push_back(GetBallById(id));
        }
        // [+] C++20 Migration: Use std::shuffle
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(otherBalls.begin(), otherBalls.end(), g);

        // Place 1-ball at Apex (Pos 0)
        ball1->x = rackPositions[0].x; ball1->y = rackPositions[0].y;
        ball1->vx = 0; ball1->vy = 0; ball1->isPocketed = false;
        // Place 9-ball at Center (Pos 4)
        ball9->x = rackPositions[4].x; ball9->y = rackPositions[4].y;
        ball9->vx = 0; ball9->vy = 0; ball9->isPocketed = false;

        // Place remaining 7 balls in other spots
        int otherBallIdx = 0;
        for (int i = 1; i < 9; ++i) {
            if (i == 4) continue; // Skip center spot
            Ball* b = otherBalls[otherBallIdx++];
            b->x = rackPositions[i].x;
            b->y = rackPositions[i].y;
            b->vx = 0; b->vy = 0; b->isPocketed = false;
        }
        ballsOnTableCount = 9;

    }
    else {
        // --- 8-Ball and Straight Pool Racking (Triangle) ---
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

        // [+] C++20 Migration: Use std::shuffle
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(objectBallsToRack.begin(), objectBallsToRack.end(), g);

        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            int eightBallCurrentIndex = -1;
            for (size_t i = 0; i < objectBallsToRack.size(); ++i) {
                if (objectBallsToRack[i]->id == 8) {
                    eightBallCurrentIndex = (int)i;
                    break;
                }
            }
            if (eightBallCurrentIndex != -1 && eightBallCurrentIndex != 4) {
                std::swap(objectBallsToRack[eightBallCurrentIndex], objectBallsToRack[4]);
            }
        }

        // --- MODIFIED LOOP: Use the dynamic ballsToRackCount ---
        for (int i = 0; i < ballsToRackCount && i < objectBallsToRack.size(); ++i) {
            Ball* ballToPlace = objectBallsToRack[i];
            ballToPlace->x = rackPositions[i].x;
            ballToPlace->y = rackPositions[i].y;
            ballToPlace->vx = 0; ballToPlace->vy = 0;
            ballToPlace->isPocketed = false;
        }
        ballsOnTableCount = 15;
    }

    // 4. Reset Cue Ball
    Ball* cueBall = GetCueBall();
    if (cueBall) {
        cueBall->isPocketed = false;
        RespawnCueBall(true); // Put cue ball in kitchen
    }

    // 5. Reset counts and state
    isOpeningBreakShot = true;
    pocketedThisTurn.clear();
    currentGameState = PRE_BREAK_PLACEMENT; // Go to placement state
    currentPlayer = 1; // Always player 1
    aiTurnPending = false;
    foulCommitted = false;
    foulDisplayCounter = 0;

    if (!g_soundEffectsMuted) {
        PlayGameSound(g_soundBufferRack);
        //PlaySound(TEXT("rack.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
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
    // FIX: This condition correctly shows the AI's called pocket arrow (and not in practice mode).
    if (!g_isPracticeMode && isPlayer2AI && IsPlayerOnEightBall(2) && calledPocketP2 >= 0) {
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
            TABLE_BOTTOM + WOODEN_RAIL_THICKNESS + 5.0f,
            TABLE_RIGHT,
            TABLE_BOTTOM + WOODEN_RAIL_THICKNESS + 30.0f
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

    if (g_isPracticeMode) {
        // Draw simplified UI for Practice Mode
        std::wostringstream oss;
        oss << L"Practice Mode (Free Play)";
        if (currentGameType == GameType::STRAIGHT_POOL) {
            oss << L"\nScore: " << player1StraightPoolScore; // Still show score for Straight Pool
        }
        /*if (currentGameType == GameType::STRAIGHT_POOL) {
            oss << L"\nScore: " << player1StraightPoolScore << L" / " << targetScoreStraightPool; // Still show score for Straight Pool
        }*/
        D2D1_RECT_F practiceRect = D2D1::RectF(TABLE_LEFT, uiTop, TABLE_RIGHT, uiTop + uiHeight);

        // --- FOOLPROOF: Use pTextFormatBold and ensure centering ---
        IDWriteTextFormat* pFormat = pTextFormatBold ? pTextFormatBold : pTextFormat;
        DWRITE_TEXT_ALIGNMENT oldAlign = pFormat->GetTextAlignment();
        DWRITE_PARAGRAPH_ALIGNMENT oldParaAlign = pFormat->GetParagraphAlignment();

        pFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        pFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        pRT->DrawText(oss.str().c_str(), (UINT32)oss.str().length(), pFormat, &practiceRect, pBrush);

        // Restore previous alignments
        pFormat->SetTextAlignment(oldAlign);
        pFormat->SetParagraphAlignment(oldParaAlign);
        // --- END FOOLPROOF ---

    }
    else {

        // Player 1 Info Text (Mode Dependent)
        std::wostringstream oss1;
        oss1 << player1Info.name.c_str();
        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            if (player1Info.assignedType != BallType::NONE) {
                if (IsPlayerOnEightBall(1)) oss1 << L"\nON 8-BALL";
                else oss1 << L"\n" << ((player1Info.assignedType == BallType::SOLID) ? L"Solids" : L"Stripes") << L" [" << player1Info.ballsPocketedCount << L"/7]";
            }
            else oss1 << L"\n(Open Table)";
        }
        else if (currentGameType == GameType::NINE_BALL) { // CORRECTED: This is now part of the same if-else chain
            oss1 << L"\nTarget: " << lowestBallOnTable << L"-Ball";
            if (player1ConsecutiveFouls > 0) {
                oss1 << L" (Fouls: " << player1ConsecutiveFouls << ")";
            }
            if (isPushOutAvailable && currentPlayer == 1) {
                oss1 << L"\n(Push-Out Available)";
            }
        }
        else { // STRAIGHT_POOL - This is the single "else" for Straight Pool
            oss1 << L"\nScore: " << player1StraightPoolScore << L" / " << targetScoreStraightPool;
        }
        IDWriteTextFormat* pFormatP1 = (currentPlayer == 1 && currentGameState != GAME_OVER) ? pTextFormatBold : pTextFormat;
        pRT->DrawText(oss1.str().c_str(), (UINT32)oss1.str().length(), pFormatP1, &p1Rect, pBrush);

        // Draw Player Side Ball Indicators (Only for 8-Ball)
        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            if (player1Info.assignedType != BallType::NONE)
            {
                D2D1_POINT_2F ballCenter = D2D1::Point2F(p1Rect.right + 15.0f, p1Rect.top + uiHeight / 2.0f);
                float radius = 10.0f;
                D2D1_ELLIPSE ball = D2D1::Ellipse(ballCenter, radius, radius);

                if (IsPlayerOnEightBall(1))
                {
                    // Player is on the 8-ball, draw the 8-ball indicator.
                    ID2D1SolidColorBrush* p8BallBrush = nullptr;
                    pRT->CreateSolidColorBrush(EIGHT_BALL_COLOR, &p8BallBrush);
                    if (p8BallBrush) pRT->FillEllipse(&ball, p8BallBrush);
                    SafeRelease(&p8BallBrush);

                    // Draw the number '8' decal
                    ID2D1SolidColorBrush* pNumWhite = nullptr, * pNumBlack = nullptr;
                    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pNumWhite);
                    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pNumBlack);
                    if (pBallNumFormat && pNumWhite && pNumBlack)
                    {
                        const float decalR = radius * 0.7f;
                        D2D1_ELLIPSE decal = D2D1::Ellipse(ballCenter, decalR, decalR);
                        pRT->FillEllipse(&decal, pNumWhite);
                        pRT->DrawEllipse(&decal, pNumBlack, 0.8f);
                        wchar_t numText[] = L"8";
                        D2D1_RECT_F layout = { ballCenter.x - decalR, ballCenter.y - decalR, ballCenter.x + decalR, ballCenter.y + decalR };
                        pRT->DrawText(numText, (UINT32)wcslen(numText), pBallNumFormat, &layout, pNumBlack);
                    }

                    // If stripes, draw a stripe band
                    SafeRelease(&pNumWhite);
                    SafeRelease(&pNumBlack);
                }
                else
                {
                    // Default: Draw the player's assigned ball type (solid/stripe).
                    ID2D1SolidColorBrush* pBallBrush = nullptr;
                    D2D1_COLOR_F ballColor = (player1Info.assignedType == BallType::SOLID) ?
                        D2D1::ColorF(1.0f, 1.0f, 0.0f) : D2D1::ColorF(1.0f, 0.0f, 0.0f);
                    pRT->CreateSolidColorBrush(ballColor, &pBallBrush);
                    if (pBallBrush)
                    {
                        pRT->FillEllipse(&ball, pBallBrush);
                        SafeRelease(&pBallBrush);
                        if (player1Info.assignedType == BallType::STRIPE)
                        {
                            ID2D1SolidColorBrush* pStripeBrush = nullptr;
                            pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pStripeBrush);
                            if (pStripeBrush)
                            {
                                D2D1_RECT_F stripeRect = { ballCenter.x - radius, ballCenter.y - 3.0f, ballCenter.x + radius, ballCenter.y + 3.0f };
                                pRT->FillRectangle(&stripeRect, pStripeBrush);
                                SafeRelease(&pStripeBrush);
                            }
                        }
                    }
                }
                // Draw a border around the indicator ball, regardless of type.
                ID2D1SolidColorBrush* pBorderBrush = nullptr;
                pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
                if (pBorderBrush) pRT->DrawEllipse(&ball, pBorderBrush, 1.5f);
                SafeRelease(&pBorderBrush);
            }
        }

        // Player 2 Info Text (Mode Dependent)
        std::wostringstream oss2;
        oss2 << player2Info.name.c_str();
        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            if (player2Info.assignedType != BallType::NONE) {
                if (IsPlayerOnEightBall(2)) oss2 << L"\nON 8-BALL";
                else oss2 << L"\n" << ((player2Info.assignedType == BallType::SOLID) ? L"Solids" : L"Stripes") << L" [" << player2Info.ballsPocketedCount << L"/7]";
            }
            else oss2 << L"\n(Open Table)";
        }
        else if (currentGameType == GameType::NINE_BALL) { // CORRECTED: This is now part of the same if-else chain
            oss2 << L"\nTarget: " << lowestBallOnTable << L"-Ball";
            if (player2ConsecutiveFouls > 0) {
                oss2 << L" (Fouls: " << player2ConsecutiveFouls << ")";
            }
            if (isPushOutAvailable && currentPlayer == 2) {
                oss2 << L"\n(Push-Out Available)";
            }
        }
        else { // STRAIGHT_POOL - This is the single "else" for Straight Pool
            oss2 << L"\nScore: " << player2StraightPoolScore << L" / " << targetScoreStraightPool;
        }
        IDWriteTextFormat* pFormatP2 = (currentPlayer == 2 && currentGameState != GAME_OVER) ? pTextFormatBold : pTextFormat;
        pRT->DrawText(oss2.str().c_str(), (UINT32)oss2.str().length(), pFormatP2, &p2Rect, pBrush);

        // Draw Player Side Ball Indicators (Only for 8-Ball)
        if (currentGameType == GameType::EIGHT_BALL_MODE) {
            if (player2Info.assignedType != BallType::NONE)
            {
                D2D1_POINT_2F ballCenter = D2D1::Point2F(p2Rect.left - 15.0f, p2Rect.top + uiHeight / 2.0f);
                float radius = 10.0f;
                D2D1_ELLIPSE ball = D2D1::Ellipse(ballCenter, radius, radius);

                if (IsPlayerOnEightBall(2))
                {
                    // Player is on the 8-ball, draw the 8-ball indicator.
                    ID2D1SolidColorBrush* p8BallBrush = nullptr;
                    pRT->CreateSolidColorBrush(EIGHT_BALL_COLOR, &p8BallBrush);
                    if (p8BallBrush) pRT->FillEllipse(&ball, p8BallBrush);
                    SafeRelease(&p8BallBrush);

                    // Draw the number '8' decal
                    ID2D1SolidColorBrush* pNumWhite = nullptr, * pNumBlack = nullptr;
                    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pNumWhite);
                    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pNumBlack);
                    if (pBallNumFormat && pNumWhite && pNumBlack)
                    {
                        const float decalR = radius * 0.7f;
                        D2D1_ELLIPSE decal = D2D1::Ellipse(ballCenter, decalR, decalR);
                        pRT->FillEllipse(&decal, pNumWhite);
                        pRT->DrawEllipse(&decal, pNumBlack, 0.8f);
                        wchar_t numText[] = L"8";
                        D2D1_RECT_F layout = { ballCenter.x - decalR, ballCenter.y - decalR, ballCenter.x + decalR, ballCenter.y + decalR };
                        pRT->DrawText(numText, (UINT32)wcslen(numText), pBallNumFormat, &layout, pNumBlack);
                    }

                    // If stripes, draw a stripe band
                    SafeRelease(&pNumWhite);
                    SafeRelease(&pNumBlack);
                }
                else
                {
                    // Default: Draw the player's assigned ball type (solid/stripe).
                    ID2D1SolidColorBrush* pBallBrush = nullptr;
                    D2D1_COLOR_F ballColor = (player2Info.assignedType == BallType::SOLID) ?
                        D2D1::ColorF(1.0f, 1.0f, 0.0f) : D2D1::ColorF(1.0f, 0.0f, 0.0f);
                    pRT->CreateSolidColorBrush(ballColor, &pBallBrush);
                    if (pBallBrush)
                    {
                        pRT->FillEllipse(&ball, pBallBrush);
                        SafeRelease(&pBallBrush);
                        if (player2Info.assignedType == BallType::STRIPE)
                        {
                            ID2D1SolidColorBrush* pStripeBrush = nullptr;
                            pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pStripeBrush);
                            if (pStripeBrush)
                            {
                                D2D1_RECT_F stripeRect = { ballCenter.x - radius, ballCenter.y - 3.0f, ballCenter.x + radius, ballCenter.y + 3.0f };
                                pRT->FillRectangle(&stripeRect, pStripeBrush);
                                SafeRelease(&pStripeBrush);
                            }
                        }
                    }
                }

                // Draw a border around the indicator ball, regardless of type.
                ID2D1SolidColorBrush* pBorderBrush = nullptr;
                pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
                if (pBorderBrush) pRT->DrawEllipse(&ball, pBorderBrush, 1.5f);
                SafeRelease(&pBorderBrush);
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
            arrowBackX = playerBox.left - 57.0f; //25.0f
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
    } // --- End else (not practice mode) ---

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
    // Draw if the logical foul flag is set OR if the display counter is active.
    if ((foulCommitted || foulDisplayCounter > 0) && currentGameState != SHOT_IN_PROGRESS) {
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

    // Decrement the FOUL display counter here (per-frame). Keep it >= 0.
    if (foulDisplayCounter > 0) foulDisplayCounter--;

    // --- Blue Arrow & Prompt for 8?Ball Call (while choosing or after called) ---
    // 8-Ball Pocket Call Arrow & Prompt (Only for 8-Ball)
       // --- FIX: Added !g_isPracticeMode to prevent this in practice mode ---
    if (currentGameType == GameType::EIGHT_BALL_MODE && !g_isPracticeMode) {
        // Check if AI called (display permanently if so)
        bool aiCalled = (isPlayer2AI && IsPlayerOnEightBall(2) && calledPocketP2 >= 0);
        // Check if human needs to choose or has chosen
        bool humanChoosingOrCalled = (!isPlayer2AI || currentPlayer == 1) &&
            (currentGameState == CHOOSING_POCKET_P1 || currentGameState == CHOOSING_POCKET_P2 ||
                calledPocketP1 >= 0 || calledPocketP2 >= 0);

        if (aiCalled || humanChoosingOrCalled) {
            int idx = -1;
            if (aiCalled) idx = calledPocketP2;
            else {
                idx = (currentPlayer == 1 ? calledPocketP1 : calledPocketP2);
                if (idx < 0) idx = 2; // Default if choosing
            }

            // draw large blue arrow
            ID2D1SolidColorBrush* pPocketArrowBrush = nullptr;
            pRT->CreateSolidColorBrush(TURN_ARROW_COLOR, &pPocketArrowBrush); // Use blue
            if (pPocketArrowBrush) {
                auto P = pocketPositions[idx];
                D2D1_POINT_2F tri[3] = { { P.x - 15.0f, P.y - 40.0f }, { P.x + 15.0f, P.y - 40.0f }, { P.x, P.y - 10.0f } };
                // Create and fill geometry (same as before)
                ID2D1PathGeometry* geom = nullptr;
                pFactory->CreatePathGeometry(&geom);
                ID2D1GeometrySink* sink = nullptr;
                if (geom && SUCCEEDED(geom->Open(&sink))) {
                    sink->BeginFigure(tri[0], D2D1_FIGURE_BEGIN_FILLED);
                    sink->AddLines(&tri[1], 2);
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();
                    pRT->FillGeometry(geom, pPocketArrowBrush);
                }
                SafeRelease(&sink); SafeRelease(&geom);
                SafeRelease(&pPocketArrowBrush);
            }

            // Draw prompt text below table
            D2D1_RECT_F txt = D2D1::RectF(TABLE_LEFT, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS + 5.0f, TABLE_RIGHT, TABLE_BOTTOM + WOODEN_RAIL_THICKNESS + 30.0f);
            std::wstring promptText = L"";
            if (aiCalled) promptText = L"AI has called pocket " + std::to_wstring(idx);
            else if (currentGameState == CHOOSING_POCKET_P1 || currentGameState == CHOOSING_POCKET_P2) promptText = L"Choose pocket for 8-Ball...";
            else if (calledPocketP1 >= 0 || calledPocketP2 >= 0) promptText = L"Pocket " + std::to_wstring(idx) + L" called for 8-Ball";

            if (!promptText.empty()) {
                pRT->DrawText(promptText.c_str(), (UINT32)promptText.length(), pTextFormat, &txt, pBrush);
            }
        }
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
    if (!g_isPracticeMode && currentGameState == AI_THINKING && pTextFormat) { // No "Thinking..." in practice
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
            pRT->DrawText(L"CHEAT MODE ON", static_cast<UINT32>(wcslen(L"CHEAT MODE ON")), pTextFormat, &cheatTextRect, pCheatBrush);
        }
        SafeRelease(&pCheatBrush);
    }
    // --- Draw DEBUG MODE label if active ---
    if (g_debugMode) {
        ID2D1SolidColorBrush* pDebugBrush = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &pDebugBrush);
        if (pDebugBrush && pTextFormat) {
            D2D1_RECT_F debugTextRect = D2D1::RectF(
                TABLE_LEFT + 10.0f,
                TABLE_TOP + 40.0f, // Below Cheat Mode text
                TABLE_LEFT + 200.0f,
                TABLE_TOP + 70.0f
            );
            pRT->DrawText(L"KIBITZER MODE", 13, pTextFormat, &debugTextRect, pDebugBrush);
        }
        SafeRelease(&pDebugBrush);
    }
    // [+] NEW: Draw Green Click Zones around pockets to verify detection
    /*ID2D1SolidColorBrush* pDebugZone = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Lime, 0.3f), &pDebugZone);
    if (pDebugZone) {
        for (int i = 0; i < 6; ++i) {
            float r = ((i == 1 || i == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS);
            // Draw the radius used in logic (sqrt(10) ~= 3.16x visual radius)
            float logicRadius = r * 3.16f;
            D2D1_ELLIPSE zone = D2D1::Ellipse(pocketPositions[i], logicRadius, logicRadius);
            pRT->DrawEllipse(&zone, pDebugZone, 2.0f);
        }
        SafeRelease(&pDebugZone);
    }*/
}

void DrawPowerMeter(ID2D1RenderTarget* pRT) {
    // --- MOVED DECLARATIONS TO TOP ---
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    ID2D1GradientStopCollection* pGradientStops = nullptr; // MOVED
    ID2D1LinearGradientBrush* pGradientBrush = nullptr;   // MOVED
    ID2D1SolidColorBrush* pNotchBrush = nullptr;
    ID2D1SolidColorBrush* pTextBrush = nullptr;
    ID2D1SolidColorBrush* pGlowBrush = nullptr;
    // --- END MOVED DECLARATIONS ---

    // Draw Border
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBorderBrush);
    if (!pBorderBrush) return;
    pRT->DrawRectangle(&powerMeterRect, pBorderBrush, 2.0f);
    // SafeRelease(&pBorderBrush); // Release later

    // Create Gradient Fill
    // ID2D1GradientStopCollection* pGradientStops = nullptr; // OLD POS
    // ID2D1LinearGradientBrush* pGradientBrush = nullptr;   // OLD POS
    D2D1_GRADIENT_STOP gradientStops[4];
    gradientStops[0].position = 0.0f;
    gradientStops[0].color = D2D1::ColorF(D2D1::ColorF::Green);
    gradientStops[1].position = 0.45f;
    gradientStops[1].color = D2D1::ColorF(D2D1::ColorF::Yellow);
    gradientStops[2].position = 0.7f;
    gradientStops[2].color = D2D1::ColorF(D2D1::ColorF::Orange);
    gradientStops[3].position = 1.0f;
    gradientStops[3].color = D2D1::ColorF(D2D1::ColorF::Red);

    pRT->CreateGradientStopCollection(gradientStops, 4, &pGradientStops); // Now assigns to the outer variable
    if (pGradientStops) {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {};
        props.startPoint = D2D1::Point2F(powerMeterRect.left, powerMeterRect.bottom);
        props.endPoint = D2D1::Point2F(powerMeterRect.left, powerMeterRect.top);
        pRT->CreateLinearGradientBrush(props, pGradientStops, &pGradientBrush); // Now assigns to the outer variable
        // SafeRelease(&pGradientStops); // Release later
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

    if (pGradientBrush) { // Check the outer variable
        pRT->FillRectangle(&fillRect, pGradientBrush);
        // SafeRelease(&pGradientBrush); // Release later
    }

    // Draw scale notches
    // ID2D1SolidColorBrush* pNotchBrush = nullptr; // OLD POS
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
        //SafeRelease(&pNotchBrush);
    }

    // Draw "Power" Label Below Meter
    if (pTextFormat) {
        // ID2D1SolidColorBrush* pTextBrush = nullptr; // OLD POS
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
            //SafeRelease(&pTextBrush);
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
        // ID2D1SolidColorBrush* pGlowBrush = nullptr; // OLD POS
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
            //SafeRelease(&pGlowBrush);
        }
    }
    // --- Release all brushes at the end ---
    SafeRelease(&pBorderBrush);
    SafeRelease(&pGradientStops); // Now safe to release here
    SafeRelease(&pGradientBrush); // Now safe to release here
    SafeRelease(&pNotchBrush);
    SafeRelease(&pTextBrush);
    SafeRelease(&pGlowBrush);
    // --- END Release ---
}

void DrawSpinIndicator(ID2D1RenderTarget* pRT) {
    // --- MOVED DECLARATIONS TO TOP ---
    ID2D1SolidColorBrush* pWhiteBrush = nullptr;
    ID2D1SolidColorBrush* pRedBrush = nullptr;
    float dotRadius = 4.0f; // MOVED
    float dotX = 0.0f;      // MOVED & initialized
    float dotY = 0.0f;      // MOVED & initialized
    // --- END MOVED DECLARATIONS ---

    pRT->CreateSolidColorBrush(CUE_BALL_COLOR, &pWhiteBrush);
    pRT->CreateSolidColorBrush(ENGLISH_DOT_COLOR, &pRedBrush);

    // --- MODIFIED CHECK ---
    // Only proceed if BOTH brushes were created successfully
    if (pWhiteBrush && pRedBrush) {
        // Draw White Ball Background
        D2D1_ELLIPSE bgEllipse = D2D1::Ellipse(spinIndicatorCenter, spinIndicatorRadius, spinIndicatorRadius);
        pRT->FillEllipse(&bgEllipse, pWhiteBrush);
        pRT->DrawEllipse(&bgEllipse, pRedBrush, 0.5f); // Thin red border

        // Draw Red Dot for Spin Position
        // float dotRadius = 4.0f; // OLD POS
        dotX = spinIndicatorCenter.x + cueSpinX * (spinIndicatorRadius - dotRadius); // Keep dot inside edge // FIXED: Use outer var
        dotY = spinIndicatorCenter.y + cueSpinY * (spinIndicatorRadius - dotRadius); // FIXED: Use outer var
        D2D1_ELLIPSE dotEllipse = D2D1::Ellipse(D2D1::Point2F(dotX, dotY), dotRadius, dotRadius); // FIXED: Use outer vars
        pRT->FillEllipse(&dotEllipse, pRedBrush); // FIXED: Use outer var
    }
    // --- END MODIFIED CHECK ---

    // Release brushes outside the block, SafeRelease handles nullptrs
    SafeRelease(&pWhiteBrush); // FIXED: Now releases correctly
    SafeRelease(&pRedBrush);   // FIXED: Now releases correctly
}


void DrawPocketedBallsIndicator(ID2D1RenderTarget* pRT) {
    // --- MOVED DECLARATIONS TO TOP ---
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    ID2D1SolidColorBrush* pBallBrush = nullptr;
    D2D1_ROUNDED_RECT roundedRect; // MOVED
    float finalAlpha = 0.8f;       // MOVED & initialized
    // --- END MOVED DECLARATIONS ---


    // Ensure render target is valid before proceeding
    if (!pRT) return;

    HRESULT hr = pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.8f), &pBgBrush);
    if (FAILED(hr)) { SafeRelease(&pBgBrush); return; }

    hr = pRT->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &pBallBrush);
    if (FAILED(hr)) {
        SafeRelease(&pBgBrush);
        SafeRelease(&pBallBrush);
        return;
    }

    // --- Use outer scope variables ---
    roundedRect = D2D1::RoundedRect(pocketedBallsBarRect, 10.0f, 10.0f); // FIXED
    float baseAlpha = 0.8f;
    float flashBoost = pocketFlashTimer * 0.5f;
    finalAlpha = std::min(1.0f, baseAlpha + flashBoost); // FIXED
    pBgBrush->SetOpacity(finalAlpha); // FIXED
    pRT->FillRoundedRectangle(&roundedRect, pBgBrush); // FIXED
    pBgBrush->SetOpacity(1.0f);
    // --- End use outer scope variables ---


    // ... (Calculation of ballDisplayRadius, spacing, padding, center_Y remains the same) ...
    float barHeight = pocketedBallsBarRect.bottom - pocketedBallsBarRect.top;
    float ballDisplayRadius = barHeight * 0.30f;
    float spacing = ballDisplayRadius * 2.2f;
    float padding = spacing * 0.75f;
    float center_Y = pocketedBallsBarRect.top + barHeight / 2.0f;
    float currentX_P1 = pocketedBallsBarRect.left + padding;
    float currentX_P2 = pocketedBallsBarRect.right - padding;
    int p1DrawnCount = 0;
    int p2DrawnCount = 0;

    // --- FIX: Split logic by game type ---
    if (currentGameType == GameType::EIGHT_BALL_MODE) {
        const int maxBallsToShow = 7;
        for (const auto& b : balls) {
            if (b.isPocketed) {
                if (b.id == 0 || b.id == 8) continue;

                bool isPlayer1Ball = (player1Info.assignedType != BallType::NONE && b.type == player1Info.assignedType);
                bool isPlayer2Ball = (player2Info.assignedType != BallType::NONE && b.type == player2Info.assignedType);

                if (isPlayer1Ball && p1DrawnCount < maxBallsToShow) {
                    pBallBrush->SetColor(b.color);
                    D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(D2D1::Point2F(currentX_P1 + p1DrawnCount * spacing, center_Y), ballDisplayRadius, ballDisplayRadius);
                    pRT->FillEllipse(&ballEllipse, pBallBrush);
                    p1DrawnCount++;
                }
                else if (isPlayer2Ball && p2DrawnCount < maxBallsToShow) {
                    pBallBrush->SetColor(b.color);
                    D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(D2D1::Point2F(currentX_P2 - p2DrawnCount * spacing, center_Y), ballDisplayRadius, ballDisplayRadius);
                    pRT->FillEllipse(&ballEllipse, pBallBrush);
                    p2DrawnCount++;
                }
            }
        }
    }
    // --- NEW LOGIC FOR 9-BALL (and
    else if (currentGameType == GameType::NINE_BALL || currentGameType == GameType::STRAIGHT_POOL) {
        const int maxBallsToShow = 15; // Max possible
        int drawnCount = 0;

        // Loop from 1 to 15 to draw them in numerical order
        for (int id = 1; id <= maxBallsToShow; ++id) {
            // For 9-Ball, stop after 9
            if (currentGameType == GameType::NINE_BALL && id > 9) {
                break;
            }

            Ball* b = GetBallById(id);
            if (b && b->isPocketed) {
                pBallBrush->SetColor(b->color);
                D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(D2D1::Point2F(currentX_P1 + drawnCount * spacing, center_Y), ballDisplayRadius, ballDisplayRadius);
                pRT->FillEllipse(&ballEllipse, pBallBrush);
                drawnCount++;
            }
        }
    }
    // --- END FIX ---

    SafeRelease(&pBgBrush);    // FIXED
    SafeRelease(&pBallBrush); // FIXED
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
    if (g_isPracticeMode) return; // No pocket selection in practice mode
    if (!pFactory) return; // Need factory to create geometry

    int pocketToIndicate = -1;
    if (currentGameState == CHOOSING_POCKET_P1 || (currentPlayer == 1 && IsPlayerOnEightBall(1))) {
        pocketToIndicate = (calledPocketP1 >= 0) ? calledPocketP1 : currentlyHoveredPocket;
    }
    else if (currentGameState == CHOOSING_POCKET_P2 || (currentPlayer == 2 && IsPlayerOnEightBall(2))) {
        pocketToIndicate = (calledPocketP2 >= 0) ? calledPocketP2 : currentlyHoveredPocket;
    }

    if (pocketToIndicate < 0 || pocketToIndicate > 5) {
        return;
    }

    ID2D1SolidColorBrush* pArrowBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, 0.9f), &pArrowBrush);
    if (!pArrowBrush) return;

    // FIXED: Get the correct radius for the indicated pocket
    float currentPocketRadius = (pocketToIndicate == 1 || pocketToIndicate == 4) ? MIDDLE_HOLE_VISUAL_RADIUS : CORNER_HOLE_VISUAL_RADIUS;

    D2D1_POINT_2F targetPocketCenter = pocketPositions[pocketToIndicate];
    // FIXED: Calculate arrow dimensions based on the correct pocket radius
    float pocketVis = GetPocketVisualRadius(pocketToIndicate);
    float arrowHeadSize = pocketVis * 0.5f;
    float arrowShaftLength = pocketVis * 0.3f;
    float arrowShaftWidth = arrowHeadSize * 0.4f;
    float verticalOffsetFromPocketCenter = pocketVis * 1.6f;

    D2D1_POINT_2F tip, baseLeft, baseRight, shaftTopLeft, shaftTopRight, shaftBottomLeft, shaftBottomRight;

    // Determine arrow direction based on pocket position (top or bottom row)
    if (targetPocketCenter.y == TABLE_TOP) { // Top row pockets
        tip = D2D1::Point2F(targetPocketCenter.x, targetPocketCenter.y - verticalOffsetFromPocketCenter - arrowHeadSize);
        baseLeft = D2D1::Point2F(targetPocketCenter.x - arrowHeadSize / 2.0f, targetPocketCenter.y - verticalOffsetFromPocketCenter);
        baseRight = D2D1::Point2F(targetPocketCenter.x + arrowHeadSize / 2.0f, targetPocketCenter.y - verticalOffsetFromPocketCenter);
        shaftTopLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y + arrowShaftLength);
        shaftTopRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y + arrowShaftLength);
        shaftBottomLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y);
        shaftBottomRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y);
    }
    else { // Bottom row pockets
        tip = D2D1::Point2F(targetPocketCenter.x, targetPocketCenter.y + verticalOffsetFromPocketCenter + arrowHeadSize);
        baseLeft = D2D1::Point2F(targetPocketCenter.x - arrowHeadSize / 2.0f, targetPocketCenter.y + verticalOffsetFromPocketCenter);
        baseRight = D2D1::Point2F(targetPocketCenter.x + arrowHeadSize / 2.0f, targetPocketCenter.y + verticalOffsetFromPocketCenter);
        shaftTopLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y);
        shaftTopRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y);
        shaftBottomLeft = D2D1::Point2F(targetPocketCenter.x - arrowShaftWidth / 2.0f, baseLeft.y - arrowShaftLength);
        shaftBottomRight = D2D1::Point2F(targetPocketCenter.x + arrowShaftWidth / 2.0f, baseRight.y - arrowShaftLength);
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