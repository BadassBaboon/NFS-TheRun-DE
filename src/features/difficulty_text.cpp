#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>

// Renaming Extreme in the menus.
//
// The menu text is a localised string loaded out of the game's loc bundles into
// the heap, so there is no fixed address for it: the buffers land wherever the
// allocator puts them and move every launch. They are narrow ASCII, which the
// spacing of the two buffers confirms — the label and its description sat 0x87
// bytes apart, and an odd gap rules out UTF-16.
//
// So the strings are found by content rather than by address. Two rules keep that
// from being reckless:
//
//   1. The description is located by a distinctive fragment ("forewarned") rather
//      than by its full text, so a difference in punctuation or wording between
//      game versions does not make the search silently fail.
//   2. The label "EXTREME" appears in plenty of unrelated places — a plain scan
//      turns up 28 hits. It is only accepted when it is null-terminated on BOTH
//      sides, which makes it a whole string rather than a fragment of a longer
//      one, and only when it is near the description buffer we already found.
//
// Replacements are written into the game's own buffer, so they cannot be longer
// than the space that is actually there. The capacity of a buffer is its own text
// plus any run of zero bytes after its terminator, since the allocator commonly
// pads. Anything longer than that is truncated rather than allowed to run into
// whatever string is stored next.

namespace {
    // Distinctive enough to be unique, short enough to survive rewording.
    const char* kDescriptionAnchor = "forewarned";
    const char* kLabelText         = "EXTREME";

    // How far either side of the anchor the label is looked for. The two buffers
    // were 0x87 apart when found by hand; this is generous without being a
    // whole-heap search.
    const size_t kLabelSearchRadius = 0x2000;

    // Bounds on walking out to a string's null terminators.
    const size_t kMaxStringLen = 512;
    // Padding past a terminator that may be used. Kept small deliberately: a long
    // run of zeroes is more likely to be a different allocation than slack.
    const size_t kMaxPadding = 16;

    bool   g_Done = false;
    int    g_ScanTicks = 0;
    // The ticker runs every 16ms. Scanning the address space is not free, so it
    // is retried about every five seconds until the menu text has been loaded.
    const int kTicksBetweenScans = 300;

