#include "dcc_arm32_hook.h"
#include <string.h>

int dcc_arm32_plan_hook(uint32_t target, uint32_t replacement,
                        const uint32_t original[2],
                        enum dcc_arm32_relocation relocation,
                        uint32_t literal, struct dcc_arm32_hook_plan *plan)
{
    if (!target || !replacement || !original || !plan || (target & 3u) ||
        (replacement & 3u)) return -1;
    memset(plan, 0, sizeof(*plan));
    plan->patch[0] = DCC_ARM_LDR_PC_ABSOLUTE;
    plan->patch[1] = replacement;
    if (relocation == DCC_ARM32_COPY_TWO) {
        plan->trampoline[0] = original[0];
        plan->trampoline[1] = original[1];
        plan->trampoline[2] = DCC_ARM_LDR_PC_ABSOLUTE;
        plan->trampoline[3] = target + 8u;
        plan->trampoline_words = 4u;
        return 0;
    }
    if (relocation == DCC_ARM32_RELOCATE_LDR_R4_LITERAL &&
        (original[1] & 0xfffff000u) == 0xe59f4000u) {
        plan->trampoline[0] = original[0];
        plan->trampoline[1] = 0xe59f4004u; /* ldr r4,[pc,#4] -> word 4 */
        plan->trampoline[2] = 0xe59ff004u; /* ldr pc,[pc,#4] -> word 5 */
        plan->trampoline[3] = 0xe1a00000u; /* nop/alignment */
        plan->trampoline[4] = literal;
        plan->trampoline[5] = target + 8u;
        plan->trampoline_words = 6u;
        return 0;
    }
    return -1;
}
