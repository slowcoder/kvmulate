#include <stdio.h>
#include <string.h>

#include "pci.h"
#include "log.h"
#include "system.h"
#include "devices/x86/vga/vga.h"

static struct {
  uint8 *pROM;
  uint32 uROMLen;
} ctx;

static uint8 rom_readb(ePCIBar bar,int off) {
  ASSERT(bar == ePCIBar_ExpansionROM,"Only expansion-rom supported");

  return *(uint8*)( ctx.pROM + off );
}

static uint16 rom_readw(ePCIBar bar,int off) {
  ASSERT(bar == ePCIBar_ExpansionROM,"Only expansion-rom supported");

  return *(uint16*)( ctx.pROM + off );
}

static uint32 rom_readl(ePCIBar bar,int off) {
  ASSERT(bar == ePCIBar_ExpansionROM,"Only expansion-rom supported");

  return *(uint32*)( ctx.pROM + off );
}

static int vga_rom_init(void) {
  static pcidev_t dev;

  LOG("VGA ROM Init");

  memset(&dev,0,sizeof(dev));

  dev.conf.vendorid    = 0x1234;
  dev.conf.deviceid    = 0x1111;
  dev.conf.commandreg  = 0x0003;
  dev.conf.statusreg   = 0x0280;
  dev.conf.classandrev =
    (0x03 << 24) | // Class Code
    (0x00 << 16) | // Sub-Class code
    (0x00 <<  8) | // Prog-IF
    (0x00 <<  0);  // Revision

  dev.conf.bar[0] = 0;
  dev.conf.bar[1] = 0;
  dev.conf.bar[2] = 0;
  dev.conf.bar[3] = 0;
  dev.conf.bar[4] = 0;
  dev.conf.bar[5] = 0;
  dev.conf.expansionrom = 0xFFFE0000; // 128KB, No address decoding

  dev.initialBARs[ePCIBar_BAR0] = ~0;
  dev.initialBARs[ePCIBar_BAR1] = ~0;
  dev.initialBARs[ePCIBar_BAR2] = ~0;
  dev.initialBARs[ePCIBar_BAR3] = ~0;
  dev.initialBARs[ePCIBar_BAR4] = ~0;
  dev.initialBARs[ePCIBar_BAR5] = ~0;
  dev.initialBARs[ePCIBar_ExpansionROM] = 0xFFFE0000;

  dev.readb = rom_readb;
  dev.readw = rom_readw;
  dev.readl = rom_readl;

  pci_register_dev(&dev);

  return 0;
}

int x86_vga_registers_init(sys_t *pSys,vgactx_t *pVGA);
int x86_vga_ram_init(sys_t *pCtx,vgactx_t *pVGA);

int x86_vga_init(sys_t *pSys) {
  FILE *in;
  vgactx_t *pVGA;

  in = fopen("vgabios.rom","rb");
  ASSERT(in != NULL,"VGA ROM BIOS Not found");
  fseek(in,0,SEEK_END);
  ctx.uROMLen = ftell(in);
  fseek(in,0,SEEK_SET);
  ctx.pROM = (uint8*)malloc(ctx.uROMLen);
  fread(ctx.pROM,1,ctx.uROMLen,in);
  fclose(in);

  vga_rom_init();

  pVGA = (vgactx_t*)calloc(1,sizeof(vgactx_t));
  x86_vga_registers_init(pSys,pVGA);
  x86_vga_ram_init(pSys,pVGA);

  return 0;
}
