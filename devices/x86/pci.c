#include <string.h>

#include "io.h"
#include "pci.h"
#include "log.h"

#define MAX_DEVICES 16

#define ADDRESS_PORT 0xCF8
#define DATA_PORT    0xCFC

typedef struct ctx {
  union {
    uint32 raw;
    struct {
      uint8 zero : 2;
      uint8 doubleword : 6;
      uint8 function : 3;
      uint8 device : 5;
      uint8 bus : 8;
      uint8 reserved : 7;
      uint8 configspacemapping : 1;
    };
  } addr;

  pcidev_t *pCurrDevice;

  pcidev_t *pDevice[MAX_DEVICES];
  int nextDeviceNdx;
} ctx_t;

static ctx_t myctx;
static ctx_t *pCtx = &myctx;

static pcidev_t *getMappedDevice(ctx_t *pCtx) {
  int i;

  for(i=0;i<MAX_DEVICES;i++) {
    if( pCtx->pDevice[i] != NULL ) {
      if( (pCtx->pDevice[i]->bus == pCtx->addr.bus) &&
	  (pCtx->pDevice[i]->dev == pCtx->addr.device) &&
	  (pCtx->pDevice[i]->fn  == pCtx->addr.function) ) {
	//pCtx->pCurrDevice = pCtx->pDevice[i];
	return pCtx->pDevice[i];
      }
    }
  }

  return NULL;
}

static uint8 pci_inb(struct io_handler *hdl,uint16 port) {
  port += hdl->base;

  if( port >= DATA_PORT ) {
    if( pCtx->pCurrDevice == NULL )
      return ~0x00;

    if( (pCtx->addr.doubleword < 9) ||
	(pCtx->addr.doubleword == 15) ) {
      return *((uint8*)( (uint32*)&pCtx->pCurrDevice->conf + pCtx->addr.doubleword ) + port - DATA_PORT);
    }
  }

  ASSERT(0,"Not implemented (port=0x%x,pCurrDev=%p,dw=%i)",port,pCtx->pCurrDevice,pCtx->addr.doubleword);

  return ~0x00;
}
static uint16 pci_inw(struct io_handler *hdl,uint16 port) {
  //ctx_t *pCtx = (ctx_t*)hdl->opaque;

  port += hdl->base;

  if( port == ADDRESS_PORT ) {
    if( pCtx->pCurrDevice == NULL ) {
      return ~0x00;
    } else {
      ASSERT(0,"Not implemented");
    }
  }

  if( pCtx->pCurrDevice == NULL )
    return ~0x00;

  if( pCtx->addr.doubleword < 0x4 ) {
    uint8 *p;

    p = (uint8*)( (uint32*)&pCtx->pCurrDevice->conf + pCtx->addr.doubleword );
    p += port - DATA_PORT;

    return *(uint16*)p;
  } else {

  }
  ASSERT(0,"Not implemented (port=0x%x,pCurrDev=%p,dw=%i)",port,pCtx->pCurrDevice,pCtx->addr.doubleword);
  return ~0x00;
}
static uint32 pci_inl(struct io_handler *hdl,uint16 port) {
  //  ctx_t *pCtx = (ctx_t*)hdl->opaque;

  port += hdl->base;

  if( port == ADDRESS_PORT ) {
    return pCtx->addr.raw;
  }

  if( (pCtx->addr.doubleword <=  9) ||
      (pCtx->addr.doubleword == 12) ||
      (pCtx->addr.doubleword == 15) ) {

    if( pCtx->addr.doubleword == 12) {
      LOG("ExpansionROM returned 0x%x",pCtx->pCurrDevice->conf.expansionrom);
    }

    return *(uint32*)( (uint32*)&pCtx->pCurrDevice->conf + pCtx->addr.doubleword );
  }

  ASSERT(0,"Not implemented (port=0x%x,pCurrDev=%p,dw=%i)",port,pCtx->pCurrDevice,pCtx->addr.doubleword);
  
  return ~0x00;
}
static void pci_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  port += hdl->base;

  if( port == 0xCFB ) {
    ASSERT(val & (1<<7),"PCI IO Transactions not supported");
  }

  ASSERT(0,"Not implemented (port=0x%x,val=0x%x)",port,val);
}
static void pci_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  port += hdl->base;

  if( port == 0xCFA ) {
    ASSERT( val & (1<<15), "PCI IO Transactions not supported");
  }

  if( (port == DATA_PORT) ) {
    if( pCtx->pCurrDevice != NULL ) {
      if( pCtx->addr.doubleword == 1 ) { // Command register
	pCtx->pCurrDevice->conf.commandreg = val;
	return;
      }
    }
  }

  ASSERT(0,"Not implemented (port=0x%x,val=0x%x,pCurrDev=%p,dw=%i)",port,val,pCtx->pCurrDevice,pCtx->addr.doubleword);
}
static void pci_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  //ctx_t *pCtx = (ctx_t*)hdl->opaque;

  port += hdl->base;

  if( port == ADDRESS_PORT ) {
    ASSERT(val & (1<<31),"PCI IO Transactions not supported");

    pCtx->addr.raw = val;
    pCtx->pCurrDevice = getMappedDevice(pCtx);
    return;
  } else {
    if( pCtx->addr.doubleword == 4 ) { /* BAR0 */ return; /*pCtx->pCurrDevice->conf.bar[0];*/ }
    else if( pCtx->addr.doubleword ==  5 ) { /* BAR1 */ return; /*pCtx->pCurrDevice->conf.bar[1];*/ }
    else if( pCtx->addr.doubleword ==  6 ) { /* BAR2 */ return; /*pCtx->pCurrDevice->conf.bar[2];*/ }
    else if( pCtx->addr.doubleword ==  7 ) { /* BAR3 */ return; /*pCtx->pCurrDevice->conf.bar[3];*/ }
    else if( pCtx->addr.doubleword ==  8 ) { /* BAR4 */ return; /*pCtx->pCurrDevice->conf.bar[4];*/ }
    else if( pCtx->addr.doubleword ==  9 ) { /* BAR5 */ return; /*pCtx->pCurrDevice->conf.bar[5];*/ }
    else if( pCtx->addr.doubleword == 12 ) { /* ExpROM */
      if( (val & 0xFFFFF800) == 0xFFFFF800 ) { // Probe
	pCtx->pCurrDevice->conf.expansionrom = pCtx->pCurrDevice->initialBARs[ePCIBar_ExpansionROM];
      } else {
	pCtx->pCurrDevice->conf.expansionrom = val;
	//ASSERT( (val & 1) == 0, "Implement ROM address decoding (val=0x%x)",val);
	/*if( (val & 1) == 1 ) {
	  LOG("Implement ROM address decoding (val=0x%x)",val);
	  }*/
      }
      LOG("New ExpansionROM Value=0x%x (val was 0x%x)",pCtx->pCurrDevice->conf.expansionrom, val);
      return; 
      /*pCtx->pCurrDevice->conf.expansionrom;*/
    }
  }

  ASSERT(0,"Not implemented (port=0x%x,val=0x%x,pCurrDev=%p,dw=%i)",port,val,pCtx->pCurrDevice,pCtx->addr.doubleword);
}

