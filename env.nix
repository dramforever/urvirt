{ mkShell, qemu }:

mkShell {
  depsBuildBuild = [ qemu ];
  hardeningDisable = [ "all" ];
}
