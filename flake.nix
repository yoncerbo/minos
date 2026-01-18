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
      x64 = pkgs.pkgsCross.x86_64-embedded.mkShell rec {
      # x64 = pkgs.pkgsCross.mingwW64.mkShell rec {
        TARGET = "x64-uefi";
        packages = with pkgs; [
          pkgsCross.mingwW64.buildPackages.gcc
          clang.cc
          lld
          bear
          qemu
          mtools
          llvmPackages.bintools
        ];
        GNU_EFI = "${pkgs.gnu-efi}";
        # shellHook = "export CC=clang";
        OVMF_FD = "${pkgs.OVMF.fd}/FV/OVMF.fd";
      };
    };
  };
}

