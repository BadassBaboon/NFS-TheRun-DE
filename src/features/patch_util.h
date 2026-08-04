#ifndef PATCH_UTIL_H
#define PATCH_UTIL_H

#include <cstdint>
#include <cstddef>

// Shared helpers for signature-aware code patching. All addresses are given as
// module offsets (relative to the game's base) so they survive relocation/ASLR.
namespace PatchUtil {
    // NOP `size` bytes at base+offset ONLY if the bytes match `expected`.
    // Skips (and logs) on mismatch/unreadable/already-NOP. Use when a verified
    // byte signature is known — the safe, preferred form.
    bool VerifiedNop(const char* name, uintptr_t offset,
                     const uint8_t* expected, size_t size);

    // NOP `size` bytes at base+offset without a known signature, but still
    // guarded: skips unreadable regions and regions that are already all-NOP
    // (double-apply), and logs the original bytes so a signature can be
    // captured and promoted to VerifiedNop later.
    bool CaptureNop(const char* name, uintptr_t offset, size_t size);

    // Overwrite `size` bytes at base+offset with `replacement`, but only if the
    // current bytes match `expected`. Skips (and logs) on mismatch. Also skips
    // (reporting success) if the bytes already equal `replacement`, so a second
    // apply is a no-op. Use for verified opcode flips / immediate changes.
    bool VerifiedPatch(const char* name, uintptr_t offset,
                       const uint8_t* expected, const uint8_t* replacement, size_t size);
}

#endif // PATCH_UTIL_H
