#pragma once
// ═══════════════════════════════════════════════════════════════
// Misc.hpp — Auto Queue + AdBlock
//
// AutoQueue:
//   After a match ends, the game shows a results screen then
//   returns to lobby. AutoQueue waits autoQueueDelayMs then
//   triggers QueueManager::startMatchmaking() via function call.
//   Falls back to simulating UI tap on the "Play" button if
//   the function pointer can't be resolved.
//
// AdBlock:
//   Unity ad calls go through a single ShowAd() method.
//   We NOP the first 4 bytes (ARM64: RET immediately).
//   This is a one-time patch applied at load time.
// ═══════════════════════════════════════════════════════════════

#include "Config.hpp"
#include "Memory.hpp"
#include "Offsets.hpp"
#include "PatternScanner.hpp"
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <ctime>

#define MISC_TAG "CrystalMisc"
#define MISCI(...) __android_log_print(ANDROID_LOG_INFO,  MISC_TAG, __VA_ARGS__)
#define MISCE(...) __android_log_print(ANDROID_LOG_ERROR, MISC_TAG, __VA_ARGS__)

class Misc {
public:
    uintptr_t fn_StartMatchmaking = 0;
    uintptr_t fn_ShowAd           = 0;
    bool      adPatched           = false;
    bool      queuePending        = false;
    int64_t   queueAt_ms          = 0;

    // ── Init — resolve + apply static patches ─────────────────
    void Init() {
        fn_StartMatchmaking = PatternScanner::FindPattern(
            "libil2cpp.so", Offsets::SIG_StartMatchmaking);
        fn_ShowAd = PatternScanner::FindPattern(
            "libil2cpp.so", Offsets::SIG_ShowAd);

        if (fn_StartMatchmaking) MISCI("StartMatchmaking @ 0x%lx", fn_StartMatchmaking);
        else                     MISCE("StartMatchmaking not found");

        // Apply AdBlock patch immediately if enabled
        if (gCfg.adBlock) ApplyAdBlock();
    }

    // ── AdBlock: NOP ShowAd() ─────────────────────────────────
    void ApplyAdBlock() {
        if (adPatched || !fn_ShowAd) return;

        // ARM64 RET = 0xD65F03C0 — makes ShowAd() return immediately
        uint32_t ret_instr = 0xD65F03C0;
        if (Memory::Patch(fn_ShowAd, &ret_instr, 4)) {
            adPatched = true;
            MISCI("AdBlock: ShowAd patched with RET @ 0x%lx", fn_ShowAd);
        } else {
            MISCE("AdBlock: Patch failed (mprotect error?)");
        }
    }

    // ── AutoQueue: called when match results are shown ────────
    // Java side should call Java_*_onMatchEnd() which calls this.
    void OnMatchEnd() {
        if (!gCfg.autoQueue) return;
        queuePending = true;
        queueAt_ms   = NowMs() + gCfg.autoQueueDelayMs;
        MISCI("AutoQueue: match ended, queueing in %d ms", gCfg.autoQueueDelayMs);
    }

    // ── Poll: call each frame from main loop ──────────────────
    void OnFrame() {
        if (!queuePending) return;
        if (NowMs() < queueAt_ms) return;
        queuePending = false;

        if (fn_StartMatchmaking) {
            // Direct function call — cleanest approach
            using StartMM_t = void(*)();
            reinterpret_cast<StartMM_t>(fn_StartMatchmaking)();
            MISCI("AutoQueue: startMatchmaking() called");
        } else {
            // Fallback: write a flag that Java-side picks up to simulate UI tap
            // Java side polls Java_*_needsQueueTap() and taps the button itself
            javaQueueTapNeeded = true;
            MISCI("AutoQueue: tap fallback requested");
        }
    }

    // ── Polled by Java to simulate Play button tap ────────────
    bool ConsumeQueueTap() {
        if (!javaQueueTapNeeded) return false;
        javaQueueTapNeeded = false;
        return true;
    }

    // ── Toggle adBlock at runtime ─────────────────────────────
    void SetAdBlock(bool enable) {
        if (enable && !adPatched) ApplyAdBlock();
        // Note: unpatching is not implemented (would need original bytes backup)
        // Toggle OFF just means future ads show — rare use case for resale product
    }

private:
    bool javaQueueTapNeeded = false;

    static int64_t NowMs() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
};

inline Misc gMisc;
