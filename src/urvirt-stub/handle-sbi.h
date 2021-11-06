#pragma once

#include <stdint.h>
#include "riscv-priv.h"

static const uintptr_t SBI_SUCCESS = 0;
static const uintptr_t SBI_ERR_FAILED = -1;
static const uintptr_t SBI_ERR_NOT_SUPPORTED = -2;
static const uintptr_t SBI_ERR_INVALID_PARAM = -3;
static const uintptr_t SBI_ERR_DENIED = -4;
static const uintptr_t SBI_ERR_INVALID_ADDRESS = -5;
static const uintptr_t SBI_ERR_ALREADY_AVAILABLE = -6;
static const uintptr_t SBI_ERR_ALREADY_STARTED = -7;
static const uintptr_t SBI_ERR_ALREADY_STOPPED = -8;

struct sbiret {
    uintptr_t error;
    uintptr_t value;
};

static const uintptr_t SBI_SET_TIMER = 0;
static const uintptr_t SBI_CONSOLE_PUTCHAR = 1;
static const uintptr_t SBI_CONSOLE_GETCHAR = 2;
static const uintptr_t SBI_SHUTDOWN = 8;

struct sbiret handle_sbi_call(
    struct priv_state *priv,
    uintptr_t which, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2);
