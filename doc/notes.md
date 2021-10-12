# 目标

在 HiFive Unmatched 的 riscv64-linux 上模拟运行无修改或尽量少修改的 uCore 和 rCore

# 思路

- 轻量级纯 userspace 模拟运行 xCore
    - 只支持在 riscv64-linux 上运行 riscv64-{u,r}core
- 实现大部分 xCore 虚存管理功能
- 在用户态实现 S/U 特权级的维护
- 在 U-mode，trap 并分发 ecall
- 在 S-mode，模拟 SBI 系统调用

# 技术栈

- 指令模拟
    - Trap: 使用 `seccomp` 通过令所有 `ecall` 发生 `SIGSYS` 来捕获 `ecall` 指令，使用 signal handler `SIGILL` 捕获特权指令。（⚠️ `ebreak` 指令和 `SIGTRAP` 调试器使用，考虑不捕获或可选捕获）
    - 模拟：通过传入的 `ucontext_t` 读取需要被模拟的指令执行时的 `npc` 和各种寄存器的值，读取寄存器，软件模拟对应的功能
- 虚存模拟
    - Trap CSR 访问和 `sfence.vma`，参考前述指令模拟
    - 使用 `mmap` 指定地址的方式来翻译页表维护虚拟内存
    - page fault → `SIGSEGV`
    - 特殊选一段地址用来存放 ummmu 的代码和数据（⚠️ 并不保证安全性？）
    - `mmap(2)` 的 `MAP_FIXED_NOREPLACE` flag
- 驱动
    - Trap “物理”地址的访问，用模拟指令的方法模拟 MMIO
