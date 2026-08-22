#pragma once
// ═══════════════════════════════════════════════════════════════
// Guideline.hpp — 8 Ball Pool aim assistant (Android port)
//
// WINDOWS → ANDROID DELTA:
//   - ImGui::GetBackgroundDrawList() → DrawCmd vector (rendered via Android Canvas)
//   - GetAsyncKeyState() → touch events (onTouch / setPoint)
//   - GHOST_BALL + LINE_EXTENDER modes preserved 1:1
//   - ReflectLine physics unchanged
//   - Memory reading adapted: read own process (in-process .so)
//
// Rendering pipeline:
//   Guideline::ComputeFrame() → fills gDrawCmds
//   main.cpp Java_*_drawFrame() → iterates gDrawCmds, calls Canvas.drawLine/Circle via JNI
//
// Touch controls (replaces keyboard):
//   Tap on cue area    → set cue ball position
//   Tap on target area → set target ball / line tip
//   Tap on hole area   → set hole position
//   Double-tap         → cycle mode (GHOST_BALL ↔ LINE_EXTENDER)
//   Pinch              → adjust ball radius
// ═══════════════════════════════════════════════════════════════

#include "MathUtils.hpp"
#include "Memory.hpp"
#include "PatternScanner.hpp"
#include <vector>
#include <cstdint>
#include <cmath>

// ─── Draw Command — what main.cpp passes to Canvas ────────────
struct DrawCmd {
    enum Type : int { LINE = 0, CIRCLE_STROKE = 1, CIRCLE_FILL = 2 };
    Type     type;
    uint32_t color;      // ARGB packed (0xAARRGGBB)
    float    x1, y1;     // line start OR circle center
    float    x2, y2;     // line end (unused for circles)
    float    radius;     // circle radius (unused for lines)
    float    strokeWidth;
};

// Global draw list — cleared each frame by ComputeFrame()
inline std::vector<DrawCmd> gDrawCmds;

// ─── helper to pack ARGB color ────────────────────────────────
static constexpr uint32_t ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ═══════════════════════════════════════════════════════════════
// 8 Ball Pool Mobile — memory offsets
// Build: 5.x.x  |  arm64-v8a  |  libil2cpp.so
//
// TODO: Verify offsets with latest APK dump via Il2CppDumper.
//       Pattern signatures auto-adapt across builds.
// ═══════════════════════════════════════════════════════════════
namespace Offsets {
    // BallController.position (Vector3 offset within instance)
    constexpr uintptr_t Ball_PosX = 0x28;
    constexpr uintptr_t Ball_PosY = 0x2C;
    constexpr uintptr_t Ball_PosZ = 0x30;

    // Pattern for BallManager::GetBalls  (arm64 instructions)
    // From Il2CppDumper dump_cs analysis
    constexpr const char* SIG_BallManager =
        "F3 53 BE A9 FD 7B 01 A9 FD 43 00 91";  // TODO: verify per build

    // Pattern for Camera::WorldToScreenPoint
    constexpr const char* SIG_WorldToScreen =
        "E8 03 1F AA ? ? ? ? ? ? ? ? 08 00 40 F9";
}

// ═══════════════════════════════════════════════════════════════
// GuidelineCore — main aim calculation class
// ═══════════════════════════════════════════════════════════════
class GuidelineCore {
public:
    enum Mode { GHOST_BALL, LINE_EXTENDER };
    Mode currentMode = LINE_EXTENDER;

    // Ball positions (screen coordinates, set by touch or memory read)
    Vector2 cueBall    = { 400, 700 };
    Vector2 targetBall = { 600, 500 };
    Vector2 hole       = { 800, 300 };
    Vector2 lineTip    = { 650, 550 };

    // Table boundaries (calibrated via menu sliders)
    float tableLeft   = 60.0f;
    float tableTop    = 100.0f;
    float tableRight  = 1020.0f;
    float tableBottom = 600.0f;

    float ballRadius  = 18.0f;

