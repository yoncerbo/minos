#include "cmn/lib.h"
#include "common.h"

void validate_elf_header(ElfHeader64 *elf) {
  ASSERT(elf->magic == ELF_MAGIC);
  ASSERT(elf->class == ELF_CLASS64);
  ASSERT(elf->endianness == ELF_ENDIAN_LITTLE);
  ASSERT(elf->os_abi == ELF_OS_ABI_SYSTEM_V);
  ASSERT(elf->abi_version == 0);
  ASSERT(elf->type == ELF_TYPE_EXECUTABLE);
  ASSERT(elf->isa == ELF_ISA_X86_64);
  ASSERT(elf->version == 1);

  ASSERT(elf->header_size == 64);
  ASSERT(elf->program_header_size == sizeof(ElfProgramHeader64));

  ASSERT(elf->program_entry_addr);
}
