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
  tables to `mmap` calls
- Use Linux timers to generate timer 'interrupts'

In URVirt, these are just regular `sigaction` signal handlers. Using another
process and `ptrace` to handle these would be another way. However it is
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

This contains information the loader tells the stub, and after that, includes
all of the privileged state, which is filled in by the stub.

This bit is mapped when the stub is doing its work, and unmapped when the
virtualized kernel is running.

### `RAM_FD`

This is all of 'RAM', created with `memfd_create`. At initialization, we copy
the kernel here and jump to it. Later on with virtual memory we map pages from
`RAM_FD` at various virtual addresses as needed.

### `KERNEL_FD`

This is just the file descriptor we opened the kernel at.

### `BLOCK_FD`

This is a flat image for the emulated block device.

## Initialization

### Loading all the files

At start up, we read the command line and open the files at the desired file
descriptors using `open`, `dup2`, `close`.

The stub is mapped into memory, configuration is written to `CONFIG_FD`, and we
jump to the stub.

### Setting up the memory map

The stub unmaps everything that is not needed. So the original libc, heap, stack
is unmapped, while the stub itself is kept. The address of the stub is passed
from `CONFIG_FD`, which is kept mapped only for as long as needed.

After that, we make ourselves a stack area that is both a workspace for the
initialization process, and also signal handler stack for the signal handler.

Since the `sp` at this point is no longer valid, we modify it using inline asm
and jump to another function to finish the rest of the job.

### Signal handlers

Firstly, we need to use `sigaltstack` to set up a stack pointer to use when a
signal occurs. We cannot do without a stack and just map one ourselves, since
the kernel actually pushes a 'signal frame' onto the user stack, so that it
saves user state for restoring on return. This, by the way, is also useful for
us later on when we do need to modify user state.

Then we set our handler function to handle the signals: `SIGILL`, `SIGSYS`,
`SIGALRM`, `SIGSEGV`.

We also construct a signal mask for the signal handlers so `SIGALRM` does not
occur when a signal handler is running, just to simplify things.

At this time, we create a timer with `timer_create` for use later on.

### Starting the kernel

Firstly, the privileged state stored in `CONFIG_FD` is initialized, including
the timer ID we got earlier.

After that, we map `RAM_FD` to the desired RAM start address. We can pick our
own address by specifying it as the `addr` argument and specifying
`MAP_FIXED_NOREPLACE` as a flag.

Now we just the kernel from `KERNEL_FD` to RAM, and jump to the kernel and the
system is started.

## The signal handler

### `ucontext_t`

When creating the signal handler, if we specify the flag `SA_SIGINFO`, the
signal handler has rougly this structure:

```c
void handler(int sig, siginfo_t *info, ucontext_t *ucontext);
```

Here, `sig` is the signal number, `info` contains various information about
'what happened', but most importantly, `ucontext` contains the entire userspace
context at the instruction the signal occurred. This means the `pc`, all
user-mode registers are available for us to access. Moreover, `ucontext` is
writable, meaning that we can modify it at will.

We dispatch to multiple cases based on the value of `sig`.

### `SIGILL` for privileged instructions

The general idea is to decode the instruction, see what it should do, and:

- If we can handle it, do it and advance `pc`
- If we can't handle it, raise the appropriate exception tp the guest.

For example, the privileged CSRs are not accessible in user space, so whenever
an access occurs, it will give `SIGILL`. We save the current privilege mode and
also values of emulated CSRs separately, so if the access happens in emulated
S-mode, the specific read/write/clear/set operation is carried out, along with
any effects emulated. If in emulated U-mode, raise an illegal instruction
exception to the guest.

### `SIGSYS` for `ecall`

`ecall` is similar to the case of `SIGILL`, as we emulate all `ecall`
executions. If in emulated U-mode, we trap to emulated S-mode, otherwise we
emulate an SBI call.

### `SIGALRM` for timer

This one just sets `sip.STIP`. At the end of the signal handler is a check to
whether we should trap to an interrupt, and if so, we do that.

### `SIGSEGV` for virtual memory

Illegal memory accesses. There are three possibilities:

- The access really is illegal
- The access is to an emulated MMIO device
- The access is to a page in emulated RAM, but has not been mapped from the
  emulate page table

For the first case, we raise an emulated exception as usual. The rest two are
explained in detail in relevant sections.

## CSR Implementations

Please just check the proper specs, there's not much I can add here other than
rabbit holes I fell into.

### `sscratch`, `scause`, `stval`

These CSRs don't do much. They just hold a value that the supervisor might need,
and the emulator also fills in `scause` and `stval` at times.

### `sepc`

`sepc` also doesn't really do much other than holding a value, but as mentioned
it's a little tricky to get right if you want to fill it in from signal
handlers.

### `stvec`

Address to jump to on supervisor traps. Almost like the inverse of `sepc`,
w.r.t. its use in `sret`. Has a WARL mode field but otherwise easy enough to
handle.

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
> either the current privilege mode is S and the `SIE` bit in the `sstatus`
> register is set, or the current privilege mode has less privilege than S-mode;
> and (b) bit *i* is set in both sip and sie.