    bool Writable(uintptr_t addr, size_t size) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                             PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return addr + size <= regionEnd;
    }

    // One hit is all this needs; the shared scanner covers the whole user address
    // space, which matters because these buffers land above 0xF0000000.
    uintptr_t ScanWritable(const char* needle, size_t len) {
        uintptr_t hit = 0;
        return Memory::ScanWritableAll(needle, len, &hit, 1) ? hit : 0;
    }

    // Given an address somewhere inside a null-terminated string, finds the start
    // of that string and how many bytes may be written to it.
    bool DescribeBuffer(uintptr_t inside, uintptr_t& outStart, size_t& outCapacity) {
        uintptr_t start = inside;
        size_t back = 0;
        while (back < kMaxStringLen) {
            if (!Memory::IsReadable(start - 1, 1)) return false;
            if (*reinterpret_cast<const char*>(start - 1) == '\0') break;
            --start;
            ++back;
        }
        if (back >= kMaxStringLen) return false;

        size_t len = 0;
        while (len < kMaxStringLen) {
            if (!Memory::IsReadable(start + len, 1)) return false;
            if (*reinterpret_cast<const char*>(start + len) == '\0') break;
            ++len;
        }
        if (len >= kMaxStringLen) return false;

        // Usable space is the text itself plus any zero padding that follows its
        // terminator, minus the one byte the new terminator needs.
        size_t pad = 0;
        while (pad < kMaxPadding && Memory::IsReadable(start + len + 1 + pad, 1)
               && *reinterpret_cast<const char*>(start + len + 1 + pad) == '\0') {
            ++pad;
        }

        outStart    = start;
        outCapacity = len + pad;
        return true;
    }

    // Writes `text` into the buffer, truncated to what actually fits.
    void WriteInPlace(const char* what, uintptr_t start, size_t capacity, const std::string& text) {
        size_t len = text.size();
        bool truncated = false;
        if (len > capacity) {
            len = capacity;
            truncated = true;
        }
        if (!Writable(start, capacity + 1)) {
            Logger::Log("Extreme rename: %s buffer at 0x%08X is not writable. Skipped.", what, start);
            return;
        }

        DWORD old;
        if (!VirtualProtect(reinterpret_cast<LPVOID>(start), capacity + 1, PAGE_READWRITE, &old)) {
            Logger::Log("Extreme rename: could not unprotect the %s buffer at 0x%08X. Skipped.", what, start);
            return;
        }
        std::memcpy(reinterpret_cast<void*>(start), text.c_str(), len);
        std::memset(reinterpret_cast<void*>(start + len), 0, capacity + 1 - len);
        VirtualProtect(reinterpret_cast<LPVOID>(start), capacity + 1, old, &old);

        if (truncated) {
            Logger::Log("Extreme rename: %s set to \"%.*s\" — TRUNCATED, the game's buffer holds "
                        "only %u characters.",
                        what, static_cast<int>(len), text.c_str(), static_cast<unsigned>(capacity));
        } else {
            Logger::Log("Extreme rename: %s at 0x%08X set to \"%s\" (%u of %u characters used).",
                        what, start, text.c_str(),
                        static_cast<unsigned>(len), static_cast<unsigned>(capacity));
        }
    }

    // Replaces every whole-string "EXTREME" near the description buffer, and
    // returns how many were rewritten.
    //
    // There is more than one. The blob holds at least two copies back to back —
    // 0xF5E56A24 and 0xF5E56A2C in one session, exactly the eight bytes of
    // "EXTREME\0" apart — and the menu draws the second. Stopping at the first
    // match renamed a string nothing was displaying, which looked like the rename
    // silently failing.
    //
    // Requiring a null on both sides is what separates these from the couple of
    // dozen other places those seven letters appear as part of something longer.
    int RenameAllLabels(uintptr_t nearAddr) {
        const size_t labelLen = std::strlen(kLabelText);

        // Clamp the window to the one region the description lives in, so the scan
        // below needs no per-byte readability check. Testing each byte with
        // VirtualQuery would mean sixteen thousand calls for a single lookup.
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(nearAddr), &mbi, sizeof(mbi))) return 0;
        uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t regionEnd  = regionBase + mbi.RegionSize;

        uintptr_t from = (nearAddr > kLabelSearchRadius) ? nearAddr - kLabelSearchRadius : regionBase;
        uintptr_t to   = nearAddr + kLabelSearchRadius;
        if (from < regionBase + 1) from = regionBase + 1;   // room for the leading null
        if (to > regionEnd - (labelLen + 1)) to = regionEnd - (labelLen + 1);

        int renamed = 0;
        for (uintptr_t a = from; a < to; ++a) {
            const char* p = reinterpret_cast<const char*>(a);
            if (p[-1] != '\0') continue;
            if (p[labelLen] != '\0') continue;
            if (std::memcmp(p, kLabelText, labelLen) != 0) continue;

            uintptr_t start;
            size_t    capacity;
            if (DescribeBuffer(a, start, capacity)) {
                WriteInPlace("label", start, capacity, Difficulty::kNewLabel);
                ++renamed;
            }
            // Past this copy's terminator. The text just written is shorter or
            // equal, so it cannot match again anyway.
            a += labelLen;
        }
        return renamed;
    }
}

namespace Features {
    void UpdateDifficultyText() {
        if (!g_Config.RunForYourLife) return;
        if (g_Done) return;

        // The text only exists once the menus have loaded their localisation, so
        // this keeps looking rather than giving up after one pass.
        if (++g_ScanTicks < kTicksBetweenScans) return;
        g_ScanTicks = 0;

        uintptr_t anchor = ScanWritable(kDescriptionAnchor, std::strlen(kDescriptionAnchor));
        if (!anchor) return;

        uintptr_t descStart;
        size_t    descCapacity;
        if (!DescribeBuffer(anchor, descStart, descCapacity)) return;

        WriteInPlace("description", descStart, descCapacity, Difficulty::kNewDescription);

        int renamed = RenameAllLabels(descStart);
        if (renamed == 0) {
            Logger::Log("Extreme rename: description renamed, but no standalone \"%s\" string was "
                        "found within 0x%X bytes of it. The label is unchanged.",
                        kLabelText, static_cast<unsigned>(kLabelSearchRadius));
        } else {
            Logger::Log("Extreme rename: %d copies of \"%s\" replaced.", renamed, kLabelText);
        }

        g_Done = true;
    }
}
