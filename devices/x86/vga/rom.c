#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "system.h"
#include "pci.h"
#include "mmio.h"
#include "log.h"

typedef struct {
  mmio_device_t mmiodev,pcirom;

  int    bIsMapped;
  uint8 *pROMData,*pRAMData;
  uint32 len;
} ctx_t;

static ctx_t ctx;
static pcidev_t dev;

static int load_bios(ctx_t *pCtx) {
  int in;
  int flen;

  in = open("vgabios.rom",O_RDONLY);
  if( in == -1 ) {
    LOG("Failed to open VGA ROM file: %s",strerror(errno));
    return -1;
  }
  flen = lseek(in,0,SEEK_END);
  lseek(in,0,SEEK_SET);

  pCtx->pROMData = calloc(1,0x20000);
  pCtx->len   = flen;
  pCtx->bIsMapped = 0;

  read(in,pCtx->pROMData,flen);
  //LOG("First bytes: 0x%02x 0x%02x",pCtx->pROMData[0],pCtx->pROMData[1]);
  close(in);

  pCtx->pRAMData = calloc(1,0x20000);
  memcpy(pCtx->pRAMData,pCtx->pROMData,0x20000);

  return 0;
}

static void remap_bios(ctx_t *pCtx,uint32 newbase) {
  if( newbase != 0xC0000 ) {
    LOG("Remapping VGA bios to 0x%x",newbase);
    ctx.pcirom.base = newbase;
    pCtx->bIsMapped = 1;
  } else {
    LOG("Refusing to remap PCI expansion bios to 0xC0000");
  }
}

static void vgarom_writeb(int reg,uint8 val) {
  // HACK!
  if( reg != 0x04 ) 
    ASSERT(0,"NOT IMPLEMENTED (reg=0x%x,val=0x%x)",reg,val);
}
static void vgarom_writew(int reg,uint16 val) {
  //ASSERT(0,"NOT IMPLEMENTED (reg=0x%x,val=0x%x)",reg,val);
}
static void vgarom_writel(int reg,uint32 val) {
  uint8 *pConf;

  //LOG("reg=0x%x,val=0x%x",reg,val);

  pConf = ((uint8*)&dev.conf) + reg;
  if( (reg == PCI_CFG_REG_BAR0) ||
      (reg == PCI_CFG_REG_BAR1) ||
      (reg == PCI_CFG_REG_BAR2) ||
      (reg == PCI_CFG_REG_BAR3) ||
      (reg == PCI_CFG_REG_BAR4) ||
      (reg == PCI_CFG_REG_BAR5) ) {
    *pConf = 0;
  } else if( (reg == PCI_CFG_REG_EXPROM) && (val == 0xFFFFF800) ) {
    dev.conf.expansionrom = 0xFFFE0000; // 128KB, No address decoding
  } else if( reg == PCI_CFG_REG_EXPROM ) {
    //ASSERT( 0,"Remapping needed.. (val=0x%x)",val);
    //ASSERT( !(val & 1),"Remapping needed..");
    if( val & 1 ) 
      remap_bios(&ctx,val & ~1);

    *(uint32*)pConf = val;
  } else {
    ASSERT(0,"NOT IMPLEMENTED (reg=0x%x,val=0x%x)",reg,val);
  }
}

static uint8 vgarom_readb(int reg) {
  uint8 *pRet;

  pRet = ((uint8*)&dev.conf) + reg;
  //LOG("Read from reg 0x%x (0x%x) <-------------",reg,*pRet);
  return *pRet;
}
static uint16 vgarom_readw(int reg) {
  uint8 *pRet;

  pRet = ((uint8*)&dev.conf) + reg;
  //LOG("Read from reg 0x%x (0x%x)",reg,*pRet);
  //ASSERT(0,"bah");
  return *(uint16*)pRet;
}
static uint32 vgarom_readl(int reg) {
  uint8 *pRet;

  pRet = ((uint8*)&dev.conf) + reg;
  //LOG("Read from reg 0x%x (0x%x)",reg,*(uint32*)pRet);

  //LOG(" HeaderType=0x%x",dev.conf.headertype);

  return *(uint32*)pRet;
}

static void vgarom_bios_writeb(struct mmio_device *pDev,uint64 address,uint8 val) {
  address -= pDev->base;

  //LOG("Addres=0x%x (val=0x%x)",address,val);

  if( pDev->base == 0xC0000 ) {
    *(uint8*)( ctx.pRAMData + address ) = val;
  } else {
    LOG(" Avoiding write to PCI Expansion _ROM_");
  }

  //ASSERT(0,"Writing into VGA ROM?  Address=0x%x, Val=0x%x",address,val);
}

