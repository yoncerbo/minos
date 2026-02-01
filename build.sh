#!/bin/sh
set -o pipefail
set -e

CFLAGS="$CFLAGS
  -std=c99 -O1
  -g3
  -I ./src 
  -Wall -Wextra -pedantic-errors
  -Wno-unused-parameter -Wno-unused-const-variable -Wno-unused-variable -Wno-unused-function
  -fno-stack-protector -ffreestanding
  -fno-omit-frame-pointer
  -static
  -nostdlib
  -mgeneral-regs-only
  -mno-red-zone
  -masm=intel
"

CLANG_UEFI_FLAGS="-target x86_64-pc-win32-coff
  -DBUILD_DEBUG
  -fshort-wchar
  -Wl,-entry:efi_main,
  -Wl,-subsystem:efi_application
  -Wl,/base:0
  -fuse-ld=lld-link
  -gdwarf
"
  # -gdwarf

DIR=src/kernel/$TARGET
USR=src/user
OUT=out/$TARGET

build() {
  mkdir -p $OUT

  case "$TARGET" in
    "rv32-sbi")
      $CC $CFLAGS -Wl,-T$DIR/linker.ld -Wl,-Map=$OUT/kernel.map -DARCH_RV32 \
        -o $OUT/kernel.elf $DIR/main.c $DIR/boot.s
      ;;
    "x64-uefi")
      $CC $CFLAGS $USR/main.c -o $OUT/user_main.elf -DARCH_X64 \
        -I ./src/user/

      $CC $CFLAGS $DIR/kernel.c -o $OUT/kernel.elf -DARCH_X64 \
        -I ./src/kernel -I ./src/kernel/headers/ -I $DIR \
        -mcmodel=kernel -Wl,--image-base=0xFFFFFFFFFFF00000

      # $CC $CFLAGS $USER_FLAGS $USR/main.c -o $OUT/user_main.bin -DARCH_X64
      # nasm -fbin src/user/example.s -o out/user/example.bin
      clang -I $DIR $CFLAGS $CLANG_UEFI_FLAGS \
        -o $OUT/BOOTX64.EFI $DIR/efi_boot.c $DIR/utils.s -DARCH_X64 \
        -I ./src/kernel -I ./src/kernel/headers -I $DIR

      mcopy -i fat.img $OUT/BOOTX64.EFI ::/EFI/BOOT -D o
      mcopy -i fat.img $OUT/kernel.elf ::/ -D o
      ;;
    *)
      echo "Unknown taret '$TARGET'"
      ;;
  esac

}

X64_QEMU_FLAGS="
  -bios $OVMF_FD -drive file=fat.img,format=raw
  -m 2G
  -smp 4
  -no-reboot -no-shutdown
  -net none
  -machine q35
  --enable-kvm
"
# -d int -M smm=off \

debug() {
  build

  case "$TARGET" in
    "x64-uefi")
      qemu-system-x86_64 $X64_QEMU_FLAGS \
        -s -S -serial file:out/x64-uefi/qemu.log &
      gdb
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
      qemu-system-x86_64 $X64_QEMU_FLAGS \
        -debugcon mon:stdio \
        -d int \
        -device virtio-keyboard-pci \
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

# Useful commands:
#
# Disassemble flat binary file:
#   x86_64-elf-objdump -b binary -m i386:x86-64 -D out/user/example.bin

"$@"