int x86_pci_init(void) {
  static struct io_handler hdl;

  memset(&myctx,0,sizeof(myctx));

  memset(&hdl,0,sizeof(hdl));
  hdl.base   = 0x0CF8;
  hdl.mask   = ~7;
  hdl.inb    = pci_inb;
  hdl.inw    = pci_inw;
  hdl.inl    = pci_inl;
  hdl.outb   = pci_outb;
  hdl.outw   = pci_outw;
  hdl.outl   = pci_outl;
  hdl.opaque = NULL;
  io_register_handler(&hdl);

  return 0;
}

int pci_register_dev(const pcidev_t *dev) {

  int i;

  for(i=0;i<MAX_DEVICES;i++) {
    if( pCtx->pDevice[i] == NULL ) {
      pCtx->pDevice[i] = (pcidev_t*)dev;

      pCtx->pDevice[i]->bus = 0;
      pCtx->pDevice[i]->dev = pCtx->nextDeviceNdx++;
      pCtx->pDevice[i]->fn  = 0;

      return 0;
    }
  }

  return -1;
}

static ePCIBar getBAR(pcidev_t *pDev,uint64 addr) {
  if( (addr >= (pDev->conf.expansionrom & ~1)) &&
      (addr <  ((pDev->conf.expansionrom & ~1) + ~pDev->initialBARs[ePCIBar_ExpansionROM])) )
    return ePCIBar_ExpansionROM;
  else if( (addr >= (pDev->conf.bar[0] & ~1)) &&
	   (addr <  (pDev->conf.bar[0] + ~(pDev->initialBARs[ePCIBar_BAR0] & ~1))) ) {
    return ePCIBar_BAR0;
  } else if( (addr >= (pDev->conf.bar[1] & ~1)) &&
	   (addr <  ((pDev->conf.bar[1] & ~1) + ~pDev->initialBARs[ePCIBar_BAR1])) )
    return ePCIBar_BAR1;
  else if( (addr >= (pDev->conf.bar[2] & ~1)) &&
	   (addr <  ((pDev->conf.bar[2] & ~1) + ~pDev->initialBARs[ePCIBar_BAR2])) )
    return ePCIBar_BAR2;
  else if( (addr >= (pDev->conf.bar[3] & ~1)) &&
	   (addr <  ((pDev->conf.bar[3] & ~1) + ~pDev->initialBARs[ePCIBar_BAR3])) )
    return ePCIBar_BAR3;
  else if( (addr >= (pDev->conf.bar[4] & ~1)) &&
	   (addr <  ((pDev->conf.bar[4] & ~1) + ~pDev->initialBARs[ePCIBar_BAR4])) )
    return ePCIBar_BAR4;
  else if( (addr >= (pDev->conf.bar[5] & ~1)) &&
	   (addr <  ((pDev->conf.bar[5] & ~1) + ~pDev->initialBARs[ePCIBar_BAR5])) )
    return ePCIBar_BAR5;

  return ePCIBar_Invalid;
}

