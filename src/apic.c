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

enum {
  APIC_LOCAL_ID = 0x20 / 4,
  APIC_END_OF_INTERRUPT = 0xB0 / 4,
  APIC_SPURIOUS_VECTOR = 0xF0 / 4,
  APIC_LVT = 0x320 / 4,
  APIC_TIMER_TICKS = 0x380 / 4,
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

  WRITE_PORT(PIC_MASTER_DATA, 0xFF); // mask out all the interrupts
  WRITE_PORT(PIC_SLAVE_DATA, 0xFF);
}

void set_pit_periodic(uint16_t count) {
  WRITE_PORT(0x43, 0x34);
  WRITE_PORT(0x40, count & 0xFF);
  WRITE_PORT(0x40, count >> 8);
}

void setup_apic(void) {
  // TODO: Make sure APIC is present using CPUID intruction
  disable_pic();

  uint32_t low, high;
  READ_MSR(MSR_APIC_BASE, low, high);

  if (low & (1 << 8)) {
    printf("Bootstrap processor\n");
  }
  ASSERT(low & (1 << 11)); // APIC global enable

  // TODO: map the address
  size_t apic_addr = low & 0xFFFFF000;
  DEBUGX(apic_addr);
  volatile uint32_t *apic_regs = (void *)apic_addr;

  apic_regs[APIC_SPURIOUS_VECTOR] = 0x1FF;

  set_pit_periodic(1000);
  // Configure the timer
  // apic_regs[APIC_LVT] = 0xF0;
  // apic_regs[APIC_TIMER_TICKS] = 100;
}
