#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Check if a user pointer range is safe to access from kernel
// For now: reject null, require canonical user addresses (< 0x0000800000000000)
bool user_ptr_valid(const void *ptr, size_t len);

// Copy from user space to kernel buffer (returns false on invalid ptr)
bool copy_from_user(void *kernel_dst, const void *user_src, size_t len);

// Copy from kernel to user space
bool copy_to_user(void *user_dst, const void *kernel_src, size_t len);
