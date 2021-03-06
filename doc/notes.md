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

# 实现计划

- 指令模拟的基本框架（能捕获各种指令和异常）
    - 目前实现：
        - 加载 stub 到地址空间，`munmap(2)` 所有其它的地址，这样完全接管。
    - 待实现：
        - Signal handler
        - 指令解码
- seccomp（捕获 ecall）
- SBI 函数实现（串口，timer）
- 物理内存的实现
    - `memfd`
- RISC-V 指令解码
- 虚拟内存功能实现
    - CSR: `satp`
    - `sfence.vma`
    - 页表的解析
    - `SIGSEGV` 捕获
- 特权级别实现
- Block device 驱动实现

# 参考资料和笔记

## RISC-V 下 Linux 的虚拟地址空间

<https://www.kernel.org/doc/html/latest/riscv/vm-layout.html>

Sv39 虚拟地址空间只有 &pm;256GB 部分可以使用。Linux 中正的地址给用户态，负的地址给内核态，`mmap(2)` 和 `munmap(2)` 不能碰内核态的空间。

xCore 会不会使用负的地址？

## RVirt

<https://github.com/mit-pdos/RVirt>

M 态运行的 hypervisor，可能能参考很大一部分。

## `seccomp(2)` 相关

### 教程

这是一个教你手写 BPF 的教程。

<https://outflux.net/teach-seccomp>

### 踩坑

- `seccomp(2)` 的 `SECCOMP_SET_MODE_FILTER` 用的好像是 BPF 而不是 eBPF
- BPF 没有 64 位数的操作指令
- QEMU user 并不支持 seccomp 的 BPF 模式

## Signal handler 相关踩坑

- `SECCOMP_RET_TRAP` 产生的 `SIGSYS`，传给 handler 的 `ucontext_t` 里的寄存器 `a7` 好像并不是系统调用编号，需要在 `siginfo_t` 的 `si_syscall` field 里找到。
- `SECCOMP_RET_TRAP` 产生的 `SIGSYS`，`pc` 已经是 `ecall`/`c.ecall` 指令后面的指令
- 把 vdso 给 `munmap(2)` 之后 signal handler 返回的时候会 `SIGSEGV`，因为内核执行 signal handler 的时候将 `ra` 设置为 vdso 里的 `__vdso_rt_sigreturn` 函数的地址，也就是恢复栈指针之后要调用系统调用 `rt_sigreturn` 返回到信号发生的地方，所以我们可以复刻一份这个函数然后绕过 vdso 里面的那份（参见 <https://elixir.bootlin.com/linux/latest/source/arch/riscv/kernel/vdso/rt_sigreturn.S#L10>）

## C 语言相关踩坑

- （存疑）把一个指针 cast 成 `long` 之后 GCC 的 escape analysis 会觉得指针没用了，但是 cast 成 `uintptr_t` 就没事，不太懂。

# 指令模拟

## 指令译码

- RISC-V 特权指令实在太规整了，几乎不需要译码

```text
                    |          12       |   5   |   3    |   5   |    7    |
                    |     7     |   5   |
                    |-----------|-------|-------|--------|-------|---------|
CSR{RW,RS,RC}       | csr12             | rs1   | funct3 | rd    | 1110011 |
CSR{RW,RS,RC}I      | csr12             | uimm5 | funct3 | rd    | 1110011 |

SFENCE.VMA          | 0001001   | rs2   | rs1   | 000    | 00000 | 1110011 |
SRET                | 0001000   | 00010 | 00000 | 000    | 00000 | 1110011 |
```


## CSR 指令的模拟

- OpenSBI 中有一些 CSR 的模拟，实现位于 [`lib/sbi/sbi_illegal_insn.c`][sbi_illegal_insn]，可以参考

[sbi_illegal_insn]: https://github.com/riscv-software-src/opensbi/blob/master/lib/sbi/sbi_illegal_insn.c

# 中断

## 中断发生的情况

> An interrupt *i* will be taken if bit *i* is set in both `mip` and `mie`, and if interrupts are globally enabled.

也就是说，例如，如果比如某个时候 `sstatus.SIE = 0`，`sip.STIP = 1`，然后设置 `sstatus.SIE = 1`，此时会发生一个 Supervisor timer interrupt。

```text
# 1. In user mode, interrupts are enabled
ecall

# 2. Traps to supervisor mode, interrupts are disabled
...

# 3. Timer elapses, interrupt not taken
...

sret
# 4. Returns to user mode, interrupts are enabled
# 5. Timer interrupt pending, interrupt taken
```

## `sip.STIP` 的维护

SBI 的 `set_timer` 维护保证 `sip.STIP = (timecmp <= time)`。

## 中断和特权级的关系

Interrupts for lower-privilege modes, `w < x`, are always globally disabled regardless of the setting of any global `wIE` bit for the lower-privilege mode. Interrupts for higher-privilege modes, `y > x`, are always globally enabled regardless of the setting of the global `yIE` bit for the higher-privilege mode.

# `printf`

Thanks <https://github.com/mpaland/printf> for the wonderful `malloc`-less `printf` implementation!
