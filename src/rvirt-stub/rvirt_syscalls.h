#pragma once

#include <syscall.h>

#include "internal_syscall.h"

inline void s_exit_group(int status) {
    internal_syscall(SYS_exit_group, 1, (long) status, /* ... */ 0, 0, 0, 0, 0);
}

inline int s_munmap(void *addr, size_t length) {
    return (int) internal_syscall(SYS_munmap, 2, (long) addr, (long) length, /* ... */ 0, 0, 0, 0);
}

