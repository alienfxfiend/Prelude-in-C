/*
  Battlezone (1980 vector-arcade) *inspired* mini-port scaffold — FINAL ALL-IN-ONE
  Win32 + GDI (Dev-C++ / MinGW friendly)

  Features:
  - Black background, lime-green wireframe “3D”
  - LEFT/RIGHT rotate, UP/DOWN move forward/back
  - SPACE fires projectile (dot)
  - Enemy tanks advance + shoot
  - HUD: enemy bearing + damage/score/time/enemy count + GOD MODE indicator
  - GOD MODE toggle (global): press G
  - Infinite roaming: props + enemies recycle ahead of you forever
  - Ground grid is generated in CAMERA SPACE so it never “disappears when turning”
  - HUD frame is outline-only (fixes white-filled block issue)
  - Window starts maximized

  Build (MinGW / Dev-C++):
    g++ -std=gnu++17 -O2 -Wall -Wextra -mwindows battlezone_gdi.cpp -lgdi32 -luser32
*/

#ifndef UNICODE
  #define UNICODE
  #define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>

static constexpr COLORREF LIME = RGB(0, 255, 0);

// Global God Mode toggle: press 'G' to toggle.
static bool g_GodMode = true;

// ------------------------------ Math ------------------------------

struct Vec3 {
  float x{}, y{}, z{};
  Vec3() = default;
  Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
  Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
  Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
};

static inline float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float Len2(const Vec3& v) { return Dot(v,v); }
static inline float Len(const Vec3& v) { return std::sqrt(Len2(v)); }

static inline Vec3 Normalize(const Vec3& v) {
  float l = Len(v);
  if (l <= 1e-6f) return {0,0,0};
  return v*(1.0f/l);
}

static inline Vec3 RotateY(const Vec3& v, float a) {
  float c = std::cos(a), s = std::sin(a);
  return { v.x*c + v.z*s, v.y, -v.x*s + v.z*c };
}

// ------------------------------ Mesh ------------------------------

struct Edge { uint16_t a, b; };

struct Mesh {
  std::vector<Vec3> v;
  std::vector<Edge> e;
};

static Mesh MakeBox(float hx, float hy, float hz) {
  Mesh m;
  m.v = {
    {-hx,-hy,-hz}, {hx,-hy,-hz}, {hx,hy,-hz}, {-hx,hy,-hz},
    {-hx,-hy, hz}, {hx,-hy, hz}, {hx,hy, hz}, {-hx,hy, hz}
  };
  m.e = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
  };
  return m;
}

static Mesh MakePyramid(float h, float r) {
  Mesh m;
  m.v = { {0,h,0}, {-r,0,-r}, {r,0,-r}, {r,0,r}, {-r,0,r} };
  m.e = { {1,2},{2,3},{3,4},{4,1}, {0,1},{0,2},{0,3},{0,4} };
  return m;
}

// A crude “Battlezone-like” tank: two boxes + gun line.
static Mesh MakeTank() {
  Mesh base = MakeBox(1.1f, 0.35f, 1.6f);
  Mesh top  = MakeBox(0.7f, 0.25f, 0.9f);

  Mesh m;
  m.v = base.v;
  m.e = base.e;

  // append turret vertices with offset
  uint16_t off = (uint16_t)m.v.size();
  for (auto vv : top.v) {
    vv.y += 0.55f;
    m.v.push_back(vv);
  }
  for (auto ee : top.e) m.e.push_back({ (uint16_t)(ee.a + off), (uint16_t)(ee.b + off) });

  // gun: line from turret front center forward
  uint16_t gunA = (uint16_t)m.v.size();
  m.v.push_back({0.0f, 0.55f, 0.9f});   // turret front-ish
  uint16_t gunB = (uint16_t)m.v.size();
  m.v.push_back({0.0f, 0.55f, 2.4f});   // barrel end
  m.e.push_back({gunA, gunB});

  return m;
}

// ------------------------------ BackBuffer (GDI double buffer) ------------------------------

class BackBuffer {
public:
  void Resize(HDC screenDC, int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (dc_)  { DeleteDC(dc_); dc_ = nullptr; }
    if (bmp_) { DeleteObject(bmp_); bmp_ = nullptr; }
    bits_ = nullptr;

    w_ = w; h_ = h;
    dc_ = CreateCompatibleDC(screenDC);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w_;
    bi.bmiHeader.biHeight      = -h_; // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    bmp_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
    SelectObject(dc_, bmp_);
  }

