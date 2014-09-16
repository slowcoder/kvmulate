/*
 * Intel 8237 Direct Memory Access Controller
 */
#include <stdlib.h>
#include <string.h>

#include "io.h"
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

typedef struct {
  uint8 regs[7];
  uint8 mode;
  uint8 mask;
} ctx_t;

static uint8 i8237_inb(struct io_handler *hdl,uint16 port) {
  ctx_t *ctx;

  ctx = (ctx_t*)hdl->opaque;

  if( hdl->base != 0x00 ) {
    //LOG("Port mongrification. Was=0x%x Willbe=0x%x",port,port>>1);
    port >>= 1;
  }
  //LOG("Port=0x%x, Base=0x%x",port,hdl->base);
  //port -= hdl->base;

  if( port <= 0x7 ) {
    return ctx->regs[port];
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
  ctx_t *ctx;

  ctx = (ctx_t*)hdl->opaque;

  if( hdl->base != 0x00 ) {
    //LOG("Port mongrification. Was=0x%x Willbe=0x%x",port,port>>1);
    port >>= 1;
  }

  if( port <= 7 ) { // Write-only registers
    ctx->regs[port] = val;
  } else if( port == WR_MASTER_CLEAR ) {
    memset(ctx,0,sizeof(ctx_t));
  } else if( port == WR_MODE_REG ) {
    ctx->mode = val;
  } else if( port == WR_SINGLE_MASK_BIT ) {
    ctx->mask = 0xF0 | (val & 0xF);
  } else if( port == W_FLIPFLOP_RESET ) {
  } else {
    ASSERT(0,"IMPLEMENT ME (port=0x%x)",port);
  }

#if 0
  if( port == WR_MASTER_CLEAR ) {
    memset(ctx,0,sizeof(ctx_t));
  } else if( port == WR_MODE_REG ) {
    ctx->mode = val;
  } else if( port == WR_SINGLE_MASK_BIT ) {
    ctx->mask = 0xF0 | (val & 0xF);
  } else {
    ASSERT(0,"Unimplemented write to 0x%04x (val=0x%02x)",
	   hdl->base + port,val);
  }
#endif
}

int x86_i8237_dma_init(uint16 base) {
  struct io_handler *hdl[1];

  hdl[0] = (struct io_handler*)calloc(1,sizeof(struct io_handler));
  //hdl[1] = (struct io_handler*)calloc(1,sizeof(struct io_handler));

  hdl[0]->base   = base;
  if( base == 0x00)
    hdl[0]->mask   = ~0xF; // 0xF   0 -> 0xF
  else
    hdl[0]->mask   = ~0x1F; // 0x1F   0xC0 -> 0xDF
  hdl[0]->inb    = i8237_inb;
  hdl[0]->outb   = i8237_outb;
  hdl[0]->opaque = calloc(1,sizeof(ctx_t));
  io_register_handler(hdl[0]);

  return 0;
}
