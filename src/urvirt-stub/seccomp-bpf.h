#pragma once

#include <stddef.h>
#include <stdint.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

// Generate a Seccomp BPF filter that checks IP, and blocks all syscalls from
// address outside the half-open range [start_addr, end_addr)
size_t gen_addr_filter(size_t start_addr, size_t end_addr, struct sock_filter *out);
