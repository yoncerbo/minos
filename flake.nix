{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  };
  outputs = { self, nixpkgs, }:
  let 
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in {
    devShells."${system}" = {
      rv32 = pkgs.pkgsCross.riscv32-embedded.mkShell rec {
        TARGET = "rv32-sbi";
        packages = with pkgs; [
          bear
          qemu
          gnumake
        ];
      };
      x64 = pkgs.mkShell rec {
        TARGET = "x64-uefi";
        packages = with pkgs; [
          clang.cc
          lld
          bear
          qemu
          mtools
        ];
        shellHook = "export CC=clang";
        OVMF_FD = "${pkgs.OVMF.fd}/FV/OVMF.fd";
      };
    };
  };
}

