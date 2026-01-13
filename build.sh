#!/bin/sh
set -o pipefail
set -e

CFLAGS="$CFLAGS
  -std=c99 -O1 -g3
  -I ./src -I ./src/headers
  -Wall -Wextra
  -Wno-unused-parameter -Wno-unused-const-variable -Wno-unused-variable -Wno-unused-function
  -fno-stack-protector -ffreestanding
  -nostdlib"

build() {
  $CC $CFLAGS -Wl,-Tsrc/hardware/kernel.ld -Wl,-Map=out/kernel.map \
    -o out/kernel.elf src/hardware/main.c src/hardware/boot.s
}

run() {
  build
	qemu-system-riscv32 -machine virt -bios default -kernel out/kernel.elf \
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

"$@"
