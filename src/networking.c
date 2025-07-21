#include "common.h"

// https://wiki.qemu.org/Documentation/Networking
// https://docs.oasis-open.org/virtio/virtio/v1.1/csprd01/virtio-v1.1-csprd01.html#x1-1940001

#define IP(x, y, z, w) ((x) << 24 | (y) << 16 | (z) << 8 | (w))

#define bswap16(n) __builtin_bswap16(n)
#define bswap32(n) __builtin_bswap32(n)

uint32_t CLIENT_IP = IP(10,0,2,15);
uint32_t SERVER_IP = IP(10,0,2,2);

const uint8_t BROADCAST_MAC[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const uint32_t BROADCAT_IP = 0xffffffff;

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

// https://en.wikipedia.org/wiki/IPv4#Header
// TODO: Can I use bit-fields?
typedef struct {
  uint8_t version_ihl; // 4, 4 bits
  uint8_t dscp_ecn; // 6, 2 bits
  uint16_t total_length;
  uint16_t identification;
  uint16_t flags_fragment_offset; // 3, 13 bits
  uint8_t ttl;
  uint8_t protocol;
  uint16_t header_checksum;
  uint32_t source_addr;
  uint32_t destination_addr;
  uint8_t data[];
} __attribute__((packed)) Ipv4Header;

typedef struct {
  uint16_t source_port;
  uint16_t destination_port;
  uint16_t length;
  uint16_t checksum;
  uint8_t data[];
} __attribute__((packed)) UdpHeader;

// https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol#Discovery
typedef struct {
  uint8_t operation;
  uint8_t hardware_type;
  uint8_t hardware_length;
  uint8_t hops;
  uint32_t transaction_id;
  uint16_t seconds_elapsed;
  uint16_t flags;
  uint32_t client_ip;
  uint32_t offered_ip;
  uint32_t server_address;
  uint32_t gateway_address;
  uint8_t chaddr[16];
  uint8_t sname[64];
  uint8_t file[128];
  uint8_t options[312];
} __attribute__((packed)) DhcpHeader;

typedef struct {
  uint8_t type;
  uint8_t code;
  uint16_t checksum;
  uint16_t id;
  uint16_t sequence;
  uint8_t payload[56];
} __attribute__((packed)) IcmpHeader;

typedef struct {
  uint8_t *mac; 
  uint32_t ip;
  uint16_t port;
} NetCon;

const int NET_SIZE_ETH = sizeof(EthHeader);
const int NET_SIZE_ARP = NET_SIZE_ETH + sizeof(ArpHeader);
const int NET_SIZE_IPV4 = NET_SIZE_ETH + sizeof(Ipv4Header);
const int NET_SIZE_UDP = NET_SIZE_IPV4 + sizeof(UdpHeader);
const int NET_SIZE_DHCP = NET_SIZE_UDP + sizeof(DhcpHeader);
const int NET_SIZE_ICMP = NET_SIZE_IPV4 + sizeof(IcmpHeader);

void net_packet_eth(
    uint8_t *buffer, uint16_t ether_type,
    uint8_t sender_mac[6], uint8_t target_mac[6]
) {
  EthHeader *eth = (void *)buffer;
  if (target_mac) memcpy(eth->destination_mac, target_mac, 6);
  memcpy(eth->source_mac, sender_mac, 6);
  eth->ether_type = bswap16(ether_type);
}

void net_packet_arp(
    uint8_t *buffer, uint8_t sender_mac[6], uint32_t sender_ip, uint32_t target_ip
) { 
  net_packet_eth(buffer, 0x806, sender_mac, BROADCAST_MAC);

  ArpHeader *arp = (void *)(buffer + sizeof(EthHeader));
  *arp = (ArpHeader){
    .htype = bswap16(1),
    .ptype = bswap16(0x800),
    .hlen = 4,
    .plen = 6,
    .opcode = bswap16(1),
    .target_mac = {0},
    .sender_ip = bswap32(CLIENT_IP),
    .target_ip = bswap32(SERVER_IP),
  };
  memcpy(arp->sender_mac, sender_mac, 6);
}

void net_packet_ipv4(
    uint8_t *buffer, NetCon sender, NetCon target, uint8_t protocol, uint16_t packet_len
) {
  ASSERT(sender.mac);
  net_packet_eth(buffer, 0x800, sender.mac, target.mac);

  Ipv4Header *ip = (void *)(buffer + sizeof(EthHeader));

  *ip = (Ipv4Header){
    .version_ihl = 0x45,
    .dscp_ecn = 0,
    .total_length = bswap16(sizeof(*ip) + packet_len),
    .identification = 0,
    .flags_fragment_offset = bswap16(0x4000),
    .ttl = 255,
    .protocol = protocol,
    .source_addr = bswap32(sender.ip),
    .destination_addr = bswap32(target.ip),
    .header_checksum = 0,
  };

  // Calculate checksum
  uint16_t *data = (void *)ip;
  uint32_t sum = 0;
  for (uint32_t i = 0; i < 10; ++i) sum += data[i];
  while(sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  ip->header_checksum = ~sum;
}

void net_packet_icmp(
    uint8_t *buffer, NetCon sender, NetCon target
) {
  IcmpHeader *icmp = (void *)(buffer + NET_SIZE_IPV4);

  net_packet_ipv4(buffer, sender, target, 1, sizeof(IcmpHeader));

  *icmp = (IcmpHeader){
    .type = 8,
    .code = 0,
    .checksum = 0,
    .id = bswap16(0x742),
    .sequence = 0,
  };
  // Calculate checksum
  uint16_t *data = (void *)icmp;
  uint16_t sum = 0;
  for (uint32_t i = 0; i < sizeof(*icmp)/2; ++i) sum += data[i];
  // while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  icmp->checksum = ~sum;
}

void net_packet_udp(
    uint8_t *buffer, NetCon sender, NetCon target, uint16_t packet_len
) {
  UdpHeader *udp = (void *)(buffer + NET_SIZE_IPV4);
  uint16_t len = sizeof(*udp) + packet_len;
  net_packet_ipv4(buffer, sender, target, 17, len);

  *udp = (UdpHeader){
    .source_port = bswap16(sender.port),
    .destination_port = bswap16(target.port),
    .length = bswap16(len),
    .checksum = 0, // optional for udp?
  };

}

void net_packet_dhcp_discover(uint8_t *buffer, uint8_t sender_mac[6]) {
  NetCon sender = { sender_mac, 0, 68 };
  NetCon target = { 0, BROADCAT_IP, 67 };
  net_packet_udp(buffer, sender, target, sizeof(DhcpHeader));

  // DHCP discover
  DhcpHeader *dhcp = (void *)(buffer + NET_SIZE_UDP);
  *dhcp = (DhcpHeader){
    .operation = 1,
    .hardware_type = 1,
    .hardware_length = 6,
    .hops = 0,
    .seconds_elapsed = 0,
    .flags = bswap16(0x8000), // broadcast
    .transaction_id = bswap32(0x287), // should be random
  };
  memcpy(dhcp->chaddr, sender_mac, 6);

  dhcp->options[0] = 0x63; // magic number
  dhcp->options[1] = 0x82;
  dhcp->options[2] = 0x53;
  dhcp->options[3] = 0x63;

  dhcp->options[4] = 53; // message type
  dhcp->options[5] = 1;
  dhcp->options[6] = 1; // discover
  dhcp->options[7] = 255; // end
}

void net_packet_dhcp_request(uint8_t *buffer, NetCon sender, NetCon dhcp_server) {
  net_packet_udp(buffer, 
      (NetCon){ sender.mac, 0, 68},
      (NetCon){ 0, BROADCAT_IP, 67 },
      sizeof(DhcpHeader)
  );

  DhcpHeader *dhcp = (void *)(buffer + NET_SIZE_UDP);
  *dhcp = (DhcpHeader){
    .operation = 1,
    .hardware_type = 1,
    .hardware_length = 6,
    .hops = 0,
    .seconds_elapsed = 0,
    .flags = bswap16(0x8000), // broadcast
    .transaction_id = bswap32(0x287), // should be random
  };
  memcpy(dhcp->chaddr, sender.mac, 6);

  dhcp->options[0] = 0x63; // magic number
  dhcp->options[1] = 0x82;
  dhcp->options[2] = 0x53;
  dhcp->options[3] = 0x63;

  dhcp->options[4] = 53; // message type
  dhcp->options[5] = 1;
  dhcp->options[6] = 3; // request

  dhcp->options[7] = 50;
  dhcp->options[8] = 4;
  uint32_t sender_ip = bswap32(sender.ip);
  memcpy(&dhcp->options[9], &sender_ip, 4);

  dhcp->options[13] = 54;
  dhcp->options[14] = 4;
  uint32_t dhcp_server_ip = bswap32(dhcp_server.ip);
  memcpy(&dhcp->options[15], &dhcp_server_ip, 4);
  dhcp->options[19] = 255;
}

void net_handle_dhcp_offer(uint8_t *buffer, NetCon *sender, NetCon *dhcp_server) {
  EthHeader *eth = (void *)buffer;
  ASSERT(bswap16(eth->ether_type) == 0x800);

  Ipv4Header *ip = (void *)eth->data;
  ASSERT(bswap16(ip->protocol == 17));

  UdpHeader *udp = (void *)ip->data;
  ASSERT(bswap16(udp->length) == sizeof(UdpHeader) + sizeof(DhcpHeader));

  DhcpHeader *dhcp = (void *)udp->data;
  ASSERT(bswap32(dhcp->transaction_id) == 0x287);
  sender->ip = bswap32(dhcp->offered_ip);
  if (dhcp_server->mac) memcpy(dhcp_server->mac, dhcp->chaddr, 6);

  uint8_t *opts = dhcp->options;
  ASSERT(opts[0] = 0x63); // magic number
  ASSERT(opts[1] = 0x82);
  ASSERT(opts[2] = 0x53);
  ASSERT(opts[3] = 0x63);

  // TODO: maybe parse options instead of hard coding it
  ASSERT(opts[4] == 53);
  ASSERT(opts[5] == 1);
  ASSERT(opts[6] == 2); // dhcp offer

  ASSERT(opts[7] == 54); // server ip
  ASSERT(opts[8] == 4);

  uint32_t server_ip = IP(opts[9], opts[10], opts[11], opts[12]);
  dhcp_server->ip = server_ip;
}

void net_handle_dhcp_ack(uint8_t *buffer, NetCon *sender, NetCon *dhcp_server) {
  EthHeader *eth = (void *)buffer;
  ASSERT(bswap16(eth->ether_type) == 0x800);

  Ipv4Header *ip = (void *)eth->data;
  ASSERT(bswap16(ip->protocol == 17));

  UdpHeader *udp = (void *)ip->data;
  ASSERT(bswap16(udp->length) == sizeof(UdpHeader) + sizeof(DhcpHeader));

  DhcpHeader *dhcp = (void *)udp->data;
  ASSERT(bswap32(dhcp->transaction_id) == 0x287);
  ASSERT(sender->ip == bswap32(dhcp->offered_ip));

  uint8_t *opts = dhcp->options;
  ASSERT(opts[0] = 0x63); // magic number
  ASSERT(opts[1] = 0x82);
  ASSERT(opts[2] = 0x53);
  ASSERT(opts[3] = 0x63);

  ASSERT(opts[4] == 53);
  ASSERT(opts[5] == 1);
  ASSERT(opts[6] == 5); // dhcp ack

  ASSERT(opts[7] == 54); // server ip
  ASSERT(opts[8] == 4);

  uint32_t server_ip = IP(opts[9], opts[10], opts[11], opts[12]);
  ASSERT(server_ip == dhcp_server->ip);
}

void test_networking(VirtioNetdev *netdev) {
  uint8_t *buffer = (void*)alloc_pages(1);

  net_packet_dhcp_discover(buffer, netdev->mac);
  virtio_net_send(netdev, buffer, NET_SIZE_DHCP);

  uint8_t dhcp_mac[6];
  NetCon dhcp_server = { dhcp_mac };
  NetCon sender = { netdev->mac };

  {
    uint32_t index = virtio_net_recv(netdev);
    VirtioNetHeader *header = (void *)(netdev->buffers + 2048 * index);
    net_handle_dhcp_offer((void *)header->packet, &sender, &dhcp_server);
    ASSERT(sender.ip == CLIENT_IP);
    ASSERT(dhcp_server.ip == SERVER_IP);
    virtio_net_return_buffer(netdev, index);
  }

  net_packet_dhcp_request(buffer, sender, dhcp_server);
  virtio_net_send(netdev, buffer, NET_SIZE_DHCP);

  {
    uint32_t index = virtio_net_recv(netdev);
    VirtioNetHeader *header = (void *)(netdev->buffers + 2048 * index);
    net_handle_dhcp_ack((void *)header->packet, &sender, &dhcp_server);
    virtio_net_return_buffer(netdev, index);
  }

  {
    uint32_t index = virtio_net_recv(netdev);
    printf("finished\n");
    virtio_net_return_buffer(netdev, index);
  }
}