    // Colors (ARGB)
    uint32_t colorLine      = ARGB(255,   0, 220, 220);  // cyan
    uint32_t colorGhost     = ARGB(150, 255, 255, 255);  // white semi
    uint32_t colorSelected  = ARGB(255,   0, 255,   0);  // green
    uint32_t colorHole      = ARGB(200, 255,  50,  50);  // red
    uint32_t colorTable     = ARGB( 50, 255, 255, 255);  // white dim
    uint32_t colorTangent   = ARGB( 80, 255, 255, 255);  // white faint

    int  selectedPoint = 1;  // 1=Cue 2=Target/Tip 3=Hole
    bool showMenu      = false;

    // Memory / auto-read
    bool   memInitialized   = false;
    uintptr_t ballManagerPtr = 0;
    int32_t   lastScanMs    = 0;

    // Screen dimensions (set by Java on overlay create)
    float screenW = 1080.0f;
    float screenH = 2400.0f;

    // ─── Memory init ────────────────────────────────────────
    bool InitMemory() {
        if (!gMem.Init()) return false;
        memInitialized = true;
        return true;
    }

    void UpdateFromMemory() {
        if (!memInitialized || ballManagerPtr != 0) return;

        // Throttle scans to every 5 s
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int32_t nowMs = (int32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
        if (nowMs - lastScanMs < 5000) return;
        lastScanMs = nowMs;

        ballManagerPtr = PatternScanner::FindPattern(
            "libil2cpp.so", Offsets::SIG_BallManager);

        if (ballManagerPtr) {
            MLOGI("BallManager found @ 0x%lx", ballManagerPtr);
        }
    }

    // ─── Touch input (replaces keyboard) ────────────────────
    // action: 0=DOWN 1=UP 2=MOVE (Android MotionEvent values)
    void OnTouch(float x, float y, int action) {
        if (action == 0 || action == 2) {  // DOWN or MOVE
            Vector2* target = nullptr;
            switch (selectedPoint) {
                case 1: target = &cueBall;    break;
                case 2: target = (currentMode == GHOST_BALL) ? &targetBall : &lineTip; break;
                case 3: target = &hole;       break;
            }
            if (target) {
                target->x = x;
                target->y = y;
            }
        }
    }

    // Explicit set (called by tap-to-select mode)
    void SetPoint(int pointId, float x, float y) {
        selectedPoint = pointId;
        OnTouch(x, y, 0);
    }

    void CycleMode()   { currentMode = (currentMode == GHOST_BALL) ? LINE_EXTENDER : GHOST_BALL; }
    void CyclePoint()  { selectedPoint = (selectedPoint % 3) + 1; }
    void SetScreen(float w, float h) { screenW = w; screenH = h; }

    // ─── Core frame computation ──────────────────────────────
    // Fills gDrawCmds — called every frame from Java onDraw()
    void ComputeFrame() {
        gDrawCmds.clear();
        UpdateFromMemory();

        // Table outline
        PushLine(tableLeft, tableTop, tableRight, tableTop,    colorTable, 2.0f);
        PushLine(tableRight, tableTop, tableRight, tableBottom, colorTable, 2.0f);
        PushLine(tableRight, tableBottom, tableLeft, tableBottom, colorTable, 2.0f);
        PushLine(tableLeft, tableBottom, tableLeft, tableTop,  colorTable, 2.0f);

        if (currentMode == GHOST_BALL) {
            DrawGhostBallMode();
        } else {
            DrawLineExtenderMode();
        }
    }

private:
    // ─── Ghost Ball Mode ─────────────────────────────────────
    void DrawGhostBallMode() {
        uint32_t c1 = (selectedPoint == 1) ? colorSelected : colorLine;
        uint32_t c2 = (selectedPoint == 2) ? colorSelected : colorLine;
        uint32_t c3 = (selectedPoint == 3) ? colorHole     : colorHole;

        // Direction from target ball to hole
        Vector2 dirToHole = (hole - targetBall).normalized();
        // Ghost ball sits directly behind the impact on the target
        Vector2 impactPoint = targetBall - (dirToHole * (ballRadius * 2.0f));

        // Cue ball circle
        PushCircle(cueBall.x, cueBall.y, ballRadius, c1, false, 2.5f);
        // Target ball circle
        PushCircle(targetBall.x, targetBall.y, ballRadius, c2, false, 2.5f);
        // Hole marker
        PushCircle(hole.x, hole.y, ballRadius * 0.6f, c3, false, 2.0f);
        // Ghost ball (where cue ball needs to hit)
        PushCircle(impactPoint.x, impactPoint.y, ballRadius, colorGhost, false, 1.5f);

        // Cue → ghost line
        PushLine(cueBall.x, cueBall.y, impactPoint.x, impactPoint.y, colorGhost, 2.0f);

        // Target → hole with wall bounces
        ReflectLine(targetBall, dirToHole, colorLine, 2);

        // Tangent predictor (cue ball deflection after impact)
        Vector2 tangent   = { -dirToHole.y, dirToHole.x };
        Vector2 cueToImp  = (impactPoint - cueBall).normalized();
        if (cueToImp.x * tangent.x + cueToImp.y * tangent.y < 0.0f)
            tangent = tangent * -1.0f;
        Vector2 tangentEnd = impactPoint + (tangent * 400.0f);
        PushLine(impactPoint.x, impactPoint.y, tangentEnd.x, tangentEnd.y, colorTangent, 1.5f);
    }

    // ─── Line Extender Mode ──────────────────────────────────
    void DrawLineExtenderMode() {
        uint32_t c1 = (selectedPoint == 1) ? colorSelected : colorLine;
        uint32_t c2 = (selectedPoint == 2) ? colorSelected : ARGB(200, 255, 220, 0);

        PushCircle(cueBall.x, cueBall.y, 8.0f, c1, false, 2.5f);
        PushCircle(lineTip.x, lineTip.y, 8.0f, c2, false, 1.5f);

        Vector2 dir = (lineTip - cueBall).normalized();
        ReflectLine(cueBall, dir, colorLine, 3);
    }

    // ─── ReflectLine — wall-bounce physics ───────────────────
    // Identical math to Windows Overlay.hpp
    void ReflectLine(Vector2 start, Vector2 dir, uint32_t color, int bounces) {
        Vector2 curStart = start;
        Vector2 curDir   = dir.normalized();

        for (int i = 0; i < bounces; i++) {
            float   t      = 10000.0f;
            Vector2 normal = { 0.0f, 0.0f };

            if (curDir.x > 0) { float d = (tableRight  - curStart.x) / curDir.x; if (d < t) { t = d; normal = {-1, 0}; } }
            if (curDir.x < 0) { float d = (tableLeft   - curStart.x) / curDir.x; if (d < t) { t = d; normal = { 1, 0}; } }
            if (curDir.y > 0) { float d = (tableBottom - curStart.y) / curDir.y; if (d < t) { t = d; normal = { 0,-1}; } }
            if (curDir.y < 0) { float d = (tableTop    - curStart.y) / curDir.y; if (d < t) { t = d; normal = { 0, 1}; } }

            Vector2 end = curStart + (curDir * t);
            PushLine(curStart.x, curStart.y, end.x, end.y, color, 1.8f);

            // Fade on successive bounces
            color = (color & 0x00FFFFFF) | (((color >> 24) * 2 / 3) << 24);

            curStart = end;
            curDir   = curDir.reflect(normal);
        }
    }

    // ─── DrawCmd push helpers ─────────────────────────────────
    void PushLine(float x1, float y1, float x2, float y2, uint32_t color, float w) {
        gDrawCmds.push_back({ DrawCmd::LINE, color, x1, y1, x2, y2, 0.0f, w });
    }

    void PushCircle(float cx, float cy, float r, uint32_t color, bool fill, float w) {
        DrawCmd::Type t = fill ? DrawCmd::CIRCLE_FILL : DrawCmd::CIRCLE_STROKE;
        gDrawCmds.push_back({ t, color, cx, cy, 0.0f, 0.0f, r, w });
    }
};

// Global instance
inline GuidelineCore gGuideline;
