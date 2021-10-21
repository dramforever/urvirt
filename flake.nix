{
  inputs.nixpkgs = {
    url = "github:NixOS/nixpkgs/nixos-unstable";
    flake = false;
  };

  outputs = { self, nixpkgs }: {
    devShell.x86_64-linux =
      (import nixpkgs {
        system = "x86_64-linux";
        crossSystem.config = "riscv64-unknown-linux-musl";
      }).callPackage ./env.nix {};
  };
}
