#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "mmio.h"
#include "system.h"
#include "devices/x86/vga/vga.h"
#include "log.h"

#define NUM_PLANES 4

static uint8 readb(struct mmio_device *pDev,uint64 address) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;

  address -= pDev->base;

  //LOG("addr=0x%x",address);

  return *(uint8*)( &pVGA->pVRAM[0][address] );

  return 0xFF;
}

static uint16 readw(struct mmio_device *pDev,uint64 address) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;

  address -= pDev->base;

  return *(uint16*)( &pVGA->pVRAM[0][address] );

  return 0xFFFF;
}

static uint32 readl(struct mmio_device *pDev,uint64 address) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;

  address -= pDev->base;

  return *(uint32*)( &pVGA->pVRAM[0][address] );

  address -= pDev->base;

  LOG("addr=0x%x",address);

  return 0xFFFFFFFF;
}

static uint64 readq(struct mmio_device *pDev,uint64 address) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;

  address -= pDev->base;

  return *(uint64*)( &pVGA->pVRAM[0][address] );
}

void   writeb(struct mmio_device *pDev,uint64 address,uint8 val) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;
  int p;

  for(p=0;p<NUM_PLANES;p++) {
    if( pVGA->seq_reg[2] & (1<<p) )
      *(uint8*)( &pVGA->pVRAM[p][address-pDev->base] ) = val;
  }
}

void   writew(struct mmio_device *pDev,uint64 address,uint16 val) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;
  int p;

  for(p=0;p<NUM_PLANES;p++) {
    if( pVGA->seq_reg[2] & (1<<p) )
      *(uint16*)( &pVGA->pVRAM[p][address-pDev->base] ) = val;
  }
}

void   writel(struct mmio_device *pDev,uint64 address,uint32 val) {
  vgactx_t *pVGA = (vgactx_t*)pDev->opaque;
  int p;

  for(p=0;p<NUM_PLANES;p++) {
    if( pVGA->seq_reg[2] & (1<<p) )
      *(uint32*)( &pVGA->pVRAM[p][address-pDev->base] ) = val;
  }
}

void   writeq(struct mmio_device *pDev,uint64 address,uint64 val) {
  //vgactx_t *pVGA = (vgactx_t*)pDev->opaque;
  //int p;

#if 0
  for(p=0;p<NUM_PLANES;p++) {
    if( pVGA->seq_reg[2] & (1<<p) )
      *(uint64*)( &pVGA->pVRAM[p][address-pDev->base] ) = val;
  }
#endif
}

#if 1
void dump_vga_ram(void) {
  FILE *out;
  int i,c;
  
  out = fopen("text.dump","wb");
  
  c = 0;
  for(i=0;i<0xFFFF;i++) {
    if( c == 80 ) {
      fputc( '\n', out );
      c = 0;
    }
    if( (i & 1) == 0 ) {
      //fputc( ram.text[i], out );
      c++;
    }
  }
  fclose(out);

  LOG("DUMP!");

  exit(0);
}
#endif

int x86_vga_ram_init(sys_t *pCtx,vgactx_t *pVGA) {
  static mmio_device_t gfxram;

  pVGA->pVRAM[0] = (uint8*)malloc(0x20000);
  pVGA->pVRAM[1] = (uint8*)malloc(0x20000);
  pVGA->pVRAM[2] = (uint8*)malloc(0x20000);
  pVGA->pVRAM[3] = (uint8*)malloc(0x20000);
  pVGA->pVRAM[4] = (uint8*)malloc(0x20000);
  pVGA->pVRAM[5] = (uint8*)malloc(0x20000);

  gfxram.opaque = pVGA;
  gfxram.base   = 0xA0000;
  gfxram.mask   = ~0x1FFFF;
  gfxram.readb  = readb;
  gfxram.readw  = readw;
  gfxram.readl  = readl;
  gfxram.readq  = readq;
  gfxram.writeb = writeb;
  gfxram.writew = writew;
  gfxram.writel = writel;
  gfxram.writeq = writeq;
  
  mmio_register_device(&gfxram);

  //ASSERT(0,"YO!");

  //signal(SIGINT,dump_vga_ram);

  return 0;
}

