#include "common.h"
#include <string.h>

// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-1940001

typedef enum {
  VIRTIO_NET_F_MAC = 5,
} ViritoNetFeatures;

typedef struct {
  uint8_t mac[6];
  uint16_t status;
  uint16_t max_virtq_pairs;
  uint16_t mtu;
} __attribute__((packed)) VirtioNetConfig;

typedef struct {
  uint8_t flags;
  uint8_t gso_type;
  uint16_t hdr_len;
  uint16_t gso_size;
  uint16_t csum_start;
  uint16_t num_buffers;
} __attribute__((packed)) VirtioNetHeader;

typedef struct {
  VirtioDevice *dev;
  Virtq *rq;
  Virtq *tq
} VirtioNetdev;

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

#define IP(x, y, z, w) ((x) << 24 | (y) << 16 | (z) << 8 | (w))

#define bswap16(n) __builtin_bswap16(n)
#define bswap32(n) __builtin_bswap32(n)

void test_netdev(void) {
  VirtioDevice *dev = (void *)0x010003000;

  ASSERT(dev->magic == 0x74726976);
  ASSERT(dev->version == 1);
  ASSERT(dev->device = VIRTIO_DEVICE_NET);

  dev->status = 0;
  dev->status |= VIRTIO_STATUS_ACK;
  dev->status |= VIRTIO_STATUS_DRIVER;
  // Will this work?
  uint32_t features = dev->host_features;
  DEBUGD(features & VIRTIO_NET_F_MAC);
  dev->guest_features = VIRTIO_NET_F_MAC;
  dev->status |= VIRTIO_STATUS_FEAT_OK;

  uint8_t *mac = (void *)dev->config;
  printf("MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1],
      mac[2], mac[3], mac[4], mac[5]);

  // TODO: the MTU stuff

  Virtq *rq = virtq_create(dev, 0);
  Virtq *tq = virtq_create(dev, 1);

  // 3. Fill the receive queues with buffers 5.1.6.3
  // 5. if you don't have mac addres, generate a random one

  dev->status |= VIRTIO_STATUS_DRIVER_OK;

  // Send completely checksumed packet
  // flags zero, gso_type = hdr_gso_none
  // the header and packet are added as one output descriptor
  
  VirtioNetHeader header = {
    .num_buffers = 0,
    .flags = 0,
    .gso_type = 0,
  };

  EthHeader *eth = (void*)alloc_pages(1);
  memcpy(eth->destination_mac, BROADCAST_MAC, 6);
  memcpy(eth->source_mac, mac, 6);
  eth->ether_type = bswap16(0x806);

  ArpHeader *arp = (void *)&eth->data;
  *arp = (ArpHeader){
    .htype = bswap16(1),
    .ptype = bswap16(0x800),
    .hlen = 4,
    .plen = 6,
    .opcode = bswap16(1),
    // .sender_mac = mac,
    .sender_ip = bswap32(IP(10,0,2,15)),
    .target_mac = {0},
    .target_ip = bswap32(IP(10,0,2,2)),
  };
  memcpy(arp->sender_mac, mac, 6);

  tq->descs[0] = (VirtqDesc){
    .addr = (uint32_t)&header,
    .len = sizeof(header),
    .next = 1,
    .flags = VIRTQ_DESC_NEXT,
  };
  tq->descs[1] = (VirtqDesc){
    .addr = (uint32_t)eth,
    .len = sizeof(*eth) + sizeof(*arp),
  };

  tq->avail.ring[tq->avail.index++ % VIRTQ_ENTRY_NUM] = 0;
  __sync_synchronize();
  dev->queue_notify = 1;
  printf("Notified\n");

  while (tq->avail.index != tq->used.index) {
    printf("Waiting\n");
  }
}

// Ethernet
// destination mac 6
// source mac 6
// ether type 2

// ARP - Address Resolution Protocol
// translates ip address into mac
// ether type 0x806
// Header:
// hardware type (ethernet = 1)
// protocol type (ip = 0x800)
// hardware size = 6
// protocol size = 4
// opcode (request = 1, response = 2)
// sender mac
// destination mac
// sender ip
// destination ip
// fill ethernet address with all ones - broadcast

// respond to arp requests

// IPv4 struct

// tcp - protocol number = 6
