#include "dcc_qnx.h"
#include "dcc_runtime_hook.h"

struct binding {
    const char *name;
    uint32_t offset;
    uint32_t word0;
    uint32_t word1;
};

static int bind(uintptr_t base, const struct binding *binding, void **result)
{
    void *symbol = dlsym(RTLD_DEFAULT, binding->name);
    const uint32_t *words = (const uint32_t *)(base + binding->offset);
    if (!symbol || (uintptr_t)symbol != base + binding->offset ||
        words[0] != binding->word0 || words[1] != binding->word1) return -1;
    *result = symbol;
    return 0;
}

int dcc_p0867d_airplay_resolve(struct dcc_p0867d_airplay_target *target)
{
    static const struct binding bindings[] = {
        {"AirPlayCopyServerInfo", 0x0000ffacu, 0xe92d4ff0u, 0xe24dde4du},
        {"AirPlayReceiverSessionSetup", 0x000165e0u, 0xe92d4ff0u, 0xe59f4f60u},
        {"AirPlayReceiverSessionStart", 0x00014124u, 0xe92d4ff0u, 0xe24dd0c4u},
        {"AirPlayReceiverSessionTearDown", 0x00015cb8u, 0xe92d4ff0u, 0xe24dd04cu},
        {"AirPlayReceiverSessionPlatformControl", 0x0000c7c8u, 0xe92d4ff0u, 0xed2d8b04u},
        {"AirPlayReceiverSessionControl", 0x00018b1cu, 0xe92d4ff0u, 0xe1a07002u},
        {"AirPlayReceiverSessionSetSecurityInfo", 0x00018950u, 0xe92d45f0u, 0xe1a04000u},
        {"AirPlayReceiverSessionScreen_SetSecurityInfo", 0x00019e9cu, 0xe92d40f0u, 0xe5d031e4u}
    };
    void *copy;
    void **outputs;
    size_t i;
    if (!target) return -1;
    memset(target, 0, sizeof(*target));
    copy = dlsym(RTLD_DEFAULT, bindings[0].name);
    if (!copy || (uintptr_t)copy < bindings[0].offset) return -1;
    target->base = (uintptr_t)copy - bindings[0].offset;
    outputs = &target->copy_server_info;
    for (i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bind(target->base, &bindings[i], &outputs[i])) {
            memset(target, 0, sizeof(*target));
            return -1;
        }
    target->derive_key = dlsym(RTLD_DEFAULT, "AirPlay_DeriveAESKeySHA512ForScreen");
    if (!target->derive_key ||
        (uintptr_t)target->derive_key != target->base + 0x0001c344u) {
        memset(target, 0, sizeof(*target));
        return -1;
    }
    return 0;
}
