#ifndef INCLUDE_HARDWARE
#define INCLUDE_HARDWARE

#include "common.h"

volatile uint8_t * const UART = (void *)0x10000000;
const uint32_t UART_INT = 10;

const uint32_t VIRTIO_MMIO_START = 0x10001000;
const uint32_t VIRTIO_INTERRUPT_START = 0;
const uint32_t VIRTIO_COUNT = 7;

#endif

