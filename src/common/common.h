#pragma once

#include <stddef.h>
#include <signal.h>

const int CONFIG_FD = 64;
const int RAM_FD = 65;
const int KERNEL_FD = 66;

const size_t RAM_START = 0x80000000;
const size_t RAM_SIZE = 16ul << 20;
const size_t SIGSTACK_SIZE = 4096 * 4;

const size_t KERNEL_START = 0x80200000;

struct urvirt_config {
    void *stub_start;
    size_t stub_size;
    size_t max_va;
    size_t kernel_size;
};

const size_t CONF_SIZE = (sizeof(struct urvirt_config) + 4095) & (~ 4095);

typedef void (*stub_entrypoint_ptr)();
