/*
 * Intel 8237 Direct Memory Access Controller
 */
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "system.h"
#include "log.h"

#define WR_START_ADDR_04   0x0
#define WR_COUNT_04        0x1
#define WR_START_ADDR_15   0x2
#define WR_COUNT_15        0x3
#define WR_START_ADDR_26   0x4
#define WR_COUNT_26        0x5
#define WR_START_ADDR_37   0x6
#define WR_COUNT_37        0x7

#define WR_SINGLE_MASK_BIT 0xA
#define WR_MODE_REG        0xB
#define WR_MASTER_CLEAR    0xD
#define WR_ALL_MASK_BITS   0xF
#define R_COMMAND          0x8
#define W_FLIPFLOP_RESET   0xC

static struct {
  struct {
    uint8 regs[7][2];
    int   flipflop[7];
    uint8 mode;
    uint8 mask;
  } ctl[2];
  struct {
    uint8 reg[0xF];
  } page;
} dma;

static uint8 i8237_inb(struct io_handler *hdl,uint16 port) {
  int ctl;

  LOGD("Read from 0%x",hdl->base+port);

  ctl = 0;
  if( hdl->base != 0x00 ) {
    // The second 8237 is off-by-one on the address bus,
    // meaning that there is a "dead" address inbetween
    // each register
    port >>= 1;
    ctl = 1;
  }

  if( port <= 0x7 ) {
    //ASSERT(dma.ctl[ctl].flipflop[port] <= 1,"Flipflop overrun / not reset");
    return dma.ctl[ctl].regs[port][ dma.ctl[ctl].flipflop[port]++ & 1 ];
  } else if( port == R_COMMAND ) {
    return (1<<2); // Disabled DMA
  } else if( port == 0x10 ) {
    return 0;
  } else {
    ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  }

  return 0;
}

static void  i8237_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  int ctl;

  LOGD("Write to 0x%x, val=0x%02x",hdl->base+port,val);

  ctl = 0;
  if( hdl->base != 0x00 ) {
    // The second 8237 is off-by-one on the address bus,
    // meaning that there is a "dead" address inbetween
    // each register
    port >>= 1;
    ctl = 1;
  }

  if( port <= 7 ) { // Write-only registers
    //ASSERT(dma.ctl[ctl].flipflop[port] <= 1,"Flipflop overrun / not reset");

    if( port == WR_START_ADDR_26 )
      LOGD("SA26=0x%02x",val);
    dma.ctl[ctl].regs[port][ dma.ctl[ctl].flipflop[port]++ &1 ] = val;
  } else if( port == WR_MASTER_CLEAR ) {
    memset(&dma.ctl[ctl],0,sizeof(dma.ctl[ctl]));
  } else if( port == WR_MODE_REG ) {
    dma.ctl[ctl].mode = val;
  } else if( port == WR_SINGLE_MASK_BIT ) {
    dma.ctl[ctl].mask = 0xF0 | (val & 0xF);
  } else if( port == W_FLIPFLOP_RESET ) {
    if( val > 0 ) {
      ASSERT(val == 0xFF,"Bitmasked flipflop reset not implemented");
      memset(dma.ctl[ctl].flipflop,0,7);
    }
  } else {
    ASSERT(0,"IMPLEMENT ME (port=0x%x)",port);
  }
}

static void  i8237_page_outb(struct io_handler *hdl,uint16 port,uint8 val) {

  dma.page.reg[port] = val;

  //ASSERT(0,"IMPLEMENT ME (port=0x%x)",port);
}

static uint8 i8237_page_inb(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"IMPLEMENT ME (port=0x%x)",port);
  return 0xFF;
}

void dma_transfer_from_device(int ch,void *pData,unsigned int uDatalen) {
  unsigned int destOff,count;
  unsigned char *pDest;

  ASSERT(ch == 2,"Only channel 2 implemented for now");

  //dest = dma.ctl[0].regs[WR_START_ADDR_26]
  LOGD("Lo=0x%x",dma.ctl[0].regs[WR_START_ADDR_26][0]);
  LOGD("Hi=0x%x",dma.ctl[0].regs[WR_START_ADDR_26][1]);
  LOGD("Pg=0x%x",dma.page.reg[1]);

  destOff  = dma.page.reg[1] << 16;
  destOff |= dma.ctl[0].regs[WR_START_ADDR_26][1] << 8;
  destOff |= dma.ctl[0].regs[WR_START_ADDR_26][0] << 0;
  LOGD("24 bit dest = 0x%x",destOff);

  count  = dma.ctl[0].regs[WR_COUNT_26][1] << 8;
  count |= dma.ctl[0].regs[WR_COUNT_26][0] << 0;
  LOGD("16 bit count = 0x%x",count);

  pDest = sys_getLowMem(destOff);
  LOGD("Dest=%p",pDest);

  memcpy(pDest,pData,count + 1);
  LOGD("DMA Five: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",pDest[0],pDest[1],pDest[2],pDest[3],pDest[4]);
  LOGD("\"Transfered\" %u bytes",count + 1);

  //ASSERT(0,"IMPLEMENT ME !!");
}

static struct io_handler hdl[3];
int x86_i8237_dma_init(uint16 base) {

  hdl[0].base   = 0x00;
  hdl[0].mask   = ~0x0F;
  hdl[0].inb    = i8237_inb;
  hdl[0].outb   = i8237_outb;
  hdl[0].opaque = NULL;
  hdl[0].pzDesc = "DMA Ctrl 0";
  io_register_handler(&hdl[0]);

  hdl[1].base   = 0xC0;
  hdl[1].mask   = ~0x1F;
  hdl[1].inb    = i8237_inb;
  hdl[1].outb   = i8237_outb;
  hdl[1].opaque = NULL;
  hdl[1].pzDesc = "DMA Ctrl 1";
  io_register_handler(&hdl[1]);

  hdl[2].base   = 0x80;
  hdl[2].mask   = ~0x0F;
  hdl[2].inb    = i8237_page_inb;
  hdl[2].outb   = i8237_page_outb;
  hdl[2].opaque = NULL;
  hdl[2].pzDesc = "DMA Page regs";
  io_register_handler(&hdl[2]);

  return 0;
}
