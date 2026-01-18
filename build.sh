#!/bin/sh
set -o pipefail
set -e

CFLAGS="$CFLAGS
  -std=c99 -O1 -g3
  -I ./src -I ./src/headers
  -Wall -Wextra -pedantic-errors
  -Wno-unused-parameter -Wno-unused-const-variable -Wno-unused-variable -Wno-unused-function
  -fno-stack-protector -ffreestanding
  -mgeneral-regs-only
  -nostdlib"

UEFI_FLAGS="-target x86_64-unknown-windows
  -fshort-wchar
  -Wl,-entry:efi_main,
  -Wl,-subsystem:efi_application
  -fuse-ld=lld
  -mno-red-zone
  -masm=intel"

DIR=src/hardware/$TARGET
OUT=out/$TARGET

build() {
  mkdir -p $OUT

  case "$TARGET" in
    "rv32-sbi")
      $CC $CFLAGS -Wl,-T$DIR/linker.ld -Wl,-Map=$OUT/kernel.map -DARCH_RV32 \
        -o $OUT/kernel.elf $DIR/main.c $DIR/boot.s
      ;;
    "x64-uefi")
      clang $CFLAGS $UEFI_FLAGS -o $OUT/BOOTX64.EFI $DIR/main.c $DIR/utils.s -DARCH_X64
      mcopy -i fat.img $OUT/BOOTX64.EFI ::/EFI/BOOT -D o
      ;;
    *)
      echo "Unknown taret '$TARGET'"
      ;;
  esac

}

run() {
  build

  case "$TARGET" in
    "rv32-sbi")
      qemu-system-riscv32 -machine virt -bios default -kernel $OUT/kernel.elf \
        -serial mon:stdio --no-reboot \
        -drive id=drive0,file=fat.fs,format=raw,if=none \
        -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
        -drive id=drive1,file=file.txt,format=raw,if=none,copy-on-read=off \
        -device virtio-blk-device,drive=drive1,bus=virtio-mmio-bus.1 \
        -netdev user,id=net0 \
        -device virtio-net-device,bus=virtio-mmio-bus.2,netdev=net0 \
        -object filter-dump,id=f1,netdev=net0,file=out/net_dump.dat \
        -device virtio-keyboard-device,bus=virtio-mmio-bus.4 \
        -device virtio-tablet-device,bus=virtio-mmio-bus.5 \
        -audio id=audiodev0,driver=sdl,model=virtio \
        -device virtio-gpu-device,bus=virtio-mmio-bus.3 \
        # -device virtio-sound-device,bus=virtio-mmio-bus.6,audiodev=audiodev0 \
      ;;
    "x64-uefi")
      qemu-system-x86_64 -bios $OVMF_FD \
        -drive file=fat.img,format=raw,media=disk \
        -m 1G \
        -debugcon mon:stdio \
        -no-reboot \
        # -d int \
      ;;
    *)
      echo "Unknown taret '$TARGET'"
      ;;
  esac
}

make-fs() {
  dd if=/dev/zero of=fat.fs bs=1024 count=1K
  mkfs.vfat fat.fs
}

mount-fs() {
  mkdir -p fs
  sudo mount -o loop,uid=1000 fat.fs ./fs
}

make-tar() {
  cd disk
  tar cf ../disk.tar --format=ustar *
}

clean() {
  rmdir fs
  rm disk.tar
  rm out -rf
}

make-fat-image() {
  dd if=/dev/zero of=fat.img bs=1k count=1440
  mformat -i fat.img -f 1440 ::
  mmd -i fat.img ::/EFI
  mmd -i fat.img ::/EFI/BOOT
}

"$@"
