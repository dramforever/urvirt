#include <signal.h>
#include <ucontext.h>
#include <syscall.h>

#include "common.h"
#include "urvirt_syscalls.h"

void handler(int sig, siginfo_t *info, void *ucontext_voidp) {
    ucontext_t *ucontext = (ucontext_t *) ucontext_voidp;
}

__attribute__((section(".text.entrypoint")))
void entrypoint(void *image_start, size_t image_size) {
    s_munmap((void *) 0, (size_t) image_start);
    s_munmap(image_start + image_size, (1ul << 38) - (size_t)(image_start + image_size));
    while(1);
    s_exit_group(-1);
}
