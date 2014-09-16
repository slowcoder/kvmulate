#include <string.h>

#include "pci.h"
#include "log.h"
#include "system.h"

static uint8 ata_readb(ePCIBar bar,int off) {
  LOG("YO!");
  return 0xFF;
}

int x86_ata_init(sys_t *pSys) {
  static pcidev_t dev;

  LOG("ATA Init");

  memset(&dev,0,sizeof(dev));

  dev.conf.vendorid    = 0xf00d;
  dev.conf.deviceid    = 0xbabe;
  dev.conf.commandreg  = 0x0003;
  dev.conf.statusreg   = 0x0280;
  dev.conf.classandrev =
    (0x01 << 24) | // Class Code
    (0x01 << 16) | // Sub-Class code
    (((1<<7)+1) <<  8) | // Prog-IF
    (0x00 <<  0);  // Revision

  dev.conf.bar[0] = 0x1F0;
  dev.conf.bar[1] = 0;
  dev.conf.bar[2] = 0;
  dev.conf.bar[3] = 0;
  dev.conf.bar[4] = 0;
  dev.conf.bar[5] = 0;
  dev.conf.expansionrom = 0; // 128KB, No address decoding

  dev.initialBARs[ePCIBar_BAR0] = 0xFFFFFF00 | 1;
  dev.initialBARs[ePCIBar_BAR1] = ~0;
  dev.initialBARs[ePCIBar_BAR2] = ~0;
  dev.initialBARs[ePCIBar_BAR3] = ~0;
  dev.initialBARs[ePCIBar_BAR4] = ~0;
  dev.initialBARs[ePCIBar_BAR5] = ~0;
  dev.initialBARs[ePCIBar_ExpansionROM] = ~0;

  dev.readb = ata_readb;
  //dev.readw = rom_readw;
  //dev.readl = rom_readl;

  pci_register_dev(&dev);

  return 0;
}
