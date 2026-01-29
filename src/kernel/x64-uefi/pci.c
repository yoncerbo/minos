#include "cmn/lib.h"
#include "arch.h"

enum {
  PCI_PORT_CONFIG = 0xCF8,
  PCI_PORT_DATA = 0xCFC,
};

enum {
  PCI_CONFIG_VENDOR_ID = 0,
  PCI_CONFIG_DEVICE_ID = 2,
  PCI_CONFIG_COMMAND = 0x04,
  PCI_CONFIG_STATUS = 0x06,
  PCI_CONFIG_REVISION_ID = 0x08,
  PCI_CONFIG_PROG_IF = 0x09,
  PCI_CONFIG_SUBCLASS = 0x0a,
  PCI_CONFIG_CLASS_CODE = 0x0b,
  PCI_CONFIG_CACHELINE_SIZE = 0x0c,
  PCI_CONFIG_LATENCY = 0x0d,
  PCI_CONFIG_HEADER_TYPE = 0x0e,
  PCI_CONFIG_BIST = 0x0f,
};

enum {
  PCI_DEV_CONFIG_BAR0 = 0x10,
};

uint32_t read_pci_register32(uint32_t id, uint32_t register_offset) {
  uint32_t addr = 0x80000000 | id | (register_offset & 0xFC);
  ASM("out %0, %1" :: "d"(PCI_PORT_CONFIG), "a"(addr));
  uint32_t value;
  ASM("in %0, %1" : "=a"(value) : "d"(PCI_PORT_DATA));
  return value;
}

uint16_t read_pci_register16(uint32_t id, uint32_t register_offset) {
  uint32_t addr = 0x80000000 | id | (register_offset & 0xFC);
  ASM("out %0, %1" :: "d"(PCI_PORT_CONFIG), "a"(addr));
  uint16_t value;
  ASM("in %0, %1" : "=a"(value) : "d"(PCI_PORT_DATA + (register_offset & 0x2)));
  return value;
}

uint8_t read_pci_register8(uint32_t id, uint32_t register_offset) {
  uint32_t addr = 0x80000000 | id | (register_offset & 0xFC);
  ASM("out %0, %1" :: "d"(PCI_PORT_CONFIG), "a"(addr));
  uint8_t value;
  ASM("in %0, %1" : "=a"(value) : "d"(PCI_PORT_DATA + (register_offset & 0x3)));
  return value;
}

uint32_t pack_pci_id(uint8_t bus, uint8_t device, uint8_t function) {
  return (bus << 16) | (device << 11) | (function << 8);
}

// NOTE: Site to lookup vendor id and device id
// SOURCE: https://devicehunt.com/all-pci-vendors
// TODO: Check in the efi to see if the PCI bus protocol exists
// TODO: Switch to PCI express and MMIO

void check_pci_device(uint8_t bus, uint8_t device, MemoryManager *mm) {
  uint32_t id = pack_pci_id(bus, device, 0);
  uint16_t vendor_id = read_pci_register16(id, PCI_CONFIG_VENDOR_ID);
  if (vendor_id == 0xFFFF) return;
  uint8_t header_type = read_pci_register8(id, PCI_CONFIG_HEADER_TYPE);

  enum {
    PCI_TYPE_MULTIFUNC = 0x80,
  };

  uint32_t func_count = header_type & PCI_TYPE_MULTIFUNC ? 8 : 1;
  for (uint32_t func = 0; func < func_count; ++func) {
    uint32_t id = pack_pci_id(bus, device, func);

    uint16_t vendor_id = read_pci_register16(id, PCI_CONFIG_VENDOR_ID);
    if (vendor_id == 0xFFFF) return;

    uint16_t device_id = read_pci_register16(id, PCI_CONFIG_DEVICE_ID);

    uint8_t class = read_pci_register8(id, PCI_CONFIG_CLASS_CODE);
    uint8_t subclass = read_pci_register8(id, PCI_CONFIG_SUBCLASS);
    uint8_t prog_if = read_pci_register8(id, PCI_CONFIG_PROG_IF);

    log("pci %d:%d:%d class=%X:%X:%X vendor=%X, device=%X",
        bus, device, func,
        class, subclass, prog_if,
        vendor_id, device_id);

    uint8_t header_type = read_pci_register8(id, PCI_CONFIG_HEADER_TYPE);

    uint8_t device_type = header_type & (PCI_TYPE_MULTIFUNC - 1);
    if (device_type != 0) continue;

    if (vendor_id == 0x1AF4 && device_id == 0x1052) {
      log("virtio input");
      size_t cap_offset = read_pci_register32(id, 0x34);

      while (cap_offset) {
        struct PACKED {
          uint8_t vendor, next_offset, cap_len, config_type;
          uint8_t bar, padding[3];
          uint32_t config_offset;
          uint32_t config_len;
        } cap;

        uint32_t *config_ints = (void *)&cap;
        config_ints[0] = read_pci_register32(id, cap_offset + 0);
        config_ints[1] = read_pci_register32(id, cap_offset + 4);
        config_ints[2] = read_pci_register32(id, cap_offset + 8);
        config_ints[3] = read_pci_register32(id, cap_offset + 12);
        ASSERT(cap.bar <= 5);

        enum {
          VIRTIO_PCI_CAP_COMMON_CFG = 1,
        };

        cap_offset = cap.next_offset;
        if (cap.config_type != VIRTIO_PCI_CAP_COMMON_CFG) continue;

        size_t bar = read_pci_register32(id, PCI_DEV_CONFIG_BAR0 + cap.bar * 4);
        // config_addr += cap.config_offset;

        ASSERT(!(bar & 1) && "Expected only memory space BAR");

        uint8_t bar_type = (bar >> 1) & 3;
        ASSERT(bar_type == 2 && "Unsupported BAR type");

        size_t next_bar = read_pci_register32(id, PCI_DEV_CONFIG_BAR0 + (cap.bar + 1) * 4);

        uint64_t config_addr = ((next_bar << 32) | (bar_type & 0xFFFFFFF0)) + cap.config_offset;
        DEBUGX(config_addr);

        // TODO: Rework the interface, use the invalidate instruction
        ASSERT(cap.config_len <= PAGE_SIZE);
        map_virtual_range(mm, config_addr, config_addr, cap.config_len,
            PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE | PAGE_BIT_USER);
        flush_page_table(mm);

        struct {
          uint32_t device_feature_select;
          uint32_t device_feature;
          uint32_t driver_feature_select;
          uint32_t driver_feature;
          uint16_t msix_config;
          uint16_t num_queues;
          uint8_t device_status;
          uint8_t config_generation;

          uint16_t queue_select;
          uint16_t queue_size;
          uint16_t queue_msix_vector;
          uint16_t queue_enable;
          uint16_t queue_notify_off;
          uint64_t queue_desc;
          uint64_t queue_driver;
          uint64_t queue_device;
        } *config = (void *)config_addr;
      }
    }
  }
}

void discover_pci_devices(MemoryManager *mm) {
  for (uint32_t bus = 0; bus < 256; ++bus) {
    for (uint32_t dev = 0; dev < 32; ++dev) {
      check_pci_device(bus, dev, mm);
    }
  }
}

