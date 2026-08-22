#pragma once
// ═══════════════════════════════════════════════════════════════
// ProfileChanger.hpp — Client-side profile display manipulation
//
// All changes are LOCAL (client memory write only).
// Real server values are unaffected — this changes what the
// LOCAL player sees on their screen.
//
// Field layout derived from iOS dump struct descriptor:
//   PlayerProfileMiniInfo {
//     coinsBalance  : uint64  @ Profile::CoinsBalance
//     cashBalance   : uint64  @ Profile::CashBalance  (estimated +8)
//     level         : int32   @ Profile::Level
//     winStreak     : int32   @ Profile::WinStreak
//     displayName   : Il2CppString*  @ Profile::DisplayName
//     userId        : Il2CppString*  @ Profile::UserId
//   }
// ═══════════════════════════════════════════════════════════════

#include "Config.hpp"
#include "Memory.hpp"
#include "Offsets.hpp"
#include "PatternScanner.hpp"
#include <android/log.h>
#include <cstring>

#define PROF_TAG "CrystalProfile"
#define PROFI(...) __android_log_print(ANDROID_LOG_INFO,  PROF_TAG, __VA_ARGS__)

class ProfileChanger {
public:
    uintptr_t profilePtr  = 0;   // local PlayerProfileMiniInfo* cached
    bool      applied     = false;

    // ── Try to locate and patch the profile object ────────────
    // Called once after game finishes loading.
    bool Apply() {
        if (!gCfg.profileChanger) { applied = false; return false; }

        uintptr_t ptr = FindProfilePtr();
        if (!ptr) {
            PROFI("ProfileChanger: profile ptr not found yet");
            return false;
        }
        profilePtr = ptr;

        // Coins
        if (gCfg.fakeCoins > 0)
            Memory::Write<uint64_t>(ptr + Offsets::Profile::CoinsBalance, gCfg.fakeCoins);

        // Cash
        if (gCfg.fakeCash > 0)
            Memory::Write<uint64_t>(ptr + Offsets::Profile::CashBalance, gCfg.fakeCash);

        // Level
        if (gCfg.fakeLevel > 0)
            Memory::Write<int32_t>(ptr + Offsets::Profile::Level, gCfg.fakeLevel);

        // Display name (Il2CppString* — patch char16 array in place)
        if (gCfg.fakeDisplayName[0] != '\0') {
            uintptr_t strPtr = 0;
            if (Memory::TryRead<uintptr_t>(ptr + Offsets::Profile::DisplayName, strPtr) && strPtr) {
                PatchIl2CppString(strPtr, gCfg.fakeDisplayName);
            }
        }

        // User ID (display only — no server effect)
        if (gCfg.fakeUserId[0] != '\0') {
            uintptr_t strPtr = 0;
            if (Memory::TryRead<uintptr_t>(ptr + Offsets::Profile::UserId, strPtr) && strPtr) {
                PatchIl2CppString(strPtr, gCfg.fakeUserId);
            }
        }

        applied = true;
        PROFI("ProfileChanger: applied (coins=%llu cash=%llu level=%d)",
              (unsigned long long)gCfg.fakeCoins,
              (unsigned long long)gCfg.fakeCash,
              gCfg.fakeLevel);
        return true;
    }

    // Must be called again if game re-creates profile object (lobby re-enter)
    void Reset() { profilePtr = 0; applied = false; }

private:
    // ── Locate the ProfileManager singleton ──────────────────
    // Strategy: scan for Il2Cpp static field via pattern + ADRP decode.
    // This is the same pattern used by all Unity IL2CPP mods.
    uintptr_t FindProfilePtr() {
        if (profilePtr) return profilePtr;

        // Pattern: ProfileManager static instance field (ADRP + LDR)
        // TODO: verify exact sig with Il2CppDumper on 5.8.0
        // For now: scan memory for known field value heuristic
        // (profile objects typically have level in 1-1000 range at known offset)
        return 0;  // returns 0 until sig is confirmed from Il2CppDumper
    }

    // ── Patch Il2CppString in place ───────────────────────────
    // Il2CppString: [0x10]=int32 length, [0x14]=char16_t chars[]
    // We overwrite chars[] with our new string, update length.
    // If new string is shorter than existing, null-pad remainder.
    void PatchIl2CppString(uintptr_t strPtr, const char* newStr) {
        if (!strPtr) return;

        int32_t  origLen = 0;
        if (!Memory::TryRead<int32_t>(strPtr + Offsets::Il2CppString::Length, origLen)) return;
        if (origLen <= 0 || origLen > 256) return;  // sanity

        size_t newLen = strlen(newStr);
        if (newLen > (size_t)origLen) newLen = (size_t)origLen;  // clamp to existing capacity

        // Write char16_t chars (ASCII → UTF-16LE: just zero-extend each byte)
        uintptr_t charsPtr = strPtr + Offsets::Il2CppString::Chars;
        for (size_t i = 0; i < newLen; i++) {
            uint16_t c16 = (uint8_t)newStr[i];
            Memory::Write<uint16_t>(charsPtr + i * 2, c16);
        }
        // Null-pad the rest
        for (size_t i = newLen; i < (size_t)origLen; i++) {
            Memory::Write<uint16_t>(charsPtr + i * 2, 0);
        }
        // Update length
        Memory::Write<int32_t>(strPtr + Offsets::Il2CppString::Length, (int32_t)newLen);

        PROFI("PatchIl2CppString: @0x%lx → \"%s\" (len=%zu)", strPtr, newStr, newLen);
    }
};

inline ProfileChanger gProfile;
