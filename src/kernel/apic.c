#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

enum {
  PIC_MASTER_COMMAND = 0x20,
  PIC_MASTER_DATA = 0x20,
  PIC_SLAVE_COMMAND = 0xA0,
  PIC_SLAVE_DATA = 0xA1,
};

enum{
  ICW_INIT = 0x11,
  ICW_MASTER_IDT = 0x20,
  ICW_SLAVE_IDT = 0x28,
  ICW_8086_MODE = 0x01,
};

enum {
  MSR_APIC_BASE = 0x1B,
};

// SOURCE: https://wiki.osdev.org/8259_PIC#Disabling

void disable_pic(void) {
  WRITE_PORT(PIC_MASTER_COMMAND, ICW_INIT);
  WRITE_PORT(PIC_SLAVE_COMMAND, ICW_INIT);

  WRITE_PORT(PIC_MASTER_DATA, ICW_MASTER_IDT);
  WRITE_PORT(PIC_SLAVE_DATA, ICW_SLAVE_IDT);

  WRITE_PORT(PIC_MASTER_DATA, 0x2);
  WRITE_PORT(PIC_SLAVE_DATA, 0x4);

  WRITE_PORT(PIC_MASTER_DATA, ICW_8086_MODE);
  WRITE_PORT(PIC_SLAVE_DATA, ICW_8086_MODE);

}

void set_pit_periodic(uint16_t count) {
  WRITE_PORT(0x43, 0x34);
  WRITE_PORT(0x40, count & 0xFF);
  WRITE_PORT(0x40, count >> 8);
}

void setup_apic(MemoryManager *mm, Apic *out_apic) {
  // TODO: Make sure APIC is present using CPUID intruction

  WRITE_PORT(PIC_MASTER_DATA, 0xFF); // mask out all the interrupts
  WRITE_PORT(PIC_SLAVE_DATA, 0xFF);

  uint32_t low, high;
  READ_MSR(MSR_APIC_BASE, low, high);

  if (low & (1 << 8)) {
    log("Bootstrap processor");
  }
  ASSERT(low & (1 << 11)); // APIC global enable

  // TODO: map the address
  paddr_t apic_addr = low & 0xFFFFF000;
  DEBUGX(apic_addr);

  volatile uint32_t *apic_regs = (void *)alloc_physical(mm, apic_addr, PAGE_SIZE * 4,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE);
  flush_page_table(mm);
  out_apic->regs = apic_regs;
  out_apic->id = apic_regs[APIC_LOCAL_ID];

  apic_regs[APIC_SPURIOUS_VECTOR] = 0x1FF;

  apic_regs[APIC_LVT] = 0xF0;
  apic_regs[APIC_TIMER_TICKS] = 0;
}

uint32_t read_ioapic_register(size_t io_apic_addr, size_t register_select) {
  volatile uint32_t *sel = (void *)io_apic_addr;
  volatile uint32_t *reg = (void *)(io_apic_addr + 16);

  *sel = register_select;
  return *reg;
}

void write_ioapic_register(size_t io_apic_addr, size_t register_select, uint32_t value) {
  volatile uint32_t *sel = (void *)io_apic_addr;
  volatile uint32_t *reg = (void *)(io_apic_addr + 16);

  *sel = register_select;
  *reg = value;
}
