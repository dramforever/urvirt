#pragma once

#include <stdint.h>

// Stolen from newlib libgloss/riscv/internal_syscall.h
// https://sourceware.org/git/?p=newlib-cygwin.git;a=blob;f=libgloss/riscv/internal_syscall.h

inline uintptr_t internal_syscall(uintptr_t n, int argc, uintptr_t _a0, uintptr_t _a1, uintptr_t _a2, uintptr_t _a3, uintptr_t _a4, uintptr_t _a5)
{
  register uintptr_t syscall_id asm("a7") = n;

  register uintptr_t a0 asm("a0") = _a0;
  if (argc < 2) {
      asm volatile ("ecall" : "+r"(a0) : "r"(syscall_id));
      return a0;
  }
  register uintptr_t a1 asm("a1") = _a1;
  if (argc == 2) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(syscall_id));
      return a0;
  }
  register uintptr_t a2 asm("a2") = _a2;
  if (argc == 3) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(syscall_id));
      return a0;
  }
  register uintptr_t a3 asm("a3") = _a3;
  if (argc == 4) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(syscall_id));
      return a0;
  }
  register uintptr_t a4 asm("a4") = _a4;
  if (argc == 5) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(syscall_id));
      return a0;
  }
  register uintptr_t a5 asm("a5") = _a5;

  asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(syscall_id));

  return a0;
}
