/*
 * PS/2 POS (Programmable Option Select)
 */
#include <string.h>

#include "io.h"
#include "log.h"

static struct {
  int A20_en;
} state;

static uint8 ps2pos_inb(struct io_handler *hdl,uint16 port) {
  if( port == 2 ) { // PS/2 System Control Port A
    return (!!state.A20_en) << 1;
  } else {
    ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  }
  
  return 0;
}

static void  ps2pos_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  if( port == 2 ) { // PS/2 System Control Port A
    state.A20_en = (val >> 1) & 1;
    return;
  }

  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

int x86_ps2_POS_init(void) {
  static struct io_handler hdl;

  memset(&state,0,sizeof(state));

  hdl.base = 0x90;
  hdl.mask = ~0x7;
  hdl.inb  = ps2pos_inb;
  hdl.outb = ps2pos_outb;
  io_register_handler(&hdl);

  return 0;
}
