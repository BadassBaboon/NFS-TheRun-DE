#include "memory.h"
#include "logger.h"
#include <psapi.h>
#include <cstring>
#include <cstdio>

namespace Memory {
    // Returns true if [address, address+size) is committed and readable.
    bool IsReadable(uintptr_t address, size_t size) {
        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t curr = address;
        uintptr_t end = address + size;
        while (curr < end) {
            if (!VirtualQuery(reinterpret_cast<void*>(curr), &mbi, sizeof(mbi))) return false;
            if (mbi.State != MEM_COMMIT) return false;
            if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
            if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                 PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
                return false;
            }
            curr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        }
        return true;
    }

    // Staging buffer for ScanWritableAll. File-scope rather than a local because
    // 64 KB does not belong on the ticker thread's stack. Only that one function
    // touches it, and only from the ticker thread.
    static uint8_t g_ScanBuffer[0x10000];

    // Base of this .asi, worked out from an address known to live inside it.
    // ScanWritableAll skips our own module: the needle it is asked to find is by
    // definition also sitting in our memory, and matching that would produce a
    // phantom candidate on every single search.
    static uintptr_t OwnModuleBase() {
        static uintptr_t base = 0;
        if (base == 0) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(reinterpret_cast<LPCVOID>(&g_ScanBuffer), &mbi, sizeof(mbi))) {
                base = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
            }
        }
        return base;
    }

    size_t ScanWritableAll(const void* needle, size_t len, uintptr_t* out, size_t maxOut) {
        if (len == 0 || maxOut == 0 || len > sizeof(g_ScanBuffer)) return 0;

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t addr    = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
        uintptr_t maxAddr = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

        const uint8_t* pat = static_cast<const uint8_t*>(needle);
        size_t found = 0;

        MEMORY_BASIC_INFORMATION mbi;
        while (addr < maxAddr && found < maxOut) {
            if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
                addr += 0x1000;
                continue;
            }
            uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            uintptr_t next = regionBase + mbi.RegionSize;
            if (next <= addr) break;  // guards against a zero-size region

            bool usable = mbi.State == MEM_COMMIT
                       && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
                       && (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                          PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
                       && reinterpret_cast<uintptr_t>(mbi.AllocationBase) != OwnModuleBase();
            if (usable && mbi.RegionSize >= len) {
                // The pages are NOT read directly. The game allocates and frees
                // constantly while loading, so a region can be freed between the
                // VirtualQuery above and the comparison below, and touching it
                // then is an access violation that takes the game down.
                // ReadProcessMemory on our own process fails cleanly instead, so
                // a region that disappears mid-scan is skipped rather than fatal.
                const size_t kChunk = 0x10000;
                uintptr_t pos = regionBase;
                uintptr_t regionEnd = regionBase + mbi.RegionSize;

                while (pos + len <= regionEnd && found < maxOut) {
                    // Overlap consecutive chunks by len-1 so a match lying across
                    // a chunk boundary is still seen.
                    size_t want = kChunk;
                    if (pos + want > regionEnd) want = regionEnd - pos;

                    SIZE_T got = 0;
                    if (!ReadProcessMemory(GetCurrentProcess(),
                                           reinterpret_cast<LPCVOID>(pos),
                                           g_ScanBuffer, want, &got) || got < len) {
                        pos += kChunk;
                        continue;
                    }

                    size_t end = got - len;
                    for (size_t i = 0; i <= end && found < maxOut; ++i) {
                        if (g_ScanBuffer[i] == pat[0] &&
                            std::memcmp(g_ScanBuffer + i, pat, len) == 0) {
                            out[found++] = pos + i;
                        }
                    }
                    pos += (got >= len) ? (got - (len - 1)) : kChunk;
                }
            }
            addr = next;
        }
        return found;
    }

    uintptr_t GetGameBase() {
        return reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));
    }

    bool VerifyBytes(uintptr_t address, const void* bytes, size_t size) {
        if (!IsReadable(address, size)) return false;
        return std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0;
    }

    std::string BytesToHex(uintptr_t address, size_t size) {
        if (!IsReadable(address, size)) return "<unreadable>";
        std::string out;
        char buf[4];
        const uint8_t* p = reinterpret_cast<const uint8_t*>(address);
        for (size_t i = 0; i < size; ++i) {
            std::snprintf(buf, sizeof(buf), "%02X", p[i]);
            if (i) out += ' ';
            out += buf;
        }
        return out;
    }

    bool PatchBytes(uintptr_t address, const void* bytes, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::Log("PatchBytes failed VirtualProtect at 0x%08X", address);
            return false;
        }

        std::memcpy(reinterpret_cast<void*>(address), bytes, size);

        VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        return true;
    }

    bool PatchNOP(uintptr_t address, size_t size) {
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::Log("PatchNOP failed VirtualProtect at 0x%08X", address);
            return false;
        }

        std::memset(reinterpret_cast<void*>(address), 0x90, size);

        VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        return true;
    }

    bool InjectJMP(uintptr_t src, uintptr_t dest, size_t size) {
        if (size < 5) return false;

        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(src), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::Log("InjectJMP failed VirtualProtect at 0x%08X", src);
            return false;
        }

        // Relative JMP opcode: 0xE9 <relative address>
        *reinterpret_cast<uint8_t*>(src) = 0xE9;
        uint32_t relativeOffset = static_cast<uint32_t>(dest - src - 5);
        *reinterpret_cast<uint32_t*>(src + 1) = relativeOffset;

        // Fill remaining bytes with NOPs if size > 5
        if (size > 5) {
            std::memset(reinterpret_cast<void*>(src + 5), 0x90, size - 5);
        }

        VirtualProtect(reinterpret_cast<void*>(src), size, oldProtect, &oldProtect);
        return true;
    }

    uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern, const char* mask) {
        size_t patternLen = std::strlen(mask);
        if (size < patternLen) return 0;

        for (size_t i = 0; i <= size - patternLen; ++i) {
            bool found = true;
            for (size_t j = 0; j < patternLen; ++j) {
                if (mask[j] != '?' && pattern[j] != *reinterpret_cast<const char*>(start + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return start + i;
            }
        }
        return 0;
    }

    uintptr_t FindPatternModule(HMODULE module, const char* pattern, const char* mask) {
        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo))) {
            Logger::Log("FindPatternModule failed GetModuleInformation");
            return 0;
        }

        return FindPattern(reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll), modInfo.SizeOfImage, pattern, mask);
    }

}
