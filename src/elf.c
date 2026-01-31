#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

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

void load_elf_file(PageAllocator2 *alloc, PageTable *pml4, void *file, vaddr_t *out_entry) {
  ElfHeader64 *elf = file;
  uint8_t *bytes = file;
  validate_elf_header(elf);

  ElfProgramHeader64 *progs = (void *)(bytes + elf->program_table_offset);
  for (uint32_t i = 0; i < elf->program_table_entry_count; ++i) {
    ElfProgramHeader64 *prog = &progs[i];
    if (progs->type != ELF_PROG_EXECUTABLE) continue;
    log("program header: %d, type=%d", i, progs->type);
    ASSERT(prog->alignment <= PAGE_SIZE);

    uint32_t page_count = (PAGE_SIZE - 1 + prog->size_in_memory) / PAGE_SIZE;

    paddr_t physical = alloc_pages2(alloc, page_count);

    memcpy((void *)physical, (void *)(bytes + prog->file_offset), prog->size_in_file);

    map_pages(alloc, pml4, physical, prog->virtual_addr, prog->size_in_memory,
        PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE | PAGE_BIT_USER);
  }
  *out_entry = elf->program_entry_addr;
}

void load_elf_file2(MemoryManager *mm, void *file, vaddr_t *out_entry) {
  ElfHeader64 *elf = file;
  uint8_t *bytes = file;
  validate_elf_header(elf);

  ElfProgramHeader64 *progs = (void *)(bytes + elf->program_table_offset);
  for (uint32_t i = 0; i < elf->program_table_entry_count; ++i) {
    ElfProgramHeader64 *prog = &progs[i];
    if (progs->type != ELF_PROG_EXECUTABLE) continue;
    log("program header: %d, type=%d", i, progs->type);
    ASSERT(prog->alignment <= PAGE_SIZE);

    uint32_t page_count = (PAGE_SIZE - 1 + prog->size_in_memory) / PAGE_SIZE;

    paddr_t physical = alloc_pages2(mm->page_alloc, page_count);

    memcpy((void *)(physical + mm->virtual_offset), (void *)(bytes + prog->file_offset), prog->size_in_file);

    map_virtual_range(mm, prog->virtual_addr, physical, prog->size_in_memory,
        PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE | PAGE_BIT_USER);
  }
  *out_entry = elf->program_entry_addr;
}
