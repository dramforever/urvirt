# URVirt

URVirt is a U-mode trap-and-emulate hypervisor for RISC-V.

## Related projects

[RVirt][rvirt] is an S-mode trap-and-emulate hypervisor for RISC-V. It runs on an M-mode firmware and translates guest privileged instructions into native RISC-V privileged architecture functionality, whereas URVirt translates them to Linux system calls. The name 'URVirt' directly comes from 'User-mode RVirt'.

[rvirt]: https://github.com/mit-pdos/RVirt
