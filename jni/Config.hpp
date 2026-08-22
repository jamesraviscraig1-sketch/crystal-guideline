#pragma once
#include <cstdint>
#include <string>
#include <cstdlib>

// ═══════════════════════════════════════════════════════════════
// Config.hpp — All runtime settings for Crystal 8 Ball Pool mod
// All fields are plain POD — safe to read/write from JNI thread.
// ═══════════════════════════════════════════════════════════════

// ── Aim action modes ──────────────────────────────────────────
enum class AimAction : int {
    NONE      = 0,   // guideline only, no auto action
    AIM       = 1,   // snap aim angle to target automatically
    AUTO_PLAY = 2,   // full auto: aim + fire shot
};

// ── Delay modes ───────────────────────────────────────────────
enum class DelayMode : int {
    NONE    = 0,
    CUSTOM  = 1,  // fixed ms from cfg.aimDelayMs
    RANDOM  = 2,  // random in [cfg.aimDelayMin, cfg.aimDelayMax]
};

// ── Prediction state ──────────────────────────────────────────
enum class PredictionState : int {
    NONE  = 0,
    ENEMY = 1,
    BOTH  = 2,
};

// ── Color themes ──────────────────────────────────────────────
enum class ColorStyle : int {
    CYAN   = 0,
    WHITE  = 1,
    RED    = 2,
    GREEN  = 3,
    YELLOW = 4,
    PURPLE = 5,
};

// ── Main config struct ────────────────────────────────────────
struct Config {

    // ── Aimbot ─────────────────────────────────────────────
    AimAction  aimAction        = AimAction::NONE;
    bool       humanizedPower   = false;  // adds small random variance to shot power
    float      humanPowerVar    = 0.05f;  // ±% variance (0.0–0.15)
    bool       humanizedAngle   = false;  // smooth angular approach instead of snap
    float      humanAngleSpeed  = 3.0f;   // radians/sec angular speed
    DelayMode  delayMode        = DelayMode::NONE;
    int        aimDelayMs       = 300;    // ms for CUSTOM mode
    int        aimDelayMin      = 100;    // ms lower bound for RANDOM
    int        aimDelayMax      = 600;    // ms upper bound for RANDOM

    // ── Visual — Prediction line ────────────────────────────
    bool       drawPrediction     = true;
    float      predLineThickness  = 1.8f;
    float      predBallRadius     = 18.0f;
    uint8_t    predAlpha          = 255;    // 0-255
    bool       predBallFilled     = false;

    // ── Visual — Shot state circle ──────────────────────────
    bool       drawShotState       = true;
    float      shotCircleThickness = 2.0f;
    float      shotCircleRadius    = 24.0f;
    uint8_t    shotCircleAlpha     = 200;
    bool       shotCircleFilled    = false;

    // ── Visual — Prediction targets ─────────────────────────
    PredictionState predState  = PredictionState::BOTH;
    bool            wideGuideline = false;  // thicker main guideline

    // ── Visual — Color style ────────────────────────────────
    ColorStyle colorStyle = ColorStyle::CYAN;

    // ── Profile changer (client-side display only) ──────────
    bool       profileChanger    = false;
    uint64_t   fakeCoins         = 0;
    uint64_t   fakeCash          = 0;
    int32_t    fakeLevel         = 0;
    char       fakeDisplayName[64] = {};
    char       fakeUserId[32]      = {};

    // ── Miscellaneous ────────────────────────────────────────
    bool       autoQueue         = false;  // auto tap "Play" after match end
    int        autoQueueDelayMs  = 1500;   // ms wait before re-queueing
    bool       adBlock           = true;   // intercept Unity ad calls

    // ── Helpers ──────────────────────────────────────────────

    // Get the main line color (ARGB) based on colorStyle
    uint32_t LineColor(uint8_t alpha = 255) const {
        switch (colorStyle) {
            case ColorStyle::WHITE:  return ((uint32_t)alpha<<24)|0xFFFFFF;
            case ColorStyle::RED:    return ((uint32_t)alpha<<24)|0xFF3333;
            case ColorStyle::GREEN:  return ((uint32_t)alpha<<24)|0x33FF66;
            case ColorStyle::YELLOW: return ((uint32_t)alpha<<24)|0xFFDD00;
            case ColorStyle::PURPLE: return ((uint32_t)alpha<<24)|0xCC44FF;
            default: /* CYAN */      return ((uint32_t)alpha<<24)|0x00DCDC;
        }
    }

    uint32_t GhostColor()   const { return LineColor(predAlpha / 2); }
    uint32_t PredColor()    const { return LineColor(predAlpha); }
    uint32_t ShotColor()    const { return (((uint32_t)shotCircleAlpha)<<24)|0xFF8800; }

    float MainLineWidth()   const { return wideGuideline ? predLineThickness * 2.2f : predLineThickness; }

    // Random delay in ms based on delayMode
    int GetDelayMs() const {
        if (delayMode == DelayMode::CUSTOM) return aimDelayMs;
        if (delayMode == DelayMode::RANDOM)
            return aimDelayMin + (std::rand() % std::max(1, aimDelayMax - aimDelayMin));
        return 0;
    }

    // Power variance multiplier [0.85, 1.15] if humanizedPower
    float PowerMultiplier() const {
        if (!humanizedPower) return 1.0f;
        float var = humanPowerVar;
        float r   = (float)(std::rand() % 1000) / 1000.0f;  // 0..1
        return 1.0f - var + r * 2.0f * var;
    }
};

inline Config gCfg;
