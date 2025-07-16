#include "common.h"

void report_exception(uint32_t cause, uint32_t val, uint32_t epc) {
  switch (cause) {
    case 0:
      PANIC("Instruction addresss misaligned val=0x%x, epc=0x%x", val, epc);
      break;
    case 1:
      PANIC("Instruction access fault val=0x%x, epc=0x%x", val, epc);
      break;
    case 2:
      PANIC("Illegal instruction val=0x%x, epc=0x%x", val, epc);
      break;
    case 3:
      PANIC("Breakpoint val=0x%x, epc=0x%x", val, epc);
      break;
    case 4:
      PANIC("Load address misaligned val=0x%x, epc=0x%x", val, epc);
      break;
    case 5:
      PANIC("Load access fault val=0x%x, epc=0x%x", val, epc);
      break;
    case 6:
      PANIC("Store/AMO address misaligned val=0x%x, epc=0x%x", val, epc);
      break;
    case 7:
      PANIC("Store/AMO access fault val=0x%x, epc=0x%x", val, epc);
      break;
    case 8:
      PANIC("Environment call from U-mode val=0x%x, epc=0x%x", val, epc);
      break;
    case 9:
      PANIC("Environment call from S-mode val=0x%x, epc=0x%x", val, epc);
      break;
    case 12:
      PANIC("Instruction page fault val=0x%x, epc=0x%x", val, epc);
      break;
    case 13:
      PANIC("Load page fault val=0x%x, epc=0x%x", val, epc);
      break;
    case 15:
      PANIC("Store/AMO page fault val=0x%x, epc=0x%x", val, epc);
      break;
    default:
      PANIC("Unknown exception cause=%d, val=%d, epc=%x\n", cause, val, epc);
  }
}

__attribute__((aligned(4)))
__attribute__((interrupt("supervisor")))
void handle_supervisor_interrupt(void) {
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
        printf("Timer\n");
        uint64_t time;
        __asm__ __volatile__("rdtime %0" : "=r"(time));
        sbi_set_timer(time + 10000000);
        break;
      case 9:
        PANIC("Supervisor external interrupt");
        break;
      default:
        PANIC("Unknown interrupt cause=%d", code);
    }
    uint32_t mask = 1 << code;
    __asm__ __volatile__(
        "csrc sip, %0"
        :
        : "r"(mask)
    );
    return;
  }
  report_exception(scause, stval, sepc);
}

__attribute__((aligned(4)))
__attribute__((interrupt("machine")))
void handle_machine_interrupt(void) {
  uint32_t mcause, mtval, mepc;
  __asm__ __volatile__(
      "csrr %0, mcause \n"
      "csrr %1, mtval \n"
      "csrr %2, mepc \n"
      : "=r"(mcause), "=r"(mtval), "=r"(mepc)
  );
  uint32_t interrupt = mcause >> 31;
  if (interrupt) {
    uint32_t code = mcause & 0x7fffffff;
    switch (code) {
      case 3:
        PANIC("Machine software interrupt");
        break;
      case 7:
        PANIC("Machine timer interrupt");
        break;
      case 11:
        PANIC("Machine external interrupt");
        break;
      default:
        PANIC("Unknown interrupt cause=%d", code);
    }
    return;
  }
  report_exception(mcause, mtval, mepc);
}
