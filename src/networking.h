#ifndef INCLUDE_NETWORKING
#define INCLUDE_NETWORKING

#include "common.h"
#include "virtio_net.h"

#define IP(x, y, z, w) ((x) << 24 | (y) << 16 | (z) << 8 | (w))

const uint32_t CLIENT_IP = IP(10,0,2,15);
const uint32_t SERVER_IP = IP(10,0,2,2);

const uint8_t BROADCAST_MAC[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const uint32_t BROADCAT_IP = 0xffffffff;

typedef struct {
  uint8_t *mac; 
  uint32_t ip;
  uint16_t port;
} NetCon;

void net_dhcp_request(VirtioNetdev *netdev);

#endif // !INCLUDE_NETWORKING
