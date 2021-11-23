# How does URVirt work?

## Basic design

The RISC-V instruction set is supposed to be classically virtualizable. This
would mean that a program running in an unprivileged mode would have no way
knowing that it is unprivileged. The privileged instructions would all trap into
a supervisor, which, if so desired, would emulate all such instructions
transparently so that the user program can imagine that it is running as a
supervisor, and would run as a virtual operating system.

This supervisor-emulating supervisor is traditonally called a *hypervisor* and
usually run as in supervisor on the CPU. Is there a way, however, to run the
hypervisor as a *user-mode* program?

Suppose we want to run the user program under Linux. Theoretically if we just
catch the appropriate signals, we can emulate most of the stuff needed for an
operating system:

- Use a Seccomp filter to catch `ecall` instructions
- Handle `SIGILL` to catch and emulate privileged instructions
- Handle `SIGSEGV` to catch and emulate virtual memory access, translating page
  tables to `mmap(2)` calls
- Use Linux timers to generate timer 'interrupts'

In URVirt, these are just regular `sigaction(2)` signal handlers. Using another
process and `ptrace(2)` to handle these would be another way. However it is
considerably more complex, and it's rather weird to run system calls in the
ptracee from the ptracer. Maybe it would have been a good choice, but it ended
up not being the one picked.

## Code organization

URVirt has two main parts: `urvirt-loader` and `urvirt-stub`. There's also the
kernel image to be loaded.

### Loader

The loader is a normal Linux program that basically just loads the stub, sets up
the 'RAM', copies the kernel into it, sets up some basic configuration for the
stub, and jumps to the stub entrypoint.

We only support flat binary kernels at this point, but it is theoretically
possible to do some ELF handling at this point.

### Stub

The stub is very special bit of program. Essentially, all it has available is:

- Readable, executable (but not writable) memory for code and read-only data
  (That is, `.text` and `.rodata`)
- A few pages of space as stack

It should be a flat binary file with no linking needed, and. In theory, the
loader could also do some ELF handling to make dynamic linking work, but that's
not really worth doing.

The loader just maps it to some random address, no big deal.

## The magical file descriptors

### `CONFIG_FD`

This is where the. Also includes all of the privileged state, which is filled in
by the stub.

This bit is mapped when the stub is doing its work, and unmapped when the
virtualized kernel is running.

### `RAM_FD`

This is all of 'RAM', created with `memfd_create(2)`. At initialization, we copy
the kernel here and jump to it. Later on with virtual memory we map pages from
`RAM_FD` at various virtual addresses as needed.

### `KERNEL_FD`

This is just the file descriptor we opened the kernel at.

## Initialization

### Loading the 'stub' and kernel image

### Setting up the memory map

### Seccomp filter

We also need to somehow catch all `ecall` instructions. Not exactly all, because
our stub also needs to make syscalls! This is done fairly easily (albeit a bit
dirtily) with a Seccomp BPF filter.

We generate filter the based on the address of the stub, in
`urvirt-stub/seccomp-bpf.{c,h}`. Then just set it up with `seccomp(2)` and we're
on our merry way.

### Signal handlers

### Jumping to the kernel

## The signal handler

### `ucontext_t`

### `SIGILL` for privileged instructions

### `SIGSYS` for `ecall`

### `SIGALRM` for timer

### `SIGSEGV` for virtual memory

## CSR Implementations

Please just check the proper specs, there's not much I can add here other than
rabbit holes I fell into.

### `sscratch`, `scause`, `stval`

These CSRs don't do much. They just hold a value that the supervisor might need,
and the emulator also fills in `scause` and `stval` at times.

### `sepc`

`sepc` also doesn't really do much other than holding a value, but as mentioned
it's a little tricky to get right if you want to fill it in from signal handlers.

### `stvec`

Address to jump to. Almost like the inverse of `sepc`, w.r.t. its use in `sret`.
Has a WARL mode field but otherwise easy enough to handle.

URVirt implements direct and vectored `stvec`, but honestly with just timer
interrupts available there's no real need for vectored mode.

### `sstatus`

So many bits in `sstatus`. The main bits we need to talk about are...

#### `SD`, `FS` and `XS`

There's no way we can track these from userspace. (Or can we? `ustatus` when?),
but technically we're allowed to make them dirty whenever we like.

Can't really turn the FPU off in user mode can we? Not going to worry about that
here.

#### `sstatus.SPP`

This bit marks whether the trap came from U-mode or S-mode. This is so that
`sret` knows which mode to return to.

Just, track it in traps and use it in `sret` I suppose.

#### `sstatus.SPIE` and `sstatus.SIE`

This was a rabbit hole for me. For one, I really thought setting `sstatus.SIE`
to `0` would disable, but somehow QEMU tells me otherwise and they are still
delivered. After like, who knows how long I realized that the almighty
`riscv-privileged.pdf` says:

> Interrupts for lower-privilege modes, `w < x`, are always globally disabled
> regardless of the setting of any global `wIE` bit for the lower-privilege
> mode. Interrupts for higher-privilege modes, `y > x`, are always globally
> enabled regardless of the setting of the global `yIE` bit for the
> higher-privilege mode.

Yeah... in U-mode, supervisor interrupts are always globally enabled. So much
fun.

Otherwise it's just simple enough moving data around. On traps, copy `SIE` to
`SPIE` and set `SIE` to `0`. On `sret`, copy `SPIE` to `SIE` and set `SPIE` to
`1`.

### `sip` and `sie`

This one was also a major pain. I, for some reason, thought that if an interrupt
is disabled when it occurs, it's just lost. However:

> An interrupt *i* will trap to S-mode if both of the following are true: (a)
> either the current privilege mode is S and the `SIE` bit in the `sstatus` register
> is set, or the current privilege mode has less privilege than S-mode; and (b)
> bit *i* is set in both sip and sie.

Yeah... pending interrupts are taken if you end up enabling them.

The two obvious place to generate traps for interrupts, which by the way only
consists of timer interrupts, are:

- The signal handler, when `SIGALRM` for the timer elapses
- After emulating the instruction that enables interrupts

In any case, I just add a check at the end of the signal handler to take
interrupt traps if necessary, and just set `sip.STIP` on `SIGALRM`.

### `satp`

## SBI functions

### Shutdown

I just implemented like three legacy functions. Don't judge me.

### Console functions

### Timer

## `satp` and page tables

## More gory details

### Wait, where is the signal handler mapped?

- Code
- Stack
- Switching stack mid-function?

### Linux system calls

### Instruction decoding

### When should an interrupt occur?

### Mapping `SIGSEGV` to specific page faults
