CFLAGS = -std=c99 -O2 -g3 -Wall -Wextra -fno-stack-protector -ffreestanding -nostdlib

build: src/main.c src/boot.s
	riscv32-none-elf-gcc ${CFLAGS} -Wl,-Tsrc/kernel.ld -Wl,-Map=out/kernel.map \
		-o out/kernel.elf src/main.c src/boot.s -I ./src/headers

run: build
	qemu-system-riscv32 -machine virt -bios default -kernel out/kernel.elf \
		-serial mon:stdio --no-reboot \
		-drive id=drive0,file=fat.fs,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-drive id=drive1,file=disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive1,bus=virtio-mmio-bus.1 \
		-netdev user,id=net0 \
		-device virtio-net-device,bus=virtio-mmio-bus.2,netdev=net0 \
		-object filter-dump,id=f1,netdev=net0,file=out/net_dump.dat \
		-device virtio-gpu-device,bus=virtio-mmio-bus.3 \

build-tests: src/tests.c src/boot.s
	riscv32-none-elf-gcc ${CFLAGS} -Wl,-Tsrc/kernel.ld -Wl,-Map=out/kernel.map \
		-o out/kernel.elf src/tests.c src/boot.s -I ./src/headers

test: build-tests
	qemu-system-riscv32 -machine virt -bios default -kernel out/kernel.elf -nographic \
		-serial mon:stdio --no-reboot \
		-drive id=drive0,file=fat.fs,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
		-drive id=drive1,file=disk.tar,format=raw,if=none \
		-device virtio-blk-device,drive=drive1,bus=virtio-mmio-bus.1 \
		-netdev user,id=net0 \
		-device virtio-net-device,bus=virtio-mmio-bus.2,netdev=net0 \
		-object filter-dump,id=f1,netdev=net0,file=out/net_dump.dat \

clean:
	rm build/* -rf

make-fs:
	dd if=/dev/zero of=fat.fs bs=1024 count=1K
	mkfs.vfat fat.fs

mount-fs:
	mkdir -p fs
	sudo mount -o loop,uid=1000 fat.fs ./fs
