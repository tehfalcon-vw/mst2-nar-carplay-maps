#include "dcc_qnx.h"
#include "dcc_runtime_hook.h"

#define PAGE_SIZE 0x1000u

void *dcc_runtime_hook_install(void *target_pointer, void *replacement_pointer,
                               const uint32_t expected[2],
                               enum dcc_arm32_relocation relocation,
                               uint32_t literal)
{
    struct dcc_arm32_hook_plan plan;
    uint32_t *target = (uint32_t *)target_pointer;
    uint32_t *trampoline;
    uintptr_t page;
    size_t trampoline_bytes;
    if (!target || !replacement_pointer || !expected ||
        target[0] != expected[0] || target[1] != expected[1]) return NULL;
    trampoline = (uint32_t *)mmap(NULL, PAGE_SIZE,
                                  PROT_READ | PROT_WRITE | PROT_EXEC,
                                  MAP_PRIVATE | MAP_ANON, NOFD, 0);
    if (trampoline == MAP_FAILED) return NULL;
    if (dcc_arm32_plan_hook((uint32_t)(uintptr_t)target,
                            (uint32_t)(uintptr_t)replacement_pointer,
                            expected, relocation, literal, &plan)) {
        munmap(trampoline, PAGE_SIZE);
        return NULL;
    }
    trampoline_bytes = plan.trampoline_words * sizeof(uint32_t);
    memcpy(trampoline, plan.trampoline, trampoline_bytes);
    if (msync(trampoline, trampoline_bytes,
              MS_SYNC | MS_INVALIDATE_ICACHE | MS_CACHE_ONLY)) {
        munmap(trampoline, PAGE_SIZE);
        return NULL;
    }
    page = (uintptr_t)target & ~(uintptr_t)(PAGE_SIZE - 1u);
    if (mprotect((void *)page, PAGE_SIZE,
                 PROT_READ | PROT_WRITE | PROT_EXEC)) {
        munmap(trampoline, PAGE_SIZE);
        return NULL;
    }
    /* Constructors run before receiver threads. This two-word patch must not
     * be used later against a live multi-threaded CarPlay process. */
    memcpy(target, plan.patch, sizeof(plan.patch));
    if (msync(target, sizeof(plan.patch),
              MS_SYNC | MS_INVALIDATE_ICACHE | MS_CACHE_ONLY)) {
        memcpy(target, expected, sizeof(plan.patch));
        (void)msync(target, sizeof(plan.patch),
                    MS_SYNC | MS_INVALIDATE_ICACHE | MS_CACHE_ONLY);
        (void)mprotect((void *)page, PAGE_SIZE, PROT_READ | PROT_EXEC);
        munmap(trampoline, PAGE_SIZE);
        return NULL;
    }
    if (mprotect((void *)page, PAGE_SIZE, PROT_READ | PROT_EXEC)) {
        memcpy(target, expected, sizeof(plan.patch));
        (void)msync(target, sizeof(plan.patch),
                    MS_SYNC | MS_INVALIDATE_ICACHE | MS_CACHE_ONLY);
        munmap(trampoline, PAGE_SIZE);
        return NULL;
    }
    return trampoline;
}
