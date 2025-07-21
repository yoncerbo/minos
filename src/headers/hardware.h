#ifndef INCLUDE_HARDWARE
#define INCLUDE_HARDWARE

#include "common.h"

volatile uint8_t * const UART = (void *)0x10000000;

void * const VIRTIO_BLK1 = (void *)0x10001000;
void * const VIRTIO_BLK2 = (void *)0x10002000;
void * const VIRTIO_NET = (void *)0x10003000;
void * const VIRTIO_GPU = (void *)0x10004000;
void * const VIRTIO_KEYBOARD = (void *)0x10005000;
void * const VIRTIO_MOUSE = (void *)0x10006000;

const uint32_t UART_INT = 10;
const uint32_t VIRTIO_BLK1_INT = 1;
const uint32_t VIRTIO_BLK2_INT = 2;
const uint32_t VIRTIO_NET_INT = 3;
const uint32_t VIRTIO_GPU_INT = 4;
const uint32_t VIRTIO_KEYBOARD_INT = 5;
const uint32_t VIRTIO_MOUSE_INT = 6;

#endif

