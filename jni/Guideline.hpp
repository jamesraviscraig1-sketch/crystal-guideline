#pragma once
// ═══════════════════════════════════════════════════════════════
// Guideline.hpp — Full 8 Ball Pool aim assistant
//
// Modes:
//   GHOST_BALL    — shows ghost ball position + impact trajectory
//   LINE_EXTENDER — extends the cue/aim line with reflections
//
// Visual settings (all driven by Config gCfg):
//   predLineThickness   — guideline stroke width
//   predBallRadius      — ball circle radius
//   predAlpha           — prediction line/ball transparency
//   predBallFilled      — fill style for ball circles
//   drawShotState       — ring around cue ball showing power
//   shotCircleRadius    — shot state ring size
//   shotCircleAlpha     — shot state ring transparency
//   shotCircleFilled    — fill style
//   predState           — whose trajectory to predict (NONE/ENEMY/BOTH)
//   wideGuideline       — double line thickness
//   colorStyle          — cyan/white/red/green/yellow/purple
// ═══════════════════════════════════════════════════════════════

#include "MathUtils.hpp"
#include "Memory.hpp"
#include "Offsets.hpp"
#include "PatternScanner.hpp"
#include "Config.hpp"
#include "Aimbot.hpp"
#include "Misc.hpp"
#include <vector>
#include <cstdint>
#include <cmath>
#include <android/log.h>

#define GUID_TAG "CrystalGuideline"
#define GLOGI(...) __android_log_print(ANDROID_LOG_INFO,  GUID_TAG, __VA_ARGS__)
#define GLOGE(...) __android_log_print(ANDROID_LOG_ERROR, GUID_TAG, __VA_ARGS__)

// ─── Draw Command ─────────────────────────────────────────────
struct DrawCmd {
    enum Type : int { LINE = 0, CIRCLE_STROKE = 1, CIRCLE_FILL = 2 };
    Type     type;
    uint32_t color;
    float    x1, y1, x2, y2;
    float    radius;
    float    strokeWidth;
};
inline std::vector<DrawCmd> gDrawCmds;

static constexpr uint32_t ARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ─── GuidelineCore ────────────────────────────────────────────
class GuidelineCore {
public:
    enum Mode { GHOST_BALL, LINE_EXTENDER };
    Mode currentMode = LINE_EXTENDER;

    // Manually placed ball positions (touch input / tap-select)
    Vector2 cueBall    = { 400, 700 };
    Vector2 targetBall = { 600, 500 };
    Vector2 hole       = { 800, 300 };
    Vector2 lineTip    = { 650, 550 };

    // Table boundaries (calibrated via menu sliders or auto-detect)
    float tableLeft   = 60.0f;
    float tableTop    = 100.0f;
    float tableRight  = 1020.0f;
    float tableBottom = 600.0f;
    float ballRadius  = 18.0f;

    int   selectedPoint   = 1;  // 1=Cue 2=Target/Tip 3=Hole
    float screenW         = 1080.f;
    float screenH         = 2400.f;

    // Memory-read ball positions (optional — overrides touch if valid)
    bool  memInitialized  = false;
    float memCueBall_x    = 0.f, memCueBall_y = 0.f;
    float currentPower    = 0.5f;  // read from memory when available

    // ── Init ──────────────────────────────────────────────────
    bool InitMemory() {
        if (!gMem.Init()) return false;
        memInitialized = true;
        gAimbot.Init();
        gMisc.Init();
        return true;
    }

    // ── Touch input ───────────────────────────────────────────
    void OnTouch(float x, float y, int action) {
        if (action == 0 || action == 2) {
            Vector2* target = nullptr;
            switch (selectedPoint) {
                case 1: target = &cueBall;    break;
                case 2: target = (currentMode == GHOST_BALL) ? &targetBall : &lineTip; break;
                case 3: target = &hole;       break;
            }
            if (target) { target->x = x; target->y = y; }
        }
    }

    void SetPoint(int id, float x, float y) { selectedPoint = id; OnTouch(x,y,0); }
    void CycleMode()  { currentMode = (currentMode==GHOST_BALL) ? LINE_EXTENDER : GHOST_BALL; }
    void CyclePoint() { selectedPoint = (selectedPoint % 3) + 1; }
    void SetScreen(float w, float h) { screenW = w; screenH = h; }

