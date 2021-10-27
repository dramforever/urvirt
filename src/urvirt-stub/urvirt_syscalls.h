#pragma once

#include <syscall.h>
#include <sys/mman.h>

#include "internal_syscall.h"

inline void s_exit_group(int status) {
    internal_syscall(SYS_exit_group, 1, (long) status, /* ... */ 0, 0, 0, 0, 0);
}

inline int s_munmap(void *addr, size_t length) {
    return (int) internal_syscall(SYS_munmap, 2, (long) addr, (long) length, /* ... */ 0, 0, 0, 0);
}

inline void *s_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset) {
    return (void *) internal_syscall(SYS_mmap, 6, (long) addr, (long) length, (long) prot, (long) flags, (long) fd, (long) pgoffset);
}

inline int s_sigaltstack(const stack_t * ss, stack_t * oss) {
    return (int) internal_syscall(SYS_sigaltstack, 2, (long) ss, (long) oss, /* ... */ 0, 0, 0, 0);
}

inline int s_rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {
    return (int) internal_syscall(SYS_rt_sigaction, 4, (long) sig, (long) act, (long) oact, (long) 8, /* ... */ 0, 0);
}
