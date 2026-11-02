#ifndef DCC_RUNTIME_HOOK_H
#define DCC_RUNTIME_HOOK_H

#include "dcc_arm32_hook.h"
#include <stdint.h>

/* Install only during the preload constructor, before CarPlay worker threads
 * can execute libAirPlay. Returns the callable original trampoline or NULL. */
void *dcc_runtime_hook_install(void *target, void *replacement,
                               const uint32_t expected[2],
                               enum dcc_arm32_relocation relocation,
                               uint32_t literal);

struct dcc_p0867d_airplay_target {
    uintptr_t base;
    void *copy_server_info;
    void *setup;
    void *start;
    void *teardown;
    void *platform_control;
    void *control;
    void *set_security_info;
    void *derive_key;
};

/* Resolves and verifies all exact P0867D addresses/prologues as one unit.
 * A partial or shifted match is rejected. */
int dcc_p0867d_airplay_resolve(struct dcc_p0867d_airplay_target *target);

#endif