    // ── Main frame ────────────────────────────────────────────
    void ComputeFrame() {
        gDrawCmds.clear();
        gMisc.OnFrame();

        // Optionally override positions from memory
        if (memInitialized) UpdateFromMemory();

        // Table outline
        uint32_t tableColor = ARGB(40, 255, 255, 255);
        PushLine(tableLeft, tableTop,    tableRight, tableTop,    tableColor, 1.5f);
        PushLine(tableRight, tableTop,   tableRight, tableBottom, tableColor, 1.5f);
        PushLine(tableRight, tableBottom,tableLeft,  tableBottom, tableColor, 1.5f);
        PushLine(tableLeft,  tableBottom,tableLeft,  tableTop,    tableColor, 1.5f);

        // Shot state ring (power indicator around cue ball)
        if (gCfg.drawShotState) DrawShotStateRing();

        // Main guideline
        if (currentMode == GHOST_BALL) DrawGhostBallMode();
        else                           DrawLineExtenderMode();

        // Feed computed aim angle to aimbot
        if (gCfg.aimAction != AimAction::NONE) {
            float angle = ComputeAimAngle();
            gAimbot.OnFrame(angle, currentPower);
        }
    }

private:
    // ── Compute optimal aim angle from current geometry ───────
    float ComputeAimAngle() const {
        if (currentMode == LINE_EXTENDER) {
            Vector2 dir = (lineTip - cueBall).normalized();
            return std::atan2(dir.y, dir.x);
        }
        // Ghost ball: angle from cue to impact point
        Vector2 dirToHole  = (hole - targetBall).normalized();
        Vector2 impact     = targetBall - (dirToHole * (ballRadius * 2.f));
        Vector2 dir        = (impact - cueBall).normalized();
        return std::atan2(dir.y, dir.x);
    }

    // ── Shot state ring ───────────────────────────────────────
    void DrawShotStateRing() {
        uint32_t col = (((uint32_t)gCfg.shotCircleAlpha) << 24) | 0xFF8800;
        // Radius scales with power
        float r = gCfg.shotCircleRadius * (0.5f + currentPower * 0.5f);
        if (gCfg.shotCircleFilled)
            PushCircle(cueBall.x, cueBall.y, r, col, true,  gCfg.shotCircleThickness);
        else
            PushCircle(cueBall.x, cueBall.y, r, col, false, gCfg.shotCircleThickness);
    }

    // ── Ghost Ball Mode ───────────────────────────────────────
    void DrawGhostBallMode() {
        uint32_t c1   = (selectedPoint==1) ? ARGB(255,0,255,0) : gCfg.PredColor();
        uint32_t c2   = (selectedPoint==2) ? ARGB(255,0,255,0) : gCfg.PredColor();
        uint32_t cHole = ARGB(200,255,50,50);
        float    lw   = gCfg.MainLineWidth();
        float    br   = gCfg.predBallRadius;
        bool     fill = gCfg.predBallFilled;

        Vector2 dirToHole = (hole - targetBall).normalized();
        Vector2 impact    = targetBall - (dirToHole * (br * 2.f));

        // Cue ball
        PushCircle(cueBall.x, cueBall.y, br, c1, fill, gCfg.predLineThickness);
        // Target ball
        PushCircle(targetBall.x, targetBall.y, br, c2, fill, gCfg.predLineThickness);
        // Hole marker
        PushCircle(hole.x, hole.y, br * 0.6f, cHole, false, 1.5f);
        // Ghost ball (semi-transparent)
        PushCircle(impact.x, impact.y, br, gCfg.GhostColor(), fill, 1.5f);

        // Cue → impact line
        PushLine(cueBall.x, cueBall.y, impact.x, impact.y, gCfg.GhostColor(), lw);

        // Target → hole trajectory (with bounces)
        // Prediction state controls whether we draw enemy ball trajectories
        bool drawTarget = (gCfg.predState == PredictionState::BOTH ||
                           gCfg.predState == PredictionState::ENEMY);
        if (drawTarget)
            ReflectLine(targetBall, dirToHole, gCfg.PredColor(), lw, 2);

        // Tangent: cue ball path after impact
        Vector2 tangent  = { -dirToHole.y, dirToHole.x };
        Vector2 cueDir   = (impact - cueBall).normalized();
        if (cueDir.dot(tangent) < 0.f) tangent = tangent * -1.f;
        Vector2 tangEnd  = impact + (tangent * 400.f);
        PushLine(impact.x, impact.y, tangEnd.x, tangEnd.y,
                 ARGB(70,255,255,255), 1.2f);
    }

