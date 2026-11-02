#include "dcc_arm32_hook.h"
#include <stdio.h>

int main(void)
{
    static const uint32_t setup[2] = {0xe92d4ff0u,0xe59f4f60u};
    static const uint32_t start[2] = {0xe92d4ff0u,0xe24dd0c4u};
    struct dcc_arm32_hook_plan plan;
    if (dcc_arm32_plan_hook(0x100165e0u,0x78004000u,setup,
                            DCC_ARM32_RELOCATE_LDR_R4_LITERAL,0x00123456u,&plan)) return 1;
    if (plan.patch[0] != 0xe51ff004u || plan.patch[1] != 0x78004000u ||
        plan.trampoline_words != 6u) return 2;
    if (plan.trampoline[0] != setup[0] || plan.trampoline[1] != 0xe59f4004u ||
        plan.trampoline[2] != 0xe59ff004u || plan.trampoline[4] != 0x00123456u ||
        plan.trampoline[5] != 0x100165e8u) return 3;
    if (dcc_arm32_plan_hook(0x10014124u,0x78005000u,start,DCC_ARM32_COPY_TWO,0,&plan)) return 4;
    if (plan.trampoline_words != 4u || plan.trampoline[0] != start[0] ||
        plan.trampoline[1] != start[1] || plan.trampoline[2] != 0xe51ff004u ||
        plan.trampoline[3] != 0x1001412cu) return 5;
    if (!dcc_arm32_plan_hook(0x100165e0u,0x78004000u,start,
                             DCC_ARM32_RELOCATE_LDR_R4_LITERAL,0,&plan)) return 6;
    puts("ARM32 exact-prologue hook planning checks passed");
    return 0;
}