Yeah... pending interrupts are taken if you end up enabling them.

The two obvious place to generate traps for interrupts, which by the way only
consists of timer interrupts, are:

- The signal handler, when `SIGALRM` for the timer elapses
- After emulating the instruction that enables interrupts

In any case, I just add a check at the end of the signal handler to take
interrupt traps if necessary, and just manage `sip.STIP` with timer stuff. See
below or more.

### `satp`

I only added support for Sv39. On startup, bare mode is used, but after the
first time `satp` is written to we unmap the physical address pages and, I just
never really bothered with the case where paging is disabled.

## SBI functions

I just implemented like three legacy functions. Don't judge me.

### Shutdown

This just corresponds to a good-old `exit`. Actually the modern syscall for
exiting an entire process is `exit_group` but that's easy enough.

### Console functions

These just correspond to `stdin` and `stdout`.

For `stdin`, I use `fcntl` to set it to non blocking so that I can properly
return `0` for `sbi_console_getchar` when there's nothing available to read.

### Timer

```c
void sbi_set_timer(uint64_t stime_value);
```

The SBI keeps track of a next-timer-elapse value. Then `sip.STIP` is taken some
time after `time >= stime_value`.

I just use a Linux `timer_create` timer to make it raise `SIGALRM` on elapse,
and set `sip.STIP` when I see that. For `sbi_set_timer` I would make sure to
reset `sip.STIP = (time >= stime_value)`.

However we can't trap reads to `time`, so, I don't really know how fast it ticks
and how to translate that to real-world time units, so to make a minimal viable
version we can use a random scale and take advantage of the fact that timer
interrupts only need to eventually trap. If we know the `time` CSR frequency,
like in the device tree `timebase-frequency`, we can use that value directly by
modifying the source code.

## `satp` and page tables

As soon as paging is enabled through a write to `satp`, the initial identity
mapping of RAM to relevant addresses is disabled using `munmap` on anything that
is not the stub.

After that, any access triggers a `SIGSEGV`. We read the desired address from
the `siginfo_t` structure passed into our signal handler.

We implement the Sv39 translation algorithm in software by looking at the RAM
contents. The details are the same as for any Sv39 implementation, and in the
end we have the leaf PTE of the desired emulated virtual address.

We now have the emulated physical address of the access. If the access should be
permitted according to the PTE:

- If the address is in RAM, map `RAM_FD` to that virtual address, and retry the
  access. Now it should succeed.
- If the address is some MMIO, handle as described in the next section.
- Otherwise it's an access fault

If the access is forbidden by the PTE, then an exception is raised.

Access to other pages are handled in a similar way.

On every further execution of `sfence.vma`, switching between S and U modes, and
writing `satp`, we unmap everything again to start afresh. Effectively, we're
using the Linux memory management system calls as a TLB.

## Emulated block devices

If an access is to an emulated physical address is targeted to the block device
MMIO registers, then any access gives a `SIGSEGV`, and we emulate the access.

| Address | Description |
|---|---|
| `0x10400000` | URVirt-block command, write only |
| `0x10400008` | URVirt-block block id, write only |
| `0x10400010` | URVirt-block buffer virtual address, write only |

For any access, put the desired block id in the block id register, (blocks are
512 bytes in length), the buffer virtual address in the virtual address
register, and put the command. The access is blocking on writing to the command registers.

The commands are:

| Number | Command |
|---|---|
| `1` | Read |
| `0` | Write |

## More gory details

### Wait, where is the signal handler mapped?

The signal handler and the stack actually just sit on some random virtual
address. The assumption is that the guest system is cooperative and won't touch
them.

The only way to avoid this is to have another process handle all the signals,
which is not possible with signal handlers, and we are going to have to use
`ptrace`. This is a possible future direction for work.

### Linux system calls

Since we do not have libc available in the signal handler, we need to make do
with using `ecall` ourselves.

The system call that gave me the most pain is `sigaction`:

- Firstly, it doesn't really exist. The system call is `rt_sigaction` and
  expects a constant argument of the size of the signal mask.
- Secondly, the `sigaction_t` structure Linux expects isn't the same one as the
  one defined in the Glibc headers. We had to copy it from the kernel source code.

### Instruction decoding

In `src/common/riscv-bits.h` we have some helpers that deal with decoding
instructions.

### Mapping `SIGSEGV` to specific page faults

`SIGSEGV` does not tell us whether the specific page fault is a load, store, or
fetch page fault. To tell those apart, this is the hack we use:

- First look at `pc`. If the address is not mapped, raise a fetch page fault.
- If it *is* mapped, decode the instruction to see if it's a load/store, and
  raise the corresponding load/store page fault.
- Otherwise, it's weird that this would generate a page fault at all, but it
  seems to do so in QEMU. We raise a fetch page fault in this case, which seems
  to work well.