  ~BackBuffer() {
    if (dc_)  DeleteDC(dc_);
    if (bmp_) DeleteObject(bmp_);
  }

  HDC  DC() const { return dc_; }
  int  W()  const { return w_; }
  int  H()  const { return h_; }

private:
  HDC dc_{};
  HBITMAP bmp_{};
  void* bits_{};
  int w_{}, h_{};
};

// ------------------------------ Game Objects ------------------------------

struct Projectile {
  Vec3 pos;
  Vec3 vel;
  bool fromEnemy{};
  bool alive{true};
};

struct Prop {
  Vec3 pos;
  float yaw{};
  const Mesh* mesh{};
};

struct EnemyTank {
  Vec3 pos;
  float yaw{};
  bool alive{true};
  float fireCooldown{};
  float speed{6.0f};
};

// ------------------------------ Renderer: camera + projection ------------------------------

struct Camera {
  Vec3 pos;
  float yaw{};
  float nearZ{0.25f};
};

struct Projector {
  int w{}, h{};
  float scale{};
  int cx{}, cy{};
};

static inline Vec3 WorldToCamera(const Camera& cam, const Vec3& p) {
  Vec3 rel = p - cam.pos;
  return RotateY(rel, -cam.yaw);
}

static bool ClipToNear(const float nearZ, Vec3& a, Vec3& b) {
  if (a.z >= nearZ && b.z >= nearZ) return true;
  if (a.z <  nearZ && b.z <  nearZ) return false;

  const float t = (nearZ - a.z) / (b.z - a.z);
  Vec3 i = a + (b - a) * t;

  if (a.z < nearZ) a = i;
  else             b = i;
  return true;
}

static inline POINT Project(const Projector& pj, const Vec3& camSpace) {
  float invz = 1.0f / camSpace.z;
  int sx = pj.cx + (int)std::lround(camSpace.x * pj.scale * invz);
  int sy = pj.cy - (int)std::lround(camSpace.y * pj.scale * invz);
  return { sx, sy };
}

// ------------------------------ Game Core ------------------------------

class Game {
public:
  explicit Game(HWND hwnd)
    : hwnd_(hwnd),
      rng_((uint32_t)GetTickCount()) {

    tankMesh_ = MakeTank();
    boxMesh_  = MakeBox(1.2f, 1.2f, 1.2f);
    pyrMesh_  = MakePyramid(2.2f, 1.4f);

    Reset();
  }

  void OnResize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    HDC screen = GetDC(hwnd_);
    back_.Resize(screen, w, h);
    ReleaseDC(hwnd_, screen);

