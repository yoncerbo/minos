#ifndef INCLUDE_INTERFACE_INPUT
#define INCLUDE_INTERFACE_INPUT

#include "common.h"
#include "input_codes.h"

typedef struct PACKED {
  uint16_t type;
  uint16_t code;
  uint32_t value;
} InputEvent;

typedef struct {
  uint32_t min, max, fuzz, flag, res;
} InputAbsInfo;

typedef struct InputDev InputDev;

// Returns the number of events read
uint32_t input_v1_get_events(InputDev *dev, InputEvent *out_events, uint32_t event_limit);

// TODO: Some way to get the name's size or make sure to read the whole name
// TODO: Maybe return how much would be written instead
uint32_t input_v1_get_name(InputDev *dev, char *buffer, uint32_t limit);

// TODO: We might not want to read the bits, but only check if the size is greater than 0
uint32_t input_v1_get_capabilities(InputDev *dev, EventType type, uint8_t *buffer, uint32_t limit);

// TODO: Handling errors
void input_v1_get_abs_info(InputDev *dev, uint8_t axis, InputAbsInfo *out_abs_info);

#endif
