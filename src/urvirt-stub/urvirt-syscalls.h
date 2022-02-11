#pragma once

#include <unistd.h>
#include <syscall.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>

#include "internal-syscall.h"

// See Linux source code, include/uapi/asm-generic/signal.h
struct kernel_sigaction {
	union {
		void (*_sa_handler)(int);
		void (*_sa_sigaction)(int, siginfo_t *, void *);
	} _sa_handler;
	uintptr_t sa_flags;
	sigset_t sa_mask;
};

// See Linux source code, include/uapi/linux/prctl.h
#define PR_SET_SYSCALL_USER_DISPATCH	59
# define PR_SYS_DISPATCH_OFF		0
# define PR_SYS_DISPATCH_ON		1
/* The control values for the user space selector when dispatch is enabled */
# define SYSCALL_DISPATCH_FILTER_ALLOW	0
# define SYSCALL_DISPATCH_FILTER_BLOCK	1

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

inline int s_rt_sigaction(int sig, const struct kernel_sigaction *act, struct kernel_sigaction *oact) {
    return (int) internal_syscall(SYS_rt_sigaction, 4, (uintptr_t) sig, (uintptr_t) act, (uintptr_t) oact, (uintptr_t) 8, /* ... */ 0, 0);
}

inline ssize_t s_write(int fd, const void *buf, size_t count) {
    return (ssize_t) internal_syscall(SYS_write, 3, (uintptr_t) fd, (uintptr_t) buf, (uintptr_t) count, /* ... */ 0, 0, 0);
}

inline int s_seccomp(unsigned int operation, unsigned int flags, void *args) {
    return (int) internal_syscall(SYS_seccomp, 3, (uintptr_t) operation, (uintptr_t) flags, (uintptr_t) args, /* ... */ 0, 0, 0);
}

inline void s_riscv_flush_icache(uintptr_t start, uintptr_t end, uintptr_t flags) {
    internal_syscall(SYS_riscv_flush_icache, 3, start, end, flags, /* ... */ 0, 0, 0);
}

inline int s_timer_create(clockid_t clockid, struct sigevent *restrict sevp, timer_t *restrict timerid) {
    internal_syscall(SYS_timer_create, 3, (uintptr_t) clockid, (uintptr_t) sevp, (uintptr_t) timerid, /* ... */ 0, 0, 0);
}

inline int s_timer_settime(timer_t timerid, int flags,
                         const struct itimerspec *restrict new_value,
                         struct itimerspec *restrict old_value) {
    internal_syscall(SYS_timer_settime, 4, (uintptr_t) timerid, (uintptr_t) flags, (uintptr_t) new_value, (uintptr_t) old_value, /* ... */ 0, 0);
}

inline ssize_t s_read(int fd, void *buf, size_t count) {
    return internal_syscall(SYS_read, 3, (uintptr_t) fd, (uintptr_t) buf, (uintptr_t) count, /* ... */ 0, 0, 0);
}

inline int s_fcntl(int fd, int cmd, int arg) {
    return internal_syscall(SYS_fcntl, 3, (uintptr_t) fd, (uintptr_t) cmd, (uintptr_t) arg, /* ... */ 0, 0, 0);
}

inline ssize_t s_pread64(int fd, void *buf, size_t count, off_t offset) {
    return internal_syscall(SYS_pread64, 4, (uintptr_t) fd, (uintptr_t) buf, (uintptr_t) count, (uintptr_t) offset, /* ... */ 0, 0);
}

inline ssize_t s_pwrite64(int fd, const void *buf, size_t count, off_t offset) {
    return internal_syscall(SYS_pwrite64, 4, (uintptr_t) fd, (uintptr_t) buf, (uintptr_t) count, (uintptr_t) offset, /* ... */ 0, 0);
}

inline ssize_t s_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
    return internal_syscall(SYS_prctl, 5, (uintptr_t) option, (uintptr_t) arg2, (uintptr_t) arg3, (uintptr_t) arg4, (uintptr_t) arg5, /* ... */ 0);
}

#define write_log(str) do { s_write(2, "[urvirt] " str "\n", sizeof("[urvirt] " str "\n") - 1); } while(0)
