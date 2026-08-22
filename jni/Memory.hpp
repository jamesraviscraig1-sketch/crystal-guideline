#pragma once
// ═══════════════════════════════════════════════════════════════
// Memory.hpp — Android in-process memory reader
//
// KEY DIFFERENCE FROM WINDOWS VERSION:
//   We are injected INTO 8 Ball Pool via modded APK.
//   We don't open another process — we ARE the target process.
//   So ReadProcessMemory() → just dereference a pointer.
//
//   Base addresses are found via /proc/self/maps (same approach
//   as Crystal Official's memory.h).
// ═══════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <android/log.h>

#define MEM_TAG "CrystalGuideline"
#define MLOGI(...) __android_log_print(ANDROID_LOG_INFO,  MEM_TAG, __VA_ARGS__)
#define MLOGE(...) __android_log_print(ANDROID_LOG_ERROR, MEM_TAG, __VA_ARGS__)

class Memory {
public:
    uintptr_t il2cppBase = 0;
    uintptr_t unityBase  = 0;

    // Scan /proc/self/maps for a library base address (first r-xp region)
    static uintptr_t GetLibBase(const char* libName) {
        FILE* maps = fopen("/proc/self/maps", "r");
        if (!maps) return 0;

        char line[512];
        uintptr_t base = 0;
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, libName) && strstr(line, "r-xp")) {
                base = (uintptr_t)strtoull(line, nullptr, 16);
                break;
            }
        }
        fclose(maps);
        return base;
    }

    bool Init() {
        // 8 Ball Pool mobile uses Unity IL2CPP
        // Main libs: libunity.so + libil2cpp.so
        il2cppBase = GetLibBase("libil2cpp.so");
        unityBase  = GetLibBase("libunity.so");

        if (!il2cppBase) {
            MLOGE("libil2cpp.so not found in maps — game not loaded yet?");
            return false;
        }

        MLOGI("il2cpp base: 0x%lx | unity base: 0x%lx", il2cppBase, unityBase);
        return true;
    }

    // ── Direct pointer reads — we're in-process ─────────────
    // Safe read: mprotect check avoids SIGSEGV on bad addresses
    template<typename T>
    static T Read(uintptr_t addr) {
        return *reinterpret_cast<volatile T*>(addr);
    }

    template<typename T>
    static bool TryRead(uintptr_t addr, T& out) {
        if (!IsValidAddr(addr)) return false;
        out = *reinterpret_cast<volatile T*>(addr);
        return true;
    }

    template<typename T>
    static void Write(uintptr_t addr, T val) {
        *reinterpret_cast<volatile T*>(addr) = val;
    }

    // Follow a pointer chain: base → offsets[0] → offsets[1] → ...
    template<typename T>
    static T ReadChain(uintptr_t base, std::initializer_list<uintptr_t> offsets) {
        uintptr_t curr = base;
        for (auto off : offsets) {
            if (!IsValidAddr(curr)) return T{};
            curr = Read<uintptr_t>(curr) + off;
        }
        return Read<T>(curr);
    }

    // patch bytes at address (PROT_READ|WRITE|EXEC)
    static bool Patch(uintptr_t addr, const void* data, size_t size) {
        uintptr_t page  = addr & ~(uintptr_t)(getpagesize() - 1);
        size_t    range = (addr + size) - page;
        if (mprotect((void*)page, range, PROT_READ|PROT_WRITE|PROT_EXEC) != 0)
            return false;
        memcpy((void*)addr, data, size);
        mprotect((void*)page, range, PROT_READ|PROT_EXEC);
        __builtin___clear_cache((char*)addr, (char*)(addr + size));
        return true;
    }

    // NOP (arm64: 0xD503201F)
    static void NopAt(uintptr_t addr, int count = 1) {
        uint32_t nop = 0xD503201F;
        for (int i = 0; i < count; i++)
            Patch(addr + i * 4, &nop, 4);
    }

    uintptr_t il2cpp(uintptr_t offset) const { return il2cppBase + offset; }
    uintptr_t unity(uintptr_t offset)  const { return unityBase  + offset; }

private:
    static bool IsValidAddr(uintptr_t addr) {
        // Quick sanity check — not null, not in low pages
        return addr > 0x1000 && addr < 0x7FFFFFFFFFFF;
    }
};

// ── Global memory instance ──────────────────────────────────
inline Memory gMem;
