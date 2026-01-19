#include "common.h"

// https://9p.io/sources/contrib/geoff/riscv/riscv-plic.pdf
// Chapter 3. Memory Map

const uint32_t PLIC_PRIORITY = 0x0c000000;
const uint32_t PLIC_PENDING = 0x0c001000;

// context 1 - supervisor mode
const uint32_t PLIC_ENABLE = 0x0c002000 + 0x80;
const uint32_t PLIC_THRESHOLD = 0x0c200000 + 0x1000;
const uint32_t PLIC_CLAIM = 0x0c200004 + 0x1000;

extern inline void plic_enable(uint32_t id) {
  volatile uint32_t *enables = (void *)PLIC_ENABLE;
  *enables |= 1 << id;
}

// priority is from 0 to 7
extern inline void plic_set_priority(uint32_t id, uint8_t priority) {
  volatile uint32_t *priorities = (void *)PLIC_PRIORITY;
  priorities[id] = priority & 7;
}

extern inline void plic_set_threshold(uint8_t threshold) {
  volatile uint32_t *plic_threshold = (void *)PLIC_THRESHOLD;
  *plic_threshold = threshold & 7;
}

extern inline uint32_t plic_claim(void) {
  volatile uint32_t *claim = (void *)PLIC_CLAIM;
  return *claim;
}

extern inline void plic_complete(uint32_t id) {
  volatile uint32_t *claim = (void *)PLIC_CLAIM;
  *claim = id;
}

extern inline void plic_enablep(uint32_t id, uint8_t priority) {
  plic_enable(id);
  plic_set_priority(id, priority);
}
