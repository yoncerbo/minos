#include "common.h"

// https://wiki.qemu.org/Documentation/Networking
// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-1940001

#define IP(x, y, z, w) ((x) << 24 | (y) << 16 | (z) << 8 | (w))

#define bswap16(n) __builtin_bswap16(n)
#define bswap32(n) __builtin_bswap32(n)

uint32_t CLIENT_IP = IP(10,0,2,15);
uint32_t SERVER_IP = IP(10,0,2,2);

const uint8_t BROADCAST_MAC[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

typedef struct {
  uint8_t destination_mac[6];
  uint8_t source_mac[6];
  uint16_t ether_type;
  uint8_t data[];
} __attribute__((packed)) EthHeader;

typedef struct {
  uint16_t htype;
  uint16_t ptype;
  uint8_t hlen;
  uint8_t plen;
  uint16_t opcode;
  uint8_t sender_mac[6];
  uint32_t sender_ip;
  uint8_t target_mac[6];
  uint32_t target_ip;
} __attribute__((packed)) ArpHeader;

// TODO: respond to arp request

void test_networking(VirtioNetdev *netdev) {
  EthHeader *eth = (void*)alloc_pages(1);
  memcpy(eth->destination_mac, BROADCAST_MAC, 6);
  memcpy(eth->source_mac, netdev->mac, 6);
  eth->ether_type = bswap16(0x806);

  ArpHeader *arp = (void *)&eth->data;
  *arp = (ArpHeader){
    .htype = bswap16(1),
    .ptype = bswap16(0x800),
    .hlen = 4,
    .plen = 6,
    .opcode = bswap16(1),
    // .sender_mac = mac,
    .sender_ip = bswap32(CLIENT_IP),
    .target_mac = {0},
    .target_ip = bswap32(SERVER_IP),
  };
  memcpy(arp->sender_mac, netdev->mac, 6);

  virtio_net_send(netdev, (char *)eth, sizeof(*eth) + sizeof(*arp));
}
