#pragma once
#include <cstdint>

// ═══════════════════════════════════════════════════════════════
// Offsets.hpp — 8 Ball Pool 5.8.0  |  Android  |  arm64-v8a
//               libil2cpp.so (Unity IL2CPP)
//
// Sync source: core_dump.so (iOS ObjC) → structure mapping
//   + ARM64 pattern signatures for runtime resolution.
//
// HOW OFFSETS WORK HERE:
//   All RVA values (RVA_*) are relative to il2cpp base.
//   All FIELD_* values are byte offsets within a managed object
//   (the IL2CPP object starts with a vtable ptr at 0x00, so
//   fields begin at 0x10 typically).
//
// Pattern signatures auto-adapt across minor builds.
// If a pattern fails, the feature falls back to manual mode.
// ═══════════════════════════════════════════════════════════════

namespace Offsets {

    // ── libil2cpp.so patterns ─────────────────────────────────
    // Format: IDA-style "AA BB ? ? CC" (? = wildcard byte)

    // BallManager::GetBall(int index)
    // ARM64 prolog: STP x29,x30 + MOV w8 pattern for index arg
    constexpr const char* SIG_BallManager_GetBall =
        "FD 7B BF A9 FD 03 00 91 ? ? ? ? 08 00 40 F9 ? ? ? ? 28 00 80 52";

    // BallManager::GetCueBall()
    // Typically calls GetBall(0) — look for XOR W0,W0,W0 before branch
    constexpr const char* SIG_GetCueBall =
        "FD 7B BF A9 FD 03 00 91 ? ? ? ? 08 00 40 F9 00 00 80 52 ? ? ? ?";

    // GameController::setAimAngle(float angle)
    // Writes aim angle float, then triggers trajectory recalc
    // Signature: FMOV S8,W0 + STR S8,[X1] pattern
    constexpr const char* SIG_SetAimAngle =
        "E8 03 00 1E ? ? 00 BD ? ? ? ? 08 00 40 F9 ? ? 00 BD";

    // GameController::setShotPower(float power)
    // Clamps [0.0, 1.0] before storing
    constexpr const char* SIG_SetShotPower =
        "FD 7B BF A9 ? ? ? ? 00 00 80 3F ? ? ? ? ? ? ? ? 28 00 80 52";

    // GameController::shootBall()
    // Triggers the actual shot — auto-play calls this
    constexpr const char* SIG_ShootBall =
        "FD 7B BE A9 FD 83 01 91 ? ? ? ? ? ? ? ? 08 00 40 F9 ? ? 00 B9";

    // Camera::WorldToScreenPoint(Vector3 pos, Camera* cam)
    constexpr const char* SIG_WorldToScreen =
        "E8 03 1F AA ? ? ? ? ? ? ? ? 08 00 40 F9 ? ? ? ? 09 01 40 F9";

    // QueueManager::startMatchmaking()  →  Auto Queue
    constexpr const char* SIG_StartMatchmaking =
        "FD 7B BF A9 FD 03 00 91 ? ? ? ? ? ? ? ? 08 00 40 F9 ? ? 00 B9";

    // AdController::ShowAd() — patch to NOP for adBlock
    constexpr const char* SIG_ShowAd =
        "FD 7B BE A9 ? ? ? ? ? ? ? ? 48 D0 3B D5 ? ? ? ? 08 00 40 F9";

    // ── Managed object field offsets ──────────────────────────
    // IL2CPP object layout:  [0x00] vtable*  [0x08] monitor*
    // First field typically at 0x10.

    // Ball (UnityEngine.MonoBehaviour → Ball)
    namespace Ball {
        constexpr uintptr_t Transform   = 0x10;  // Transform*
        constexpr uintptr_t BallIndex   = 0x18;  // int32
        constexpr uintptr_t IsPocketed  = 0x1C;  // bool
        constexpr uintptr_t IsCueBall   = 0x1D;  // bool
    }

    // Transform → local position
    namespace Transform {
        constexpr uintptr_t LocalPos    = 0x90;  // Vector3 (x,y,z floats)
    }

    // GameController
    namespace GameCtrl {
        constexpr uintptr_t AimAngle    = 0x28;  // float  (radians)
        constexpr uintptr_t ShotPower   = 0x2C;  // float  [0.0-1.0]
        constexpr uintptr_t IsMyTurn    = 0x34;  // bool
        constexpr uintptr_t BallMgr     = 0x40;  // BallManager*
    }

    // BallManager
    namespace BallMgr {
        constexpr uintptr_t BallArray   = 0x20;  // Ball*[]
        constexpr uintptr_t BallCount   = 0x28;  // int32
        constexpr uintptr_t CueBall     = 0x30;  // Ball*
    }

    // PlayerProfile (from iOS dump: coinsBalance=Q, level=i)
    // Offset chain: ProfileManager → localProfile → fields
    namespace Profile {
        constexpr uintptr_t CoinsBalance = 0x58;  // uint64
        constexpr uintptr_t CashBalance  = 0x60;  // uint64
        constexpr uintptr_t Level        = 0x68;  // int32
        constexpr uintptr_t WinStreak    = 0x6C;  // int32
        constexpr uintptr_t DisplayName  = 0x70;  // Il2CppString*
        constexpr uintptr_t UserId       = 0x80;  // Il2CppString*
    }

    // ── Il2CppString layout ───────────────────────────────────
    // [0x00] vtable*  [0x08] monitor*  [0x10] int32 length
    // [0x14] char16_t chars[length]
    namespace Il2CppString {
        constexpr uintptr_t Length  = 0x10;
        constexpr uintptr_t Chars   = 0x14;
    }
}
