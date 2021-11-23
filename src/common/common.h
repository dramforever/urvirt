#pragma once

#include <stddef.h>
#include <signal.h>

static const int CONFIG_FD = 64;
static const int RAM_FD = 65;
static const int KERNEL_FD = 66;

static const size_t RAM_START = 0x80000000;
static const size_t RAM_SIZE = 16ul << 20;
static const size_t SIGSTACK_SIZE = 4096;

static const size_t KERNEL_START = 0x80200000;

struct urvirt_config {
    void *stub_start;
    size_t stub_size;
    size_t kernel_size;
};

static const size_t CONF_SIZE = 4096;

typedef void (*stub_entrypoint_ptr)();
