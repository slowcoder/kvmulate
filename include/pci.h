#pragma once

#include "types.h"
#include <linux/kvm.h>

typedef enum {
  ePCIBar_Invalid = 0,
  ePCIBar_BAR0,
  ePCIBar_BAR1,
  ePCIBar_BAR2,
  ePCIBar_BAR3,
  ePCIBar_BAR4,
  ePCIBar_BAR5,
  ePCIBar_ExpansionROM,
  ePCIBar_MAX,
} ePCIBar;

#define PCI_CFG_REG_BAR0   0x10
#define PCI_CFG_REG_BAR1   0x14
#define PCI_CFG_REG_BAR2   0x18
#define PCI_CFG_REG_BAR3   0x1C
#define PCI_CFG_REG_BAR4   0x20
#define PCI_CFG_REG_BAR5   0x24
#define PCI_CFG_REG_EXPROM 0x30

#pragma pack(push,unpack)
#pragma pack(1)
typedef struct {
  uint16 vendorid;
  uint16 deviceid;
  uint16 commandreg;
  uint16 statusreg;
  uint32 classandrev;
  uint8  cachelinesize;
  uint8  latencytimer;
  uint8  headertype;
  uint8  bist;
  uint32 bar[6];
  uint32 cardbus_cis;
  uint16 subsys_vendorid;
  uint16 subsys_id;
  uint32 expansionrom;
  uint32 pack[3];

  uint32 padding[60];
} pcidev_config_t;
#pragma pack(pop)

typedef struct {
  uint8 bus,dev,fn;

  pcidev_config_t conf;
  uint32 initialBARs[ePCIBar_MAX];

  uint8  (*readb)(ePCIBar bar,int off);
  uint16 (*readw)(ePCIBar bar,int off);
  uint32 (*readl)(ePCIBar bar,int off);
  void   (*writeb)(ePCIBar bar,int off,uint8  val);
  void   (*writew)(ePCIBar bar,int off,uint16 val);
  void   (*writel)(ePCIBar bar,int off,uint32 val);
} pcidev_t;

int x86_pci_init(void);
int pci_register_dev(const pcidev_t *dev);
int handle_pci(struct kvm_run *pStat);