static void vgarom_bios_writew(struct mmio_device *pDev,uint64 address,uint16 val) {
  address -= pDev->base;

  //LOG("Addres=0x%x (val=0x%x)",address,val);

  if( pDev->base == 0xC0000 ) {
    *(uint16*)( ctx.pRAMData + address ) = val;
  } else {
    LOG(" Avoiding write to PCI Expansion _ROM_");
  }

  //ASSERT(0,"Writing into VGA ROM?  Address=0x%x, Val=0x%x",address,val);
}

static void vgarom_bios_writel(struct mmio_device *pDev,uint64 address,uint32 val) {
  address -= pDev->base;

  //LOG("Addres=0x%x (Base=0x%x,val=0x%x)",address,pDev->base,val);

  if( pDev->base == 0xC0000 ) {
    *(uint32*)( ctx.pRAMData + address ) = val;
  } else {
    LOG(" Avoiding write to PCI Expansion _ROM_");
  }

  //ASSERT(0,"Writing into VGA ROM?  Address=0x%x, Val=0x%x",address,val);
}

static uint8 vgarom_bios_readb(struct mmio_device *pDev,uint64 address) {
  address -= pDev->base;

  //LOG("Address = 0x%x (base=0x%x)",address,pDev->base);

  if( pDev->base == 0xC0000 ) {
    return *(uint8*)( ctx.pRAMData + address );
  } else {
    return *(uint8*)( ctx.pROMData + address );
  }

  LOG("NOT mapped...");
  return 0xF0;
}

static uint16 vgarom_bios_readw(struct mmio_device *pDev,uint64 address) {
  address -= pDev->base;

  //LOG("Address = 0x%x (base=0x%x)",address,pDev->base);

  if( pDev->base == 0xC0000 ) {
    //LOG(" Returning 0x%x",*(uint16*)( ctx.pRAMData + address ));
    return *(uint16*)( ctx.pRAMData + address );
  } else {
    return *(uint16*)( ctx.pROMData + address );
  }

  LOG("NOT mapped...");
  return 0xF0;
}

static uint32 vgarom_bios_readl(struct mmio_device *pDev,uint64 address) {
  address -= pDev->base;

  //LOG("Address = 0x%x (base=0x%x)",address,pDev->base);

  if( pDev->base == 0xC0000 ) {
    return *(uint32*)( ctx.pRAMData + address );
  } else {
    return *(uint32*)( ctx.pROMData + address );
  }

  LOG("NOT mapped...");
  return 0xF0;
}

int x86_vga_rom_init(sys_t *pCtx) {

  memset(&ctx,0,sizeof(ctx_t));

  load_bios(&ctx);
#if 0
  ctx.mmiodev.base   =  0xC0000;
  ctx.mmiodev.mask   = ~0x1FFFF;
  ctx.mmiodev.writeb =  vgarom_bios_writeb;
  ctx.mmiodev.writew =  vgarom_bios_writew;
  ctx.mmiodev.writel =  vgarom_bios_writel;
  ctx.mmiodev.readb  =  vgarom_bios_readb;
  ctx.mmiodev.readw  =  vgarom_bios_readw;
  ctx.mmiodev.readl  =  vgarom_bios_readl;
  mmio_register_device(&ctx.mmiodev);
#endif
  //ctx.pcirom.base   =  0xC0000;
  ctx.pcirom.base   =  0xFFAA0000;
  ctx.pcirom.mask   = ~0x1FFFF;
  ctx.pcirom.writeb =  vgarom_bios_writeb;
  ctx.pcirom.writew =  vgarom_bios_writew;
  ctx.pcirom.writel =  vgarom_bios_writel;
  ctx.pcirom.readb  =  vgarom_bios_readb;
  ctx.pcirom.readw  =  vgarom_bios_readw;
  ctx.pcirom.readl  =  vgarom_bios_readl;
  mmio_register_device(&ctx.pcirom);

  memset(&dev.conf,0,sizeof(dev.conf));
  dev.conf.vendorid    = 0x1013;
  dev.conf.deviceid    = 0x00b8;

  dev.conf.vendorid    = 0x1234;
  dev.conf.deviceid    = 0x1111;

  //dev.conf.vendorid    = 0x1b36;
  //dev.conf.deviceid    = 0x0100;

  dev.conf.commandreg  = 0x0003;
  dev.conf.statusreg   = 0x0280;
  dev.conf.classandrev = 
    (0x03 << 24) | // Class Code
    (0x00 << 16) | // Sub-Class code
    (0x00 <<  8) | // Prog-IF
    (0x00 <<  0);  // Revision
  dev.conf.headertype   = 0x00;
  dev.conf.expansionrom = 0xC0000;
  //dev.conf.expansionrom = 0xC0000000;

  dev.readb  = vgarom_readb;
  dev.readw  = vgarom_readw;
  dev.readl  = vgarom_readl;
  dev.writeb = vgarom_writeb;
  dev.writew = vgarom_writew;
  dev.writel = vgarom_writel;
  pci_register_dev(-1,&dev);

  ctx.bIsMapped = 0;

  return 0;
}