    // ── Line Extender Mode ────────────────────────────────────
    void DrawLineExtenderMode() {
        float    lw = gCfg.MainLineWidth();
        uint32_t c1 = (selectedPoint==1) ? ARGB(255,0,255,0)    : gCfg.PredColor();
        uint32_t c2 = (selectedPoint==2) ? ARGB(255,0,255,0)    : ARGB(200,255,220,0);

        PushCircle(cueBall.x,  cueBall.y,  8.f, c1, false, 2.5f);
        PushCircle(lineTip.x,  lineTip.y,  8.f, c2, false, 1.5f);

        Vector2 dir = (lineTip - cueBall).normalized();
        ReflectLine(cueBall, dir, gCfg.PredColor(), lw, 3);

        // Wide guideline: draw a second parallel line offset by ±ballRadius
        if (gCfg.wideGuideline) {
            Vector2 perp = { -dir.y, dir.x };
            float   r    = gCfg.predBallRadius;
            Vector2 offA = { cueBall.x + perp.x*r, cueBall.y + perp.y*r };
            Vector2 offB = { cueBall.x - perp.x*r, cueBall.y - perp.y*r };
            Vector2 dirA = { lineTip.x + perp.x*r, lineTip.y + perp.y*r };
            Vector2 dirB = { lineTip.x - perp.x*r, lineTip.y - perp.y*r };
            uint32_t dimCol = (gCfg.PredColor() & 0x00FFFFFF) |
                              ((((gCfg.PredColor()>>24) * 2/5) & 0xFF) << 24);
            ReflectLine(offA, (dirA - offA).normalized(), dimCol, lw * 0.6f, 3);
            ReflectLine(offB, (dirB - offB).normalized(), dimCol, lw * 0.6f, 3);
        }
    }

    // ── ReflectLine — wall-bounce physics ────────────────────
    void ReflectLine(Vector2 start, Vector2 dir, uint32_t color, float lw, int bounces) {
        Vector2 cur = start;
        Vector2 d   = dir.normalized();
        for (int i = 0; i < bounces; i++) {
            float   t  = 10000.f;
            Vector2 n  = {};
            if (d.x > 0) { float dt=(tableRight -cur.x)/d.x; if(dt<t){t=dt;n={-1,0};} }
            if (d.x < 0) { float dt=(tableLeft  -cur.x)/d.x; if(dt<t){t=dt;n={ 1,0};} }
            if (d.y > 0) { float dt=(tableBottom-cur.y)/d.y; if(dt<t){t=dt;n={ 0,-1};} }
            if (d.y < 0) { float dt=(tableTop   -cur.y)/d.y; if(dt<t){t=dt;n={ 0, 1};} }
            Vector2 end = cur + (d * t);
            PushLine(cur.x, cur.y, end.x, end.y, color, lw);
            // Fade on each bounce
            color = (color & 0x00FFFFFF) | (((color>>24)*2/3 & 0xFF)<<24);
            lw   *= 0.85f;
            cur   = end;
            d     = d.reflect(n);
        }
    }

    // ── Memory read (optional position override) ──────────────
    void UpdateFromMemory() {
        // Read cue ball screen position if ballManagerPtr is resolved
        // Falls back to touch-placed position if not available
        // Power: read from GameController
        uintptr_t ctrl = 0; // TODO: cache from ADRP resolve
        if (ctrl) {
            float p = 0.f;
            if (Memory::TryRead<float>(ctrl + Offsets::GameCtrl::ShotPower, p))
                currentPower = Clamp(p, 0.f, 1.f);
        }
    }

    // ── Draw helpers ─────────────────────────────────────────
    void PushLine(float x1,float y1,float x2,float y2,uint32_t col,float w) {
        gDrawCmds.push_back({DrawCmd::LINE,col,x1,y1,x2,y2,0.f,w});
    }
    void PushCircle(float cx,float cy,float r,uint32_t col,bool fill,float w) {
        DrawCmd::Type t = fill ? DrawCmd::CIRCLE_FILL : DrawCmd::CIRCLE_STROKE;
        gDrawCmds.push_back({t,col,cx,cy,0.f,0.f,r,w});
    }
};

inline GuidelineCore gGuideline;
