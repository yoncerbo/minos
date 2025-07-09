CFLAGS = -std=c99 -pedantic -Wall -Wextra -O2 -g3 -fno-stack-protector -ffreestanding -nostdlib

build: src/main.c
	riscv32-none-elf-gcc ${CFLAGS} -Wl,-Tsrc/kernel.ld -Wl,-Map=out/kernel.map \
		-o out/kernel.elf src/main.c src/boot.s

run: build
	qemu-system-riscv32 -machine virt -bios default -nographic \
		-serial mon:stdio --no-reboot -kernel out/kernel.elf \
		-drive id=drive0,file=file.txt,format=raw,if=none \
		-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \

clean:
	rm build/* -rf