static int handle_mmio(struct kvm_run *pStat,pcidev_t *pDev) {
  uint32 off;
  ePCIBar bar;

  bar = getBAR(pDev,pStat->mmio.phys_addr);

  if( bar == ePCIBar_Invalid )
    return -1;

  if( bar != 7 ) {
    int i;
    LOG("BAR = %i",bar);
    for(i=0;i<=5;i++) {
      LOG("pDev->conf.bar[%i] = 0x%8x / 0x%8x",i,pDev->conf.bar[i],pDev->initialBARs[i+1]);
    }
    LOG("pDev->conf.expansionrom = 0x%8x / 0x%8x",pDev->conf.expansionrom,pDev->initialBARs[ePCIBar_ExpansionROM]);
    LOG("pStat->mmio.phys_addr = 0x%8x",pStat->mmio.phys_addr);
    //LOG("pDev->initialBARs[ePCIBar_ExpansionROM] = 0x%x",
    LOG("Is Write: %i, MMIO Len: %i",pStat->mmio.is_write,pStat->mmio.len);
  }

  if( bar == ePCIBar_ExpansionROM )
    off = pStat->mmio.phys_addr - (pDev->conf.expansionrom & ~1);
  else
    off = pStat->mmio.phys_addr - (pDev->conf.bar[bar-1]);

  if( pStat->mmio.is_write && (pStat->mmio.len == 1) ) {
      ASSERT(pDev->writeb != NULL,"Implement writeb handler");
      pDev->writeb(bar,off,*(uint8*)pStat->mmio.data);
      return 0;
  } else if( !pStat->mmio.is_write && (pStat->mmio.len == 1) ) {
      ASSERT(pDev->readb != NULL,"Implement readb handler");
      *(uint8*)pStat->mmio.data = pDev->readb(bar,off);
      return 0;
  } else if( !pStat->mmio.is_write && (pStat->mmio.len == 2) ) {
      ASSERT(pDev->readw != NULL,"Implement readw handler");
      *(uint16*)pStat->mmio.data = pDev->readw(bar,off);
      return 0;
  } else if( !pStat->mmio.is_write && (pStat->mmio.len == 4) ) {
      ASSERT(pDev->readl != NULL,"Implement readl handler");
      *(uint32*)pStat->mmio.data = pDev->readl(bar,off);
      return 0;
  } else {
    ASSERT(0,"Implement more (phys_addr=0x%x, is_write=%i, len=%i)",pStat->mmio.phys_addr,pStat->mmio.is_write,pStat->mmio.len);
  }    

#if 0
#if 1
  LOG("HIT!");
  LOG("pDev->conf.expansionrom = 0x%x",pDev->conf.expansionrom);
  LOG("pStat->mmio.phys_addr = 0x%x",pStat->mmio.phys_addr);
  LOG("pDev->initialBARs[ePCIBar_ExpansionROM] = 0x%x",pDev->initialBARs[ePCIBar_ExpansionROM]);
  LOG("Is Write: %i, MMIO Len: %i",pStat->mmio.is_write,pStat->mmio.len);
#endif

  if( (pStat->mmio.phys_addr >= (pDev->conf.expansionrom & ~1)) &&
      (pStat->mmio.phys_addr <  ((pDev->conf.expansionrom & ~1) + ~pDev->initialBARs[ePCIBar_ExpansionROM])) ) {
    off = pStat->mmio.phys_addr - (pDev->conf.expansionrom & ~1);

    if( pStat->mmio.is_write && (pStat->mmio.len == 1) ) {
      ASSERT(pDev->writeb != NULL,"Implement writeb handler");
      pDev->writeb(ePCIBar_ExpansionROM,off,*(uint8*)pStat->mmio.data);
      return 0;
    } else if( pStat->mmio.is_write && (pStat->mmio.len == 2) ) {
      ASSERT(pDev->writew != NULL,"Implement writew handler");
      pDev->writew(ePCIBar_ExpansionROM,off,*(uint8*)pStat->mmio.data);
      return 0;
    } else if( !pStat->mmio.is_write && (pStat->mmio.len == 1) ) {
      ASSERT(pDev->readb != NULL,"Implement readw handler");
      *(uint8*)pStat->mmio.data = pDev->readb(ePCIBar_ExpansionROM,off);
      return 0;
    } else if( !pStat->mmio.is_write && (pStat->mmio.len == 2) ) {
      ASSERT(pDev->readw != NULL,"Implement readw handler");
      *(uint16*)pStat->mmio.data = pDev->readw(ePCIBar_ExpansionROM,off);
      return 0;
    } else if( !pStat->mmio.is_write && (pStat->mmio.len == 4) ) {
      ASSERT(pDev->readl != NULL,"Implement readw handler");
      *(uint32*)pStat->mmio.data = pDev->readl(ePCIBar_ExpansionROM,off);
      return 0;
    } else {
      ASSERT(0,"Implement more (is_write=%i, len=%i)",pStat->mmio.is_write,pStat->mmio.len);
    }
  } else {
    ASSERT(0,"Wasn't an expansionROM access");
  }


#if 0
  if( ((pDev->conf.expansionrom &~1) & (pStat->mmio.phys_addr & pDev->initialBARs[ePCIBar_ExpansionROM])) ==
      pDev->conf.expansionrom ) {
    ASSERT(0,"SUCCESS!!");
  }
#endif
  return -1;
#if 0
  if( pStat->mmio.is_write ) {
    
  } else { /* Read */
  }
#endif
#endif
}

