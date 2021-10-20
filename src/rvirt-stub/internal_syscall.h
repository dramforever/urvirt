#pragma once

// Stolen from newlib libgloss/riscv/internal_syscall.h
// https://sourceware.org/git/?p=newlib-cygwin.git;a=blob;f=libgloss/riscv/internal_syscall.h

inline long internal_syscall(long n, int argc, long _a0, long _a1, long _a2, long _a3, long _a4, long _a5)
{
  register long syscall_id asm("a7") = n;

  register long a0 asm("a0") = _a0;
  if (argc < 2) {
      asm volatile ("ecall" : "+r"(a0) : "r"(syscall_id));
      return a0;
  }
  register long a1 asm("a1") = _a1;
  if (argc == 2) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(syscall_id));
      return a0;
  }
  register long a2 asm("a2") = _a2;
  if (argc == 3) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(syscall_id));
      return a0;
  }
  register long a3 asm("a3") = _a3;
  if (argc == 4) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(syscall_id));
      return a0;
  }
  register long a4 asm("a4") = _a4;
  if (argc == 5) {
      asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(syscall_id));
      return a0;
  }
  register long a5 asm("a5") = _a5;

  asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(syscall_id));

  return a0;
}
