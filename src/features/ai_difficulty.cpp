#include "features.h"
#include "../config.h"
#include "../memory.h"
#include "../logger.h"
#include <windows.h>
#include <cstdint>

// AI performance scalars, used by Run For Your Life.
//
// Everything else the mode does makes the PLAYER weaker. This is the half that
// makes the AI better, which is what separates a difficulty from a handicap.
//
// sub_1261E40 copies an AI performance struct and multiplies two of its fields by
// per-difficulty scalars:
//
//     0x01261EA1  movss xmm0,[eax] / movss xmm1,[esi+08]   -> a2+8  *= glue
//     0x01261ED9  movss xmm0,[eax] / movss xmm1,[esi+0C]   -> a2+12 *= skill
//
// In both, [eax] is the scalar and [esi+N] is the value it multiplies, so scaling
// xmm0 scales the multiplier the game is about to apply. These are the exact two
// sites mRally2's Master Table overwrites for its "Glue Scalar" and "Difficulty
// Scalar" scripts, with 0.7 and 5.0.
//
// WHY THIS IS SAFER THAN THE OTHER HOOKS. Twice now a field that looked
// player-specific turned out to be written for AI cars too — the driving assists
// and then nitrous — and suppressing it for everyone made races easier rather
// than harder. That cannot happen here: this function exists to apply difficulty
// scaling to AI performance, so there is no player car flowing through it.
//
// Each site is nine bytes, which is room for the 5-byte jump plus padding. The
// caves reproduce both original instructions and multiply in between. mulss does
// not touch the flags register, so unlike the input-state caves there is nothing
// to reason about on the way back.
//
// The two scalars are INI-tunable ON PURPOSE, and only for now. Neither direction
// is known: 5.0 on skill is clearly "make the AI fast", but glue is rubber-banding
// and whether raising or lowering it makes a race harder needs to be found by
// feel. Once the right values are known they get hardcoded next to the rest of the
// mode's rules in features.h and the knobs come out, exactly as happened with the
// kickup particle scale.

extern "C" {
    float     g_AiSkillScale = 1.0f;
    float     g_AiGlueScale  = 1.0f;
    uintptr_t g_pAiSkillReturn = 0;
    uintptr_t g_pAiGlueReturn  = 0;
    void AiSkillHookAsm();
    void AiGlueHookAsm();
}

asm(
    ".text\n"
    ".globl _AiGlueHookAsm\n"
    "_AiGlueHookAsm:\n"
    "    movss (%eax), %xmm0\n"            // the scalar the game read
    "    mulss _g_AiGlueScale, %xmm0\n"    // ours on top of it
    "    movss 0x8(%esi), %xmm1\n"         // the value it will be applied to
    "    jmpl *_g_pAiGlueReturn\n"
);

asm(
    ".text\n"
    ".globl _AiSkillHookAsm\n"
    "_AiSkillHookAsm:\n"
    "    movss (%eax), %xmm0\n"
    "    mulss _g_AiSkillScale, %xmm0\n"
    "    movss 0xC(%esi), %xmm1\n"
    "    jmpl *_g_pAiSkillReturn\n"
);

namespace {
    const uintptr_t kSiteGlue  = 0x1261EA1 - 0x400000;
    const uintptr_t kSiteSkill = 0x1261ED9 - 0x400000;

    const uint8_t kExpectGlue[9]  = { 0xF3,0x0F,0x10,0x00, 0xF3,0x0F,0x10,0x4E,0x08 };
    const uint8_t kExpectSkill[9] = { 0xF3,0x0F,0x10,0x00, 0xF3,0x0F,0x10,0x4E,0x0C };

    bool g_Installed = false;
    bool g_Logged = false;
    float g_LastSkill = -1.0f;
    float g_LastGlue  = -1.0f;

    void InstallSite(const char* name, uintptr_t off, const uint8_t* expect,
                     void* cave, uintptr_t* pReturn) {
        uintptr_t addr = Memory::GetGameBase() + off;
        if (!Memory::VerifyBytes(addr, expect, 9)) {
            Logger::Log("AI scalars: site %s ABORTED at 0x%08X, bytes [%s] don't match.",
                        name, addr, Memory::BytesToHex(addr, 9).c_str());
            return;
        }
        *pReturn = addr + 9;
        if (Memory::InjectJMP(addr, reinterpret_cast<uintptr_t>(cave), 9)) {
            Logger::Log("AI scalars: site %s installed at 0x%08X.", name, addr);
            g_Installed = true;
        } else {
            Logger::Log("AI scalars: site %s FAILED at 0x%08X.", name, addr);
        }
    }
}

namespace Features {
    void InitAiDifficulty() {
        if (!g_Config.RunForYourLife) return;
        InstallSite("glue",  kSiteGlue,  kExpectGlue,
                    reinterpret_cast<void*>(AiGlueHookAsm),  &g_pAiGlueReturn);
        InstallSite("skill", kSiteSkill, kExpectSkill,
                    reinterpret_cast<void*>(AiSkillHookAsm), &g_pAiSkillReturn);
    }

    void UpdateAiDifficulty() {
        if (!g_Installed) return;

        // 1.0 leaves the game's own scaling untouched, so a disengaged mode is
        // indistinguishable from the mod not being here.
        const bool rfyl = Difficulty::RunForYourLifeActive();
        float skill = rfyl ? g_Config.DeadlyAiSkillScale : 1.0f;
        float glue  = rfyl ? g_Config.DeadlyAiGlueScale  : 1.0f;

        g_AiSkillScale = skill;
        g_AiGlueScale  = glue;

        if ((skill != g_LastSkill || glue != g_LastGlue) && (g_Logged || rfyl)) {
            Logger::Log("AI scalars: skill x%.2f, glue x%.2f.", skill, glue);
            g_Logged = true;
        }
        g_LastSkill = skill;
        g_LastGlue  = glue;
    }
}
