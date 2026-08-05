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

    uintptr_t FindPatternProcess(const char* pattern, const char* mask) {
        uintptr_t results[1];
        if (FindAllPatternsProcess(pattern, mask, results, 1) > 0) {
            return results[0];
        }
        return 0;
    }

    uintptr_t FindPatternRange(uintptr_t minAddr, uintptr_t maxAddr, const char* pattern, const char* mask) {
        size_t patternLen = std::strlen(mask);
        uintptr_t start = minAddr;

        MEMORY_BASIC_INFORMATION mbi;
        while (start < maxAddr) {
            if (VirtualQuery(reinterpret_cast<void*>(start), &mbi, sizeof(mbi))) {
                if ((mbi.State == MEM_COMMIT) && !(mbi.Protect & PAGE_GUARD) && !(mbi.Protect & PAGE_NOACCESS)) {
                    if (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ)) {
                        uintptr_t curr = start;
                        uintptr_t regionEnd = start + mbi.RegionSize;
                        if (regionEnd > maxAddr) regionEnd = maxAddr;

                        if (regionEnd - curr >= patternLen) {
                            uintptr_t found = FindPattern(curr, regionEnd - curr, pattern, mask);
                            if (found != 0) {
                                return found;
                            }
                        }
                    }
                }
                start += mbi.RegionSize;
            } else {
                start += 0x1000;
            }
        }
        return 0;
    }

    size_t FindAllPatternsProcess(const char* pattern, const char* mask, uintptr_t* outAddresses, size_t maxCount) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        uintptr_t start = reinterpret_cast<uintptr_t>(sysInfo.lpMinimumApplicationAddress);
        // Bound max address to 0x20000000 to search only process heap/game memory and avoid system DLLs
        uintptr_t maxAddr = 0x20000000;
        size_t patternLen = std::strlen(mask);
        size_t foundCount = 0;

        MEMORY_BASIC_INFORMATION mbi;
        while (start < maxAddr && foundCount < maxCount) {
            if (VirtualQuery(reinterpret_cast<void*>(start), &mbi, sizeof(mbi))) {
                if ((mbi.State == MEM_COMMIT) && !(mbi.Protect & PAGE_GUARD) && !(mbi.Protect & PAGE_NOACCESS)) {
                    if (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ)) {
                        uintptr_t curr = start;
                        uintptr_t regionEnd = start + mbi.RegionSize;
                        if (regionEnd > maxAddr) regionEnd = maxAddr;

                        while (curr < regionEnd && foundCount < maxCount) {
                            size_t remaining = regionEnd - curr;
                            if (remaining < patternLen) break;

                            uintptr_t found = FindPattern(curr, remaining, pattern, mask);
                            if (found != 0) {
                                outAddresses[foundCount++] = found;
                                curr = found + 1; // Move past current match
                            } else {
                                break;
                            }
                        }
                    }
                }
                start += mbi.RegionSize;
            } else {
                start += 0x1000;
            }
        }
        return foundCount;
    }
}
