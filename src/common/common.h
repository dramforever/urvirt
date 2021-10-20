#pragma once

#include <stddef.h>

typedef void (*stub_entrypoint_ptr)(void *image_start, size_t image_size);
