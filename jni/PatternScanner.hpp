#pragma once
// ═══════════════════════════════════════════════════════════════
// PatternScanner.hpp — Android ARM64 in-process scanner
//
// WINDOWS → ANDROID DELTA:
//   - K32GetModuleInformation → parse /proc/self/maps for region bounds
//   - ReadProcessMemory → direct pointer read (we're in-process)
//   - Same IDA-style "AA BB ? ? CC" signature format
// ═══════════════════════════════════════════════════════════════

#include "Memory.hpp"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>

class PatternScanner {
public:
    struct Region {
        uintptr_t start;
        uintptr_t end;
    };

    // Find all r-xp (executable) regions for a specific library
    static std::vector<Region> GetExecutableRegions(const char* libName) {
        std::vector<Region> regions;
        FILE* maps = fopen("/proc/self/maps", "r");
        if (!maps) return regions;

        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (!strstr(line, libName)) continue;
            if (!strstr(line, "r-xp"))  continue;

            uintptr_t start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                regions.push_back({ start, end });
            }
        }
        fclose(maps);
        return regions;
    }

    // Scan a library for an IDA-style byte signature
    // Returns absolute address of first match, 0 on miss
    // Signature format: "AA BB ? ? CC DD" (? is wildcard byte)
    static uintptr_t FindPattern(const char* libName, const char* sig) {
        auto regions = GetExecutableRegions(libName);
        if (regions.empty()) {
            MLOGE("PatternScanner: no executable regions for %s", libName);
            return 0;
        }

        std::vector<uint8_t>  pattern;
        std::vector<bool>     wildcard;
        ParseSignature(sig, pattern, wildcard);

        size_t patLen = pattern.size();

        for (auto& reg : regions) {
            uintptr_t addr = reg.start;
            while (addr + patLen <= reg.end) {
                if (MatchAt(addr, pattern, wildcard)) {
                    MLOGI("PatternScanner: found at 0x%lx in %s", addr, libName);
                    return addr;
                }
                addr++;
            }
        }

        MLOGE("PatternScanner: pattern not found in %s", libName);
        return 0;
    }

    // Scan within a specific address range
    static uintptr_t FindPatternInRange(uintptr_t start, uintptr_t end, const char* sig) {
        std::vector<uint8_t> pattern;
        std::vector<bool>    wildcard;
        ParseSignature(sig, pattern, wildcard);

        size_t patLen = pattern.size();
        for (uintptr_t addr = start; addr + patLen <= end; addr++) {
            if (MatchAt(addr, pattern, wildcard))
                return addr;
        }
        return 0;
    }

    // Resolve relative address from instruction:
    //   ldr x0, [pc + offset]  or  b label
    //   instructionAddress + offsetInInstruction + sizeof(imm) + relativeOffset
    static uintptr_t ResolveRelAddr(uintptr_t instrAddr, int offsetInInstr, int instrSize) {
        int32_t relOff = Memory::Read<int32_t>(instrAddr + offsetInInstr);
        return instrAddr + instrSize + relOff;
    }

    // ARM64 decode: extract page-offset from ADRP+ADD/LDR pair
    // ADRP Xn, label  →  PC-relative 4KB page address
    static uintptr_t ResolveADRP(uintptr_t adrpAddr) {
        uint32_t instr  = Memory::Read<uint32_t>(adrpAddr);
        int64_t  immhi  = (int64_t)((instr >> 5) & 0x7FFFF) << 2;
        int64_t  immlo  = (int64_t)((instr >> 29) & 0x3);
        int64_t  imm21  = (immhi | immlo);
        // Sign-extend from 21 bits
        if (imm21 & (1 << 20)) imm21 |= ~((1 << 21) - 1);
        uintptr_t page  = adrpAddr & ~0xFFFULL;
        return page + (imm21 << 12);
    }

private:
    static void ParseSignature(const char* sig,
                               std::vector<uint8_t>& bytes,
                               std::vector<bool>&    mask)
    {
        std::string s(sig);
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == ' ') { i++; continue; }
            if (s[i] == '?') {
                bytes.push_back(0x00);
                mask.push_back(false);  // wildcard
                if (i + 1 < s.size() && s[i+1] == '?') i++;  // skip ??
                i++;
            } else {
                // parse two hex chars
                char hex[3] = { s[i], (i+1 < s.size() ? s[i+1] : '0'), '\0' };
                bytes.push_back((uint8_t)strtol(hex, nullptr, 16));
                mask.push_back(true);
                i += 2;
            }
        }
    }

    static bool MatchAt(uintptr_t addr,
                        const std::vector<uint8_t>& pat,
                        const std::vector<bool>&    mask)
    {
        for (size_t j = 0; j < pat.size(); j++) {
            if (!mask[j]) continue;  // wildcard, skip
            if (*reinterpret_cast<volatile uint8_t*>(addr + j) != pat[j])
                return false;
        }
        return true;
    }
};
