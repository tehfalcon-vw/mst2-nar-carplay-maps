#ifndef DCC_ARM32_HOOK_H
#define DCC_ARM32_HOOK_H

#include <stddef.h>
#include <stdint.h>

#define DCC_ARM_LDR_PC_ABSOLUTE 0xe51ff004u

enum dcc_arm32_relocation {
    DCC_ARM32_COPY_TWO = 0,
    DCC_ARM32_RELOCATE_LDR_R4_LITERAL = 1
};

struct dcc_arm32_hook_plan {
    uint32_t patch[2];
    uint32_t trampoline[6];
    size_t trampoline_words;
};

/* Creates the instruction words only; allocation, page replacement and cache
 * maintenance remain target-runtime responsibilities. The literal argument is
 * the word loaded by the original PC-relative LDR, not its address. */
int dcc_arm32_plan_hook(uint32_t target, uint32_t replacement,
                        const uint32_t original[2],
                        enum dcc_arm32_relocation relocation,
                        uint32_t literal, struct dcc_arm32_hook_plan *plan);

#endif
