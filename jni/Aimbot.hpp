#pragma once
// ═══════════════════════════════════════════════════════════════
// Aimbot.hpp — Aim snapping, humanized control, auto-play
//
// Three action modes (Config::aimAction):
//   NONE      → passive, guideline only
//   AIM       → rotate game's aim angle to face optimal shot
//   AUTO_PLAY → AIM + fire shot after delay
//
// Humanized power: applies ±% variance before firing.
// Humanized angle: smooth lerp approach over multiple frames.
// Delay mode:      wait custom or random ms before acting.
// ═══════════════════════════════════════════════════════════════

#include "Config.hpp"
#include "Memory.hpp"
#include "Offsets.hpp"
#include "MathUtils.hpp"
#include "PatternScanner.hpp"
#include <ctime>
#include <unistd.h>
#include <android/log.h>

#define ABOT_TAG "CrystalAimbot"
#define ABOTI(...) __android_log_print(ANDROID_LOG_INFO,  ABOT_TAG, __VA_ARGS__)
#define ABOTE(...) __android_log_print(ANDROID_LOG_ERROR, ABOT_TAG, __VA_ARGS__)

class Aimbot {
public:
    // Resolved function pointers
    uintptr_t fn_SetAimAngle  = 0;
    uintptr_t fn_SetShotPower = 0;
    uintptr_t fn_ShootBall    = 0;

    bool      resolved        = false;
    bool      fired           = false;   // prevent double-fire per turn
    float     currentAngle    = 0.f;     // smoothed approach state
    int64_t   fireAt_ms       = 0;       // wall-clock time to fire

    // Pointer to GameController instance (resolved from memory scan)
    uintptr_t gameCtrlPtr     = 0;

    // ── Init: resolve function pointers ──────────────────────
    void Init() {
        fn_SetAimAngle  = PatternScanner::FindPattern("libil2cpp.so", Offsets::SIG_SetAimAngle);
        fn_SetShotPower = PatternScanner::FindPattern("libil2cpp.so", Offsets::SIG_SetShotPower);
        fn_ShootBall    = PatternScanner::FindPattern("libil2cpp.so", Offsets::SIG_ShootBall);

        resolved = fn_SetAimAngle && fn_SetShotPower && fn_ShootBall;
        if (resolved) ABOTI("Aimbot resolved: aim@0x%lx power@0x%lx shoot@0x%lx",
                             fn_SetAimAngle, fn_SetShotPower, fn_ShootBall);
        else          ABOTE("Aimbot: one or more functions not found — manual mode only");
    }

    // ── Called each frame by GuidelineCore::ComputeFrame() ───
    // targetAngle: ideal aim angle in radians (computed from guideline geometry)
    // targetPower: normalized shot power [0.0, 1.0]
    void OnFrame(float targetAngle, float targetPower) {
        if (gCfg.aimAction == AimAction::NONE) return;
        if (!resolved) return;

        bool isMyTurn = IsMyTurn();
        if (!isMyTurn) {
            fired    = false;
            fireAt_ms = 0;
            return;
        }

        float angle = targetAngle;

        // ── Humanized angle: lerp toward target ──────────────
        if (gCfg.humanizedAngle) {
            // currentAngle wraps smoothly
            float diff = angle - currentAngle;
            // Normalize diff to [-π, π]
            while (diff >  3.14159f) diff -= 6.28318f;
            while (diff < -3.14159f) diff += 6.28318f;
            float step = gCfg.humanAngleSpeed * (1.f / 60.f);  // assume ~60fps
            if (std::abs(diff) > step)
                currentAngle += (diff > 0 ? step : -step);
            else
                currentAngle = angle;
            angle = currentAngle;
        } else {
            currentAngle = angle;
        }

        // ── Write aim angle to game ───────────────────────────
        CallSetAimAngle(angle);

        if (gCfg.aimAction == AimAction::AIM) return;  // AIM only, no fire

        // ── AUTO_PLAY: schedule fire ──────────────────────────
        if (!fired) {
            if (fireAt_ms == 0) {
                // First frame of our turn — schedule fire time
                fireAt_ms = NowMs() + gCfg.GetDelayMs();
            } else if (NowMs() >= fireAt_ms) {
                // Delay elapsed — fire
                float power = Clamp(targetPower * gCfg.PowerMultiplier(), 0.05f, 1.0f);
                CallSetShotPower(power);
                usleep(16000);  // one frame gap between power and shoot
                CallShootBall();
                fired    = true;
                fireAt_ms = 0;
                ABOTI("AUTO_PLAY: fired angle=%.3f power=%.3f", angle, power);
            }
        }
    }

    // ── Called when turn changes (reset state) ────────────────
    void OnTurnChange() {
        fired     = false;
        fireAt_ms = 0;
        currentAngle = 0.f;
    }

private:
    // ── Inline function call wrappers (ARM64) ─────────────────
    // We call the game's own functions so anti-cheat doesn't see
    // anomalous memory writes — the game validates angle itself.

    using SetAimAngle_t  = void(*)(uintptr_t self, float angle);
    using SetShotPower_t = void(*)(uintptr_t self, float power);
    using ShootBall_t    = void(*)(uintptr_t self);

    void CallSetAimAngle(float angle) {
        uintptr_t ctrl = GetGameCtrl();
        if (!ctrl || !fn_SetAimAngle) return;
        reinterpret_cast<SetAimAngle_t>(fn_SetAimAngle)(ctrl, angle);
    }

    void CallSetShotPower(float power) {
        uintptr_t ctrl = GetGameCtrl();
        if (!ctrl || !fn_SetShotPower) return;
        reinterpret_cast<SetShotPower_t>(fn_SetShotPower)(ctrl, power);
    }

    void CallShootBall() {
        uintptr_t ctrl = GetGameCtrl();
        if (!ctrl || !fn_ShootBall) return;
        reinterpret_cast<ShootBall_t>(fn_ShootBall)(ctrl);
    }

    // ── Read isMyTurn from GameController ────────────────────
    bool IsMyTurn() {
        uintptr_t ctrl = GetGameCtrl();
        if (!ctrl) return false;
        bool v = false;
        Memory::TryRead<bool>(ctrl + Offsets::GameCtrl::IsMyTurn, v);
        return v;
    }

    // Lazy-resolve GameController singleton pointer
    uintptr_t GetGameCtrl() {
        if (gameCtrlPtr) return gameCtrlPtr;
        // GameController is a MonoBehaviour singleton.
        // Pattern: static field loaded via ADRP → LDR from il2cpp metadata.
        // For now use direct memory scan result cached in gMem.
        // TODO: resolve via Il2Cpp class lookup when metadata is available.
        return gameCtrlPtr;
    }

    // Wall-clock time in ms
    static int64_t NowMs() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
};

inline Aimbot gAimbot;