    pj_.w = w; pj_.h = h;
    pj_.cx = w / 2;
    pj_.cy = (int)(h * 0.60f);
    pj_.scale = std::min(w, h) * 0.85f;
  }

  void OnKeyDown(WPARAM vk) {
    if (vk < 256) keys_[vk] = true;
  }

  void OnKeyUp(WPARAM vk) {
    if (vk < 256) keys_[vk] = false;
  }

  void OnChar(wchar_t ch) {
    if (ch == L'r' || ch == L'R') Reset();
    if (ch == L'g' || ch == L'G') g_GodMode = !g_GodMode;
  }

  void Update(float dt) {
    if (dt <= 0) return;

    if (!running_) return;

    time_ += dt;

    // Rotation
    const float rotSpeed = 2.0f;
    if (keys_[VK_LEFT])  cam_.yaw -= rotSpeed * dt;
    if (keys_[VK_RIGHT]) cam_.yaw += rotSpeed * dt;

    // Movement
    const float moveSpeed = 12.0f;
    Vec3 forward = Forward();
    if (keys_[VK_UP])   cam_.pos = cam_.pos + forward * (moveSpeed * dt);
    if (keys_[VK_DOWN]) cam_.pos = cam_.pos - forward * (moveSpeed * dt);

    // Infinite world streaming (props/enemies recycle forever)
    RecycleWorld();

    // Fire projectile on SPACE edge
    bool space = keys_[VK_SPACE];
    if (space && !spaceWasDown_) FirePlayer();
    spaceWasDown_ = space;

    UpdateProjectiles(dt);
    UpdateEnemies(dt);
    ResolveCollisions();

    // Keep pressure constant (reuses pool)
    MaintainEnemyCount();
  }

  void RenderTo(HDC targetDC) {
    if (!back_.DC()) return;

    HDC dc = back_.DC();

    // clear to black
    RECT rc{0,0, back_.W(), back_.H()};
    FillRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    HPEN pen = CreatePen(PS_SOLID, 1, LIME);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, LIME);

    // Camera-space ground grid (never disappears when turning)
    DrawGroundGrid(dc);

    // Props
    for (const auto& p : props_) {
      DrawMesh(dc, *p.mesh, p.pos, p.yaw);
    }

    // Enemies
    for (const auto& e : enemies_) {
      if (!e.alive) continue;
      DrawMesh(dc, tankMesh_, e.pos, e.yaw);
    }

    // Projectiles
    for (const auto& pr : shots_) {
      if (!pr.alive) continue;
      DrawDot3D(dc, pr.pos, 2);
    }

    // Crosshair brackets
    DrawCrosshair(dc);

    // HUD
    DrawHUD(dc);

    // Game over overlay
    if (!running_) DrawGameOver(dc);

    SelectObject(dc, oldPen);
    DeleteObject(pen);

    BitBlt(targetDC, 0, 0, back_.W(), back_.H(), back_.DC(), 0, 0, SRCCOPY);
  }

  void Reset() {
    running_ = true;
    score_ = 0;
    damage_ = 0;
    time_ = 0;

    cam_.pos = {0, 1.2f, 0};
    cam_.yaw = 0;
    cam_.nearZ = 0.25f;

    shots_.clear();
    enemies_.clear();
    props_.clear();

    BuildWorld();
    SpawnEnemies(32);   // match the new desiredAlive count
  }

