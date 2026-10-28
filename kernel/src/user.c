#include "user.h"

// x86-64 user space is typically below 0x00007FFFFFFFFFFF
#define USER_SPACE_TOP  0x0000800000000000ULL

bool user_ptr_valid(const void *ptr, size_t len) {
    uint64_t addr = (uint64_t)ptr;
    if (addr == 0) return false;
    if (len > USER_SPACE_TOP) return false;
    if (addr >= USER_SPACE_TOP) return false;
    if (addr + len > USER_SPACE_TOP) return false;
    // Also reject kernel higher-half addresses
    if (addr >= 0xFFFF800000000000ULL) return false;
    return true;
}

bool copy_from_user(void *kernel_dst, const void *user_src, size_t len) {
    if (!user_ptr_valid(user_src, len)) return false;
    const uint8_t *s = user_src;
    uint8_t *d = kernel_dst;
    for (size_t i = 0; i < len; i++) d[i] = s[i];
    return true;
}

bool copy_to_user(void *user_dst, const void *kernel_src, size_t len) {
    if (!user_ptr_valid(user_dst, len)) return false;
    const uint8_t *s = kernel_src;
    uint8_t *d = user_dst;
    for (size_t i = 0; i < len; i++) d[i] = s[i];
    return true;
}
