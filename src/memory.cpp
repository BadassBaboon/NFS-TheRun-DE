#include "memory.h"
#include "logger.h"
#include <psapi.h>
#include <cstring>
#include <cstdio>
#include <vector>

namespace Memory {
    // Returns true if [address, address+size) is committed and readable.
    bool IsReadable(uintptr_t address, size_t size) {
        if (!address || !size || size > UINTPTR_MAX - address) return false;
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
            const uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (next <= curr) return false;
            curr = next;
        }
        return true;
    }

    bool ReadBytes(uintptr_t address, void* out, size_t size) {
        if (!out || !IsReadable(address, size)) return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address),
                                 out, size, &copied) && copied == size;
    }

    bool WriteBytes(uintptr_t address, const void* bytes, size_t size) {
        if (!bytes || !address || !size || size > UINTPTR_MAX - address) return false;
        const uintptr_t end = address + size;
        for (uintptr_t curr = address; curr < end;) {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(reinterpret_cast<LPCVOID>(curr), &mbi, sizeof(mbi)) ||
                mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                !(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                 PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
            const uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (next <= curr) return false;
            curr = next;
        }
        SIZE_T copied = 0;
        return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(address),
                                  bytes, size, &copied) && copied == size;
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
        if (!needle || !out || len == 0 || maxOut == 0 || len > sizeof(g_ScanBuffer)) return 0;

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

                while (pos <= regionEnd && len <= regionEnd - pos && found < maxOut) {
                    // Overlap consecutive chunks by len-1 so a match lying across
                    // a chunk boundary is still seen.
                    size_t want = kChunk;
                    if (want > regionEnd - pos) want = regionEnd - pos;

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
        if (!bytes || !IsReadable(address, size)) return false;
        std::vector<uint8_t> copy(size);
        return ReadBytes(address, copy.data(), size) && std::memcmp(copy.data(), bytes, size) == 0;
    }

    std::string BytesToHex(uintptr_t address, size_t size) {
        if (!IsReadable(address, size)) return "<unreadable>";
        std::string out;
        char buf[4];
        std::vector<uint8_t> copy(size);
        if (!ReadBytes(address, copy.data(), size)) return "<unreadable>";
        const uint8_t* p = copy.data();
        for (size_t i = 0; i < size; ++i) {
            std::snprintf(buf, sizeof(buf), "%02X", p[i]);
            if (i) out += ' ';
            out += buf;
        }
        return out;
    }

    bool PatchBytes(uintptr_t address, const void* bytes, size_t size) {
        if (!bytes || !IsReadable(address, size)) return false;
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) return false;
        // VirtualProtect returns only the first page's old protection. Refuse
        // mixed-protection spans rather than restoring the wrong permissions.
        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (regionEnd <= address || size > regionEnd - address) return false;
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::Log("PatchBytes failed VirtualProtect at 0x%08X", address);
            return false;
        }

        SIZE_T copied = 0;
        const bool written = WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
                                                bytes, size, &copied) && copied == size;

        if (!VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect))
            Logger::Log("PatchBytes: failed to restore protection at 0x%08X", address);
        if (!FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size))
            Logger::Log("PatchBytes: failed to flush instruction cache at 0x%08X", address);
        // The patch is already installed, even if post-write cleanup failed.
        return written;
    }

    bool PatchNOP(uintptr_t address, size_t size) {
        if (!IsReadable(address, size)) return false;
        std::vector<uint8_t> patch(size, 0x90);
        return PatchBytes(address, patch.data(), size);
    }

    bool InjectJMP(uintptr_t src, uintptr_t dest, size_t size) {
        if (!dest || size < 5 || !IsReadable(src, size)) return false;
        std::vector<uint8_t> patch(size, 0x90);
        patch[0] = 0xE9;
        uint32_t relativeOffset = static_cast<uint32_t>(dest - src - 5);
        std::memcpy(patch.data() + 1, &relativeOffset, sizeof(relativeOffset));
        return PatchBytes(src, patch.data(), size);
    }

    uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern, const char* mask) {
        if (!pattern || !mask || !IsReadable(start, size)) return 0;
        size_t patternLen = std::strlen(mask);
        if (!patternLen || size < patternLen) return 0;
        std::vector<char> copy(size);
        if (!ReadBytes(start, copy.data(), size)) return 0;

        for (size_t i = 0; i <= size - patternLen; ++i) {
            bool found = true;
            for (size_t j = 0; j < patternLen; ++j) {
                if (mask[j] != '?' && pattern[j] != copy[i + j]) {
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

        const uintptr_t base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
        if (modInfo.SizeOfImage > UINTPTR_MAX - base) return 0;
        const uintptr_t end = base + modInfo.SizeOfImage;
        for (uintptr_t pos = base; pos < end;) {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(reinterpret_cast<LPCVOID>(pos), &mbi, sizeof(mbi))) break;
            uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            if (next <= pos) break;
            if (next > end) next = end;
            // A module can contain uncommitted or inaccessible gaps.
            if (IsReadable(pos, next - pos)) {
                const uintptr_t found = FindPattern(pos, next - pos, pattern, mask);
                if (found) return found;
            }
            pos = next;
        }
        return 0;
    }

}
