#ifndef INCLUDE_DEVICES
#define INCLUDE_DEVICES

#include "common.h"

volatile uint8_t * const UART = (void *)0x10000000;

void * const VIRTIO_BLK1 = (void *)0x10001000;
void * const VIRTIO_BLK2 = (void *)0x10002000;
void * const VIRTIO_NET = (void *)0x10003000;

const uint32_t UART_INT = 10;
const uint32_t VIRTIO_BLK1_INT = 1;
const uint32_t VIRTIO_BLK2_INT = 2;
const uint32_t VIRTIO_NET_INT = 3;

#endif