static int handle_io(struct kvm_run *pStat,pcidev_t *pDev) {
  //ASSERT(0,"Implement me");
  return -1;
}

int handle_pci(struct kvm_run *pStat) {
  int i;

  for(i=0;i<MAX_DEVICES;i++) {
    if( pCtx->pDevice[i] != NULL ) {
      if( pStat->exit_reason == KVM_EXIT_MMIO )
	if( handle_mmio(pStat,pCtx->pDevice[i]) == 0 )
	  return 0;
      if( pStat->exit_reason == KVM_EXIT_IO )
	if( handle_io(pStat,pCtx->pDevice[i]) == 0 )
	  return 0;
    }
  }

  return -1;
}

#if 0
// Maximum functions (devs*functions) per bus, and 
// maximum number of busses
#define MAX_FPB (8*32)
#define MAX_BUS 1

static struct {
  int configspace;
  uint8 bus,dev,fn,index;
} state;

static int devs = 0;
static pcidev_t *dev[8*32];

static pcidev_t *getDevice(void) {
  int i;

  for(i=0;i<MAX_FPB;i++) {
    if( dev[i] != NULL ) {
      //LOG(" IsTrue?  %i==%i %i==%i %i==0",dev[i]->bus,state.bus,dev[i]->dev,state.dev,dev[i]->fn);
      if( (dev[i]->bus == state.bus) &&
	  (dev[i]->dev == state.dev) &&
	  (dev[i]->fn  == state.fn) ) {
	//LOG(" Match. Returning %p",dev[i]);
	return dev[i];
      }
    }
  }

  return NULL;
}