private:
  // ----- Infinite-world helpers -----

  Vec3 Forward() const { return RotateY({0,0,1}, cam_.yaw); }
  Vec3 Right()   const { return RotateY({1,0,0}, cam_.yaw); }

  float Rand(float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(rng_);
  }

  int RandInt(int a, int b) {
    std::uniform_int_distribution<int> d(a, b);
    return d(rng_);
  }

  void SpawnPropAhead(Prop& p) {
    // Spawn in front of the camera in a band.
    // Denser and more visible: tighter X band (more stays in view), deeper Z band (more on screen).
    float x = Rand(-140.0f, 140.0f);
    float z = Rand(30.0f, 700.0f);
    float yaw = Rand(0.0f, 6.28318f);
    int type = RandInt(0, 2);

    Vec3 w = cam_.pos + Right()*x + Forward()*z;
    w.y = 0.0f;

    p.pos = w;
    p.yaw = yaw;
    p.mesh = (type == 0) ? &boxMesh_ : &pyrMesh_;
  }

  void SpawnEnemyAhead(EnemyTank& t) {
    // More enemies closer/within view: wider X, closer Z start.
    float x = Rand(-120.0f, 120.0f);
    float z = Rand(80.0f, 800.0f);

    Vec3 w = cam_.pos + Right()*x + Forward()*z;
    w.y = 0.0f;

    t.pos = w;
    t.yaw = 3.14159f;
    t.alive = true;
    t.speed = 6.0f;
    t.fireCooldown = Rand(0.8f, 2.2f);
  }

  void RecycleWorld() {
    RecycleProps();
    RecycleEnemies();
  }

  void RecycleProps() {
    // Reposition props that are far behind/off to the sides, or absurdly far away.
    for (auto& p : props_) {
      Vec3 c = WorldToCamera(cam_, p.pos);
      // Keep the prop cloud concentrated around the player's view volume.
      if (c.z < -80.0f || c.z > 900.0f || std::fabs(c.x) > 240.0f) {
        SpawnPropAhead(p);
      }
    }
  }

  void RecycleEnemies() {
    // Recycle far-behind/far-off enemies so the fight stays around the player.
    for (auto& e : enemies_) {
      if (!e.alive) continue;
      Vec3 c = WorldToCamera(cam_, e.pos);
      // Keep enemies closer so you see more of them at once.
      if (c.z < -140.0f || c.z > 1100.0f || std::fabs(c.x) > 300.0f) {
        SpawnEnemyAhead(e);
      }
    }
  }

  // ----- World population -----

  void BuildWorld() {
    // Fixed prop pool that is recycled forever.
    const int kPropCount = 220;  // MUCH denser world
    props_.resize(kPropCount);
    for (auto& p : props_) SpawnPropAhead(p);
  }

  void SpawnEnemies(int n) {
    enemies_.reserve(enemies_.size() + (size_t)n);
    for (int i = 0; i < n; ++i) {
      EnemyTank t;
      SpawnEnemyAhead(t);
      t.fireCooldown += i * 0.25f; // stagger
      enemies_.push_back(t);
    }
  }

  void MaintainEnemyCount() {
    const int desiredAlive = 32; // MANY more tanks active
    int alive = 0;
    for (auto& e : enemies_) if (e.alive) ++alive;

    // Respawn dead slots first
    for (auto& e : enemies_) {
      if (alive >= desiredAlive) break;
      if (!e.alive) {
        SpawnEnemyAhead(e);
        ++alive;
      }
    }

    // If still not enough, extend pool (rare after first run)
    while (alive < desiredAlive) {
      EnemyTank t;
      SpawnEnemyAhead(t);
      enemies_.push_back(t);
      ++alive;
    }
  }

  // ----- Combat -----

  void FirePlayer() {
    Vec3 forward = Forward();
    Projectile p;
    p.fromEnemy = false;
    p.pos = cam_.pos + forward * 2.3f + Vec3{0.0f, 0.2f, 0.0f};
    p.vel = forward * 60.0f;
    p.alive = true;
    shots_.push_back(p);
  }

  void FireEnemy(EnemyTank& e) {
    Vec3 dir = Normalize(cam_.pos - e.pos);
    Projectile p;
    p.fromEnemy = true;
    p.pos = e.pos + dir * 2.0f + Vec3{0.0f, 0.4f, 0.0f};
    p.vel = dir * 45.0f;
    p.alive = true;
    shots_.push_back(p);
  }

  void UpdateProjectiles(float dt) {
    for (auto& p : shots_) {
      if (!p.alive) continue;
      p.pos = p.pos + p.vel * dt;

      // cull if too far from player (prevents infinite growth)
      Vec3 rel = p.pos - cam_.pos;
      if (std::fabs(rel.x) > 600.0f || std::fabs(rel.z) > 900.0f) p.alive = false;
    }

    if (shots_.size() > 512) {
      shots_.erase(std::remove_if(shots_.begin(), shots_.end(),
                   [](const Projectile& p){ return !p.alive; }), shots_.end());
    }
  }

  void UpdateEnemies(float dt) {
    for (auto& e : enemies_) {
      if (!e.alive) continue;

      Vec3 toPlayer = cam_.pos - e.pos;
      float dist = Len(toPlayer);

      // face player
      e.yaw = std::atan2(toPlayer.x, toPlayer.z);

      // move toward player
      if (dist > 12.0f) {
        Vec3 dir = Normalize(toPlayer);
        e.pos = e.pos + Vec3{dir.x, 0.0f, dir.z} * (e.speed * dt);
      }

      // fire logic
      e.fireCooldown -= dt;
      if (e.fireCooldown <= 0.0f && dist < 220.0f) {
        FireEnemy(e);
        e.fireCooldown = 1.8f + (float)(fmod(time_, 1.0)) * 0.7f;
      }
    }
  }

  void ResolveCollisions() {
    if (!running_) return;

    // Player hit by enemy projectiles
    const float playerHitR2 = 2.2f * 2.2f;
    for (auto& p : shots_) {
      if (!p.alive || !p.fromEnemy) continue;
      Vec3 d = p.pos - cam_.pos;
      if (Len2(d) < playerHitR2) {
        p.alive = false;
        damage_ += 15;

        if (!g_GodMode && damage_ >= 100) {
          running_ = false;
        }
      }
    }

    // Enemy hit by player projectiles
    const float enemyHitR2 = 2.8f * 2.8f;
    for (auto& p : shots_) {
      if (!p.alive || p.fromEnemy) continue;
      for (auto& e : enemies_) {
        if (!e.alive) continue;
        Vec3 d = p.pos - e.pos;
        if (Len2(d) < enemyHitR2) {
          p.alive = false;
          e.alive = false;
          score_ += 100;
          break;
        }
      }
    }
  }

  // ----- Drawing -----

  void DrawMesh(HDC dc, const Mesh& mesh, const Vec3& pos, float yaw) {
    std::vector<Vec3> camv;
    camv.reserve(mesh.v.size());

    for (const auto& lv : mesh.v) {
      Vec3 wv = RotateY(lv, yaw) + pos;
      Vec3 cv = WorldToCamera(cam_, wv);
      camv.push_back(cv);
    }

    for (const auto& ed : mesh.e) {
      Vec3 a = camv[ed.a];
      Vec3 b = camv[ed.b];
      if (!ClipToNear(cam_.nearZ, a, b)) continue;

      POINT pa = Project(pj_, a);
      POINT pb = Project(pj_, b);

      if ((pa.x < -2000 && pb.x < -2000) || (pa.x > pj_.w+2000 && pb.x > pj_.w+2000)) continue;
      if ((pa.y < -2000 && pb.y < -2000) || (pa.y > pj_.h+2000 && pb.y > pj_.h+2000)) continue;

      MoveToEx(dc, pa.x, pa.y, nullptr);
      LineTo(dc, pb.x, pb.y);
    }
  }

  void DrawDot3D(HDC dc, const Vec3& worldPos, int sizePx) {
    Vec3 c = WorldToCamera(cam_, worldPos);
    if (c.z < cam_.nearZ) return;
    POINT p = Project(pj_, c);
    RECT r{ p.x - sizePx, p.y - sizePx, p.x + sizePx + 1, p.y + sizePx + 1 };
    HBRUSH b = CreateSolidBrush(LIME);
    FillRect(dc, &r, b);
    DeleteObject(b);
  }

  // camera-space line drawing
  void DrawCameraLine(HDC dc, Vec3 a, Vec3 b) {
    if (!ClipToNear(cam_.nearZ, a, b)) return;
    POINT pa = Project(pj_, a);
    POINT pb = Project(pj_, b);
    MoveToEx(dc, pa.x, pa.y, nullptr);
    LineTo(dc, pb.x, pb.y);
  }
  
    // World-space line drawing (world -> camera -> clip -> project)
  void DrawWorldLine(HDC dc, const Vec3& aW, const Vec3& bW) {
    Vec3 a = WorldToCamera(cam_, aW);
    Vec3 b = WorldToCamera(cam_, bW);
    DrawCameraLine(dc, a, b);
  }

  void DrawGroundGrid(HDC dc) {
        // WORLD-SPACE grid (anchored to world): this makes the ground visibly "scroll"
    // when you move, so it feels like real motion. We generate a grid region
    // around the camera so it never disappears when turning.

    const float y = 0.0f; // world ground plane
    const float rangeX_far  = 160.0f; // far half-width
    const float rangeZ_far  = 950.0f; // far depth

    // VERY fine "micro-grid" region close to the player (tiny squares)
    const float rangeX_micro = 90.0f; //110.0f;
    const float rangeZ_micro = 150.0f; //180.0f;
    const float stepMicro    = 3.0f;   // <<< make smaller (e.g. 2.0f) for even tinier squares (more lines)

    // Medium grid beyond micro region (prevents the whole screen becoming line soup)
    const float rangeZ_mid = 340.0f;
    const float stepMid    = 12.0f;

    // Far grid (sparse)
    const float stepFarZ   = 60.0f;
    const float stepFarX   = 14.0f;

    auto floorToStep = [](float v, float s) -> float {
      return std::floor(v / s) * s;
    };

    const float camX = cam_.pos.x;
    const float camZ = cam_.pos.z;

        // X-constant lines (far scaffold)
    float x0f = floorToStep(camX - rangeX_far, stepFarX);
    float x1f = floorToStep(camX + rangeX_far, stepFarX) + stepFarX;
    for (float x = x0f; x <= x1f; x += stepFarX) {
      DrawWorldLine(dc, Vec3{x, y, camZ - rangeZ_far}, Vec3{x, y, camZ + rangeZ_far});
    }

    // X-constant lines (micro-grid close to player: tiny squares)
    float x0m = floorToStep(camX - rangeX_micro, stepMicro);
    float x1m = floorToStep(camX + rangeX_micro, stepMicro) + stepMicro;
    for (float x = x0m; x <= x1m; x += stepMicro) {
      DrawWorldLine(dc, Vec3{x, y, camZ - rangeZ_micro}, Vec3{x, y, camZ + rangeZ_micro});
    }

    // Z-constant lines (far, sparse)
    float z0f = floorToStep(camZ - rangeZ_far, stepFarZ);
    float z1f = floorToStep(camZ + rangeZ_far, stepFarZ) + stepFarZ;
    for (float z = z0f; z <= z1f; z += stepFarZ) {
      DrawWorldLine(dc, Vec3{camX - rangeX_far, y, z}, Vec3{camX + rangeX_far, y, z});
    }

    // Z-constant lines (mid)
    float z0mid = floorToStep(camZ - rangeZ_mid, stepMid);
    float z1mid = floorToStep(camZ + rangeZ_mid, stepMid) + stepMid;
    for (float z = z0mid; z <= z1mid; z += stepMid) {
      DrawWorldLine(dc, Vec3{camX - rangeX_far, y, z}, Vec3{camX + rangeX_far, y, z});
    }

    // Z-constant lines (micro-grid close to player: tiny squares)
    float z0m = floorToStep(camZ - rangeZ_micro, stepMicro);
    float z1m = floorToStep(camZ + rangeZ_micro, stepMicro) + stepMicro;
    for (float z = z0m; z <= z1m; z += stepMicro) {
      DrawWorldLine(dc, Vec3{camX - rangeX_micro, y, z}, Vec3{camX + rangeX_micro, y, z});
    }
  }

  void DrawCrosshair(HDC dc) {
    const int cx = pj_.cx;
    const int cy = pj_.cy;

    // Bracket crosshair: 4 right-angle corners
    const int halfBox = 44;
    const int leg     = 14;

    const int left   = cx - halfBox;
    const int right  = cx + halfBox;
    const int top    = cy - halfBox;
    const int bottom = cy + halfBox;

    // Top-left (+)
    MoveToEx(dc, left, top + leg, nullptr);   LineTo(dc, left, top);
    MoveToEx(dc, left, top,       nullptr);   LineTo(dc, left + leg, top);

    // Top-right (+)
    MoveToEx(dc, right - leg, top, nullptr);  LineTo(dc, right, top);
    MoveToEx(dc, right, top, nullptr);        LineTo(dc, right, top + leg);

    // Bottom-left (+)
    MoveToEx(dc, left, bottom - leg, nullptr); LineTo(dc, left, bottom);
    MoveToEx(dc, left, bottom, nullptr);       LineTo(dc, left + leg, bottom);

    // Bottom-right (+)
    MoveToEx(dc, right - leg, bottom, nullptr); LineTo(dc, right, bottom);
    MoveToEx(dc, right, bottom - leg, nullptr); LineTo(dc, right, bottom);
  }

  std::wstring EnemyBearingText() const {
    const EnemyTank* best = nullptr;
    float bestD2 = 1e30f;

    for (const auto& e : enemies_) {
      if (!e.alive) continue;
      float d2 = Len2(e.pos - cam_.pos);
      if (d2 < bestD2) { bestD2 = d2; best = &e; }
    }
    if (!best) return L"ENEMY: NONE";

    Vec3 c = WorldToCamera(cam_, best->pos);
    float ang = std::atan2(c.x, c.z);
    if (ang < -0.25f) return L"ENEMY: LEFT";
    if (ang >  0.25f) return L"ENEMY: RIGHT";
    return L"ENEMY: AHEAD";
  }

  void DrawHUD(HDC dc) {
    int alive = 0;
    for (auto& e : enemies_) if (e.alive) ++alive;

    std::wstring top = EnemyBearingText();

    std::wstring line1 =
      L"DAMAGE: " + std::to_wstring(damage_) + L"%   SCORE: " + std::to_wstring(score_) +
      L"   TIME: " + std::to_wstring((int)time_) + L"s   ENEMIES: " + std::to_wstring(alive) +
      (g_GodMode ? L"   [GOD MODE: ON]" : L"   [G: GOD MODE]");

    SIZE sz{};
    GetTextExtentPoint32W(dc, top.c_str(), (int)top.size(), &sz);
    TextOutW(dc, (pj_.w - sz.cx)/2, 10, top.c_str(), (int)top.size());
    TextOutW(dc, 10, 34, line1.c_str(), (int)line1.size());

    // HUD frame: outline only (fixes the white filled rectangle)
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, 6, 6, pj_.w - 6, 60);
    SelectObject(dc, oldBrush);
  }

  void DrawGameOver(HDC dc) {
    std::wstring s1 = L"GAME OVER";
    std::wstring s2 = L"Press R to Restart";

    HFONT font = CreateFontW(48,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                             DEFAULT_PITCH|FF_SWISS,L"Consolas");
    HGDIOBJ oldF = SelectObject(dc, font);

    SIZE a{};
    GetTextExtentPoint32W(dc, s1.c_str(), (int)s1.size(), &a);
    TextOutW(dc, (pj_.w - a.cx)/2, (pj_.h/2) - 60, s1.c_str(), (int)s1.size());

    HFONT font2 = CreateFontW(20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                              DEFAULT_PITCH|FF_SWISS,L"Consolas");
    SelectObject(dc, font2);

    SIZE b{};
    GetTextExtentPoint32W(dc, s2.c_str(), (int)s2.size(), &b);
    TextOutW(dc, (pj_.w - b.cx)/2, (pj_.h/2) + 5, s2.c_str(), (int)s2.size());

    SelectObject(dc, oldF);
    DeleteObject(font2);
    DeleteObject(font);
  }

