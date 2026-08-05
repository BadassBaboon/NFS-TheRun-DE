#ifndef MEMORY_H
#define MEMORY_H

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <string>

namespace Memory {
    // Base address of the main game module (accounts for ASLR/relocation).
    uintptr_t GetGameBase();

    // True if `size` bytes at `address` sit on committed, readable pages. Use
    // before walking a pointer chain, where any link may be null or stale.
    bool IsReadable(uintptr_t address, size_t size);

    // Scans every committed, writable region of the whole user address space for
    // `needle`, writing up to `maxOut` hits into `out` and returning the count.
    //
    // FindAllPatternsProcess stops at 0x20000000 and cannot be used for heap data:
    // the game is large-address aware and its EBX and localisation buffers land
    // above 0xF0000000.
    size_t ScanWritableAll(const void* needle, size_t len, uintptr_t* out, size_t maxOut);

    // Returns true if the `size` bytes at `address` exactly match `bytes`.
    // Safe against unreadable pages (returns false instead of crashing).
    bool VerifyBytes(uintptr_t address, const void* bytes, size_t size);

    // Formats up to `size` bytes at `address` as "AA BB CC" for logging.
    // Returns "<unreadable>" if the page cannot be read.
    std::string BytesToHex(uintptr_t address, size_t size);

    bool PatchBytes(uintptr_t address, const void* bytes, size_t size);
    bool PatchNOP(uintptr_t address, size_t size);
    bool InjectJMP(uintptr_t src, uintptr_t dest, size_t size = 5);
    uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern, const char* mask);
    uintptr_t FindPatternModule(HMODULE module, const char* pattern, const char* mask);
    uintptr_t FindPatternProcess(const char* pattern, const char* mask);
    uintptr_t FindPatternRange(uintptr_t minAddr, uintptr_t maxAddr, const char* pattern, const char* mask);
    size_t FindAllPatternsProcess(const char* pattern, const char* mask, uintptr_t* outAddresses, size_t maxCount);
}

#endif // MEMORY_H