static uint8 pci_inb(struct io_handler *hdl,uint16 port) {
  pcidev_t *dev;

  //LOG(" readb. Port=%u, Index=0x%x",port,index);

  if( port >= 0x4 ) {
    dev = getDevice();
    if( dev == NULL ) {
      //LOG("Read from non-existant PCI device");
      return 0xFF;
    }
    return dev->readb(state.index + port - 0x4);
  }
  ASSERT(0,"Unimplemented inb from 0x%04x",hdl->base + port);
  return 0xFF;
}
static uint16 pci_inw(struct io_handler *hdl,uint16 port) {
  pcidev_t *dev;

  if( port >= 0x4 ) {
    dev = getDevice();
    if( dev == NULL ) {
      //LOG("Read from non-existant PCI device");
      return 0xFFFF;
    }
    return dev->readw(state.index + port - 0x4);
  }
  ASSERT(0,"Unimplemented inw from 0x%04x",hdl->base + port);
}
static uint32 pci_inl(struct io_handler *hdl,uint16 port) {
  //ASSERT(0,"Unimplemented read from 0x%04",hdl->base + port);
  if(port == 0) {
    uint32 ret;

    ret  = state.configspace << 31;
    ret |= (state.bus & 0x0F) << 16;
    ret |= (state.dev & 0x1F) << 11;
    ret |= state.index & 0xFF;

    LOG("PCI Conf Readback (cs=0x%x)",state.configspace);
    return ret;
  } else {
    pcidev_t *dev;

    if( port >= 0x4 ) {
      dev = getDevice();
      if( dev == NULL ) {
	//LOG("Read from non-existant PCI device");
	return 0xFFFF;
      }
      return dev->readl(state.index + port - 0x4);
    }
    ASSERT(0,"Unimplemented inl from 0x%04x",hdl->base + port);
  }
  return 0xFFFFFFFF;
}

static void  pci_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  if( port <= 3 ) { // Byte access to config reg?
    if( port == 3 ) {
      state.index = (state.index & ~0xFF) | val;
    } else {
      ASSERT(0,"Unimplemented write to 0x%04x (stateNdx=0x%04x,val=0x%02x)",hdl->base + port,state.index,val);
    }
  } else {
    pcidev_t *dev;

    dev = getDevice();
    if( dev == NULL ) {
      //LOG("Write to non-existant PCI device");
      return;
    }
    //ASSERT(0,"OMG!");
    dev->writeb(state.index + port - 0x4,val);
    return;
  }
}

static void  pci_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  if( port <= 3 ) { // Byte access to config reg?
    ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
  } else {
    pcidev_t *dev;

    dev = getDevice();
    if( dev == NULL ) {
      //LOG("Write to non-existant PCI device");
      return;
    }
    //ASSERT(0,"OMG!");
    ASSERT(dev->writew != NULL,"NULL writew handler for (port=%i, val=0x%x, ndx=0x%x)",port,val,state.index);
    dev->writew(state.index + port - 0x4,val);
    return;
  }
}

static void  pci_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  //LOG("PCI Type1: Port=0x%x, Val=0x%x",port,val);
  if( port == 0 ) {
    state.configspace = val>>31;
    state.index = val & 0xFF;
    state.bus   = (val >> 16) & 0x0F;
    state.dev   = (val >> 11) & 0x1F;
    state.fn    = (val >>  8) & 0x07;
    //ASSERT(state.configspace == 1,"Configuration space not implemented (val=0x%x)",val);

  } else {
    pcidev_t *dev;

    ASSERT(state.configspace == 1,
	   "Configuration space not implemented (cs=0x%x)",
	   state.configspace);

    dev = getDevice();
    if( dev == NULL ) {
      LOG("Write to non-existant PCI device");
      return;
    }
    return dev->writel(state.index + port - 0x4,val);
    //ASSERT(0,"Write to PCI device not supported (port=%i, val=0x%x, ndx=0x%x)",port,val,state.index);
  }
}

int x86_pci_init(void) {
  static struct io_handler hdl;

  memset(&state,0,sizeof(state));
  memset(dev,0,sizeof(pcidev_t*)*MAX_FPB);
  devs = 0;

  hdl.base   = 0x0CF8;
  //hdl.mask   = 0xFFFB;
  hdl.mask   = ~7;
  hdl.inb    = pci_inb;
  hdl.inw    = pci_inw;
  hdl.inl    = pci_inl;
  hdl.outb   = pci_outb;
  hdl.outw   = pci_outw;
  hdl.outl   = pci_outl;
  hdl.opaque = NULL;
  io_register_handler(&hdl);

  return 0;
}

int pci_register_dev(int bus,const pcidev_t *newdev) {
  int i;

  ASSERT(bus == -1,"No allowing set busses right now");
  ASSERT(newdev != NULL,"Registering NULL devices?");

  for(i=0;i<MAX_FPB;i++) {
    if( dev[i] == NULL ) {
      dev[i] = (pcidev_t*)newdev;
      dev[i]->bus = 0;
      dev[i]->dev = devs++;
      dev[i]->fn  = 0;

      LOG("Assigned address B%i, D%i, FN%i",dev[i]->bus,dev[i]->dev,dev[i]->fn);

      return 0;
    }
  }

  return -1;
}
#endif