private:
  HWND hwnd_{};
  BackBuffer back_;
  Projector pj_{};
  Camera cam_{};

  Mesh tankMesh_{}, boxMesh_{}, pyrMesh_{};

  std::vector<Prop> props_;
  std::vector<EnemyTank> enemies_;
  std::vector<Projectile> shots_;

  std::mt19937 rng_;

  bool keys_[256]{};
  bool spaceWasDown_{false};

  bool running_{true};
  int score_{0};
  int damage_{0};
  float time_{0.0f};
};

// ------------------------------ Win32 Loop ------------------------------

static Game* g_game = nullptr;

static uint64_t QpcNow() {
  LARGE_INTEGER x{};
  QueryPerformanceCounter(&x);
  return (uint64_t)x.QuadPart;
}

static double QpcFreq() {
  LARGE_INTEGER f{};
  QueryPerformanceFrequency(&f);
  return (double)f.QuadPart;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_SIZE:
      if (g_game) g_game->OnResize(LOWORD(lp), HIWORD(lp));
      return 0;

    case WM_ERASEBKGND:
      return 1; // we draw the whole frame

    case WM_KEYDOWN:
      if (g_game) g_game->OnKeyDown(wp);
      return 0;

    case WM_KEYUP:
      if (g_game) g_game->OnKeyUp(wp);
      return 0;

    case WM_CHAR:
      if (g_game) g_game->OnChar((wchar_t)wp);
      return 0;

    case WM_PAINT: {
      PAINTSTRUCT ps{};
      HDC hdc = BeginPaint(hwnd, &ps);
      if (g_game) g_game->RenderTo(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
  const wchar_t* CLASS_NAME = L"BattlezoneWireframeGDI";

  WNDCLASSEXW wc{};
  wc.cbSize        = sizeof(wc);
  wc.lpfnWndProc   = WndProc;
  wc.hInstance     = hInst;
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.style         = CS_HREDRAW | CS_VREDRAW;

  if (!RegisterClassExW(&wc)) return 0;

  DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  HWND hwnd = CreateWindowExW(
    0, CLASS_NAME, L"Battlezone-Inspired Wireframe (GDI) - Infinite World",
    style, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
    nullptr, nullptr, hInst, nullptr
  );
  if (!hwnd) return 0;

  ShowWindow(hwnd, SW_MAXIMIZE);
  UpdateWindow(hwnd);

  Game game(hwnd);
  g_game = &game;

  RECT rc{};
  GetClientRect(hwnd, &rc);
  game.OnResize(rc.right - rc.left, rc.bottom - rc.top);

  const double freq = QpcFreq();
  uint64_t last = QpcNow();

  MSG msg{};
  for (;;) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) { g_game = nullptr; return (int)msg.wParam; }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    uint64_t now = QpcNow();
    float dt = (float)((now - last) / freq);
    last = now;
    dt = std::min(dt, 0.05f);

    game.Update(dt);

    InvalidateRect(hwnd, nullptr, FALSE);
    Sleep(1);
  }
}