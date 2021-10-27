#pragma once

#include <unistd.h>
#include <syscall.h>
#include <sys/mman.h>

#include "internal_syscall.h"

inline void s_exit_group(int status) {
    internal_syscall(SYS_exit_group, 1, (uintptr_t) status, /* ... */ 0, 0, 0, 0, 0);
}

inline int s_munmap(void *addr, size_t length) {
    return (int) internal_syscall(SYS_munmap, 2, (uintptr_t) addr, (uintptr_t) length, /* ... */ 0, 0, 0, 0);
}

inline void *s_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset) {
    return (void *) internal_syscall(SYS_mmap, 6, (uintptr_t) addr, (uintptr_t) length, (uintptr_t) prot, (uintptr_t) flags, (uintptr_t) fd, (uintptr_t) pgoffset);
}

inline int s_sigaltstack(const stack_t * ss, stack_t * oss) {
    return (int) internal_syscall(SYS_sigaltstack, 2, (uintptr_t) ss, (uintptr_t) oss, /* ... */ 0, 0, 0, 0);
}

inline int s_rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {
    return (int) internal_syscall(SYS_rt_sigaction, 4, (uintptr_t) sig, (uintptr_t) act, (uintptr_t) oact, (uintptr_t) 8, /* ... */ 0, 0);
}

inline ssize_t s_write(int fd, const void *buf, size_t count) {
    return (ssize_t) internal_syscall(SYS_write, 3, (uintptr_t) fd, (uintptr_t) buf, (uintptr_t) count, /* ... */ 0, 0, 0);
}
