#include "common.h"
#include "interfaces/gpu.h"
#include "interfaces/blk.h"
#include "interfaces/input.h"

// Source files
#include "drawing.c"

void kernel_init(Hardware *hw) {
  LOG("Hello World!\n");

  ASSERT(hw->gpu);

  Surface surface;
  gpu_v1_get_surface(hw->gpu, &surface);

  for (uint32_t i = 0; i < surface.width * surface.height; ++i) {
    surface.ptr[i] = bswap32(0x333333ff);
  }

  draw_line(&surface, 50, 50, RED, "Hello, World!", -1);
  draw_line(&surface, 50, 62, GREEN, "Some more strings", -1);
  draw_line(&surface, 50, 74, BLUE, "Blue text\nsome more", -1);


  ASSERT(hw->blk_devices_count == 2);
  char buffer[SECTOR_SIZE];
  blk_v1_read_write_sectors(&hw->blk_devices[1], buffer, 0, 1, BLKDEV_READ);

  draw_line(&surface, 50, 100, WHITE, buffer, SECTOR_SIZE);

  gpu_v1_flush(hw->gpu);

  for (uint32_t i = 0; i < hw->input_devices_count; ++i) {
    char buffer[128 + 1];
    uint32_t written = input_v1_get_name(&hw->input_devices[i], buffer, 128);
    buffer[written] = 0;
    LOG("Input device at %d: %s\n", i, buffer);
  }
}

void kernel_update(Hardware *hw) {
  InputEvent events[16];
  for (uint32_t dev_index = 0; dev_index < hw->input_devices_count; ++dev_index) {
    uint32_t events_written = input_v1_read_events(&hw->input_devices[dev_index], events, 16);
    for (int i = 0; i < events_written; ++i) {
      if (events[i].type == EV_KEY && events[i].code == KEY_A && events[i].value == 0) {
        LOG("key A released\n", 0);
      }
    }
  }
}
