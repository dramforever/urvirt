#pragma once

#include <stdint.h>

static const uintptr_t URVIRT_BLOCK = 0x10400000;
static const uintptr_t URVIRT_BLOCK_COMMAND = 0;
static const uintptr_t URVIRT_BLOCK_BLOCK_ID = 8;
static const uintptr_t URVIRT_BLOCK_BUF = 16;

static const uintptr_t URVIRT_BLOCK_CMD_READ = 1;
static const uintptr_t URVIRT_BLOCK_CMD_WRITE = 2;

static const size_t URVIRT_BLOCK_SIZE = 512;
