{ mkShell, qemu }:

mkShell {
  depsBuildBuild = [ qemu ];
}
