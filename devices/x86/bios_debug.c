/*
 * SeaBIOS Debug Port
 */
#include <stdio.h>

//#include "devices/bios_debug.h"
#include "io.h"
#include "log.h"

static uint8 biosdebug_inb(struct io_handler *hdl,uint16 port) {

  ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  
  return 0;
}

static void  biosdebug_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  putchar(val);
}

int x86_biosdebug_init(void) {
  static struct io_handler hdl;

  hdl.base   = 0x402;
  hdl.mask   = 0xFFFF;
  hdl.inb    = biosdebug_inb;
  hdl.outb   = biosdebug_outb;
  hdl.opaque = NULL;
  io_register_handler(&hdl);

  return 0;
}
