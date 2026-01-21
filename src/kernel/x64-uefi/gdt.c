#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

void set_gdt_descriptor(GdtEntry *entry, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags) {
  *entry = (GdtEntry){
    .limit0_15 = (uint16_t)limit,
    .base0_15 = (uint16_t)base,
    .base16_23 = (uint8_t)(base >> 16),
    .access_byte = access_byte,
    .limit16_19_flags = ((limit >> 16) & 0xF) | (flags & 0xF) << 4,
    .base24_31 = base >> 24,
  };
}

void setup_gdt_and_tss(GdtEntry gdt[GDT_COUNT], Tss *tss, void *interrupt_stack_top) {
  // SOURCE: https://wiki.osdev.org/GDT_Tutorial
  // SOURCE: https://wiki.osdev.org/Global_Descriptor_Table
  // NOTE: In long mode base and limit are ignored, each descriptor covers the whole memory

  tss->ist1 = (size_t)interrupt_stack_top;

  size_t tss_addr = (size_t)tss;

  set_gdt_descriptor(&gdt[GDT_KERNEL_CODE], 0, 0, 0x9A, 0xA);
  set_gdt_descriptor(&gdt[GDT_KERNEL_DATA], 0, 0, 0x92, 0xA);
  set_gdt_descriptor(&gdt[GDT_USER_CODE], 0, 0, 0xFA, 0xA);
  set_gdt_descriptor(&gdt[GDT_USER_DATA], 0, 0, 0xF2, 0xA);
  set_gdt_descriptor(&gdt[GDT_TSS_LOW], (uint32_t)tss_addr, sizeof(*tss) - 1, 0x89, 0xA);
  *(uint32_t *)&gdt[7] = (uint32_t)(tss_addr >> 32);

  GdtPtr gdt_table_ptr = {sizeof(GdtEntry[GDT_COUNT]) - 1, (uint64_t)gdt};
  load_gdt_table(&gdt_table_ptr);

}
