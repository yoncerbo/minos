target remote localhost:1234
watch *(unsigned long long *)0x10000 == 0xDEADBEEF
continue
delete 1
set $base = *(unsigned long long *)(0x10000 + 8)
add-symbol-file out/x64-uefi/BOOTX64.EFI -o $base
set disassembly-flavor intel

add-symbol-file out/x64-uefi/kernel.elf
