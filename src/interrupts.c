#include "common.h"

__attribute__((aligned(4)))
__attribute__((interrupt("supervisor")))
void handle_interrupt(void) {
  uint32_t scause, stval, sepc;
  __asm__ __volatile__(
      "csrr %0, scause \n"
      "csrr %1, stval \n"
      "csrr %2, sepc \n"
      : "=r"(scause), "=r"(stval), "=r"(sepc)
  );
  uint32_t interrupt = scause >> 31;
  if (interrupt) {
    uint32_t code = scause & 0x7fffffff;
    switch (code) {
      case 1:
        PANIC("Supervisor software interrupt");
        break;
      case 5:
        PANIC("Supervisor timer interrupt");
        break;
      case 9:
        PANIC("Supervisor external interrupt");
        break;
      default:
        PANIC("Unknown interrupt cause=%d", code);
    }
    return;
  }
  switch (scause) {
    case 0:
      PANIC("Instruction addresss misaligned stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 1:
      PANIC("Instruction access fault stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 2:
      PANIC("Illegal instruction stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 3:
      PANIC("Breakpoint stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 4:
      PANIC("Load address misaligned stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 5:
      PANIC("Load access fault stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 6:
      PANIC("Store/AMO address misaligned stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 7:
      PANIC("Store/AMO access fault stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 8:
      PANIC("Environment call from U-mode stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 9:
      PANIC("Environment call from S-mode stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 12:
      PANIC("Instruction page fault stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 13:
      PANIC("Load page fault stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    case 15:
      PANIC("Store/AMO page fault stval=0x%x, sepc=0x%x", stval, sepc);
      break;
    default:
      PANIC("Unknown exception scause=%d, stval=%d, sepc=%x\n", scause, stval, sepc);
  }
}
