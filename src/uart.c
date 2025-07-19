#include "common.h"

void uart_init(void) {
  volatile uint8_t *uart = UART;

  uint32_t lcr = (1 << 0) | (1 << 1);
  uart[3] = lcr;
  uart[2] = 1 << 0;
  uart[1] = 1 << 0;

  uart[3] = lcr | 1 << 7;
  uint16_t div = 592;
  uart[0] = div & 0xff;
  uart[1] = div >> 8;

  uart[3] = lcr;
}
