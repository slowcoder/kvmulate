/*
 * Serial port (8250, 16550A, etc)
 */
#include <stdio.h>

#include "io.h"
#include "log.h"

static uint8 serport_inb(struct io_handler *hdl,uint16 port) {
  //ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);

  return 0x00;
}

static uint16 serport_inw(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  return 0x0000;
}
static uint32 serport_inl(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04",hdl->base + port);
  return 0x00000000;
}

static void  serport_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  //ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static void  serport_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static void  serport_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

int x86_serialport_init(void) {
  static struct io_handler hdl[4];

  hdl[0].base   = 0x03F8;
  hdl[0].mask   = 0xFFF8;
  hdl[0].inb    = serport_inb;
  hdl[0].inw    = serport_inw;
  hdl[0].inl    = serport_inl;
  hdl[0].outb   = serport_outb;
  hdl[0].outw   = serport_outw;
  hdl[0].outl   = serport_outl;
  hdl[0].opaque = NULL;
  io_register_handler(&hdl[0]);

  hdl[1].base   = 0x02F8;
  hdl[1].mask   = 0xFFF8;
  hdl[1].inb    = serport_inb;
  hdl[1].inw    = serport_inw;
  hdl[1].inl    = serport_inl;
  hdl[1].outb   = serport_outb;
  hdl[1].outw   = serport_outw;
  hdl[1].outl   = serport_outl;
  hdl[1].opaque = NULL;
  io_register_handler(&hdl[1]);

  hdl[2].base   = 0x03E8;
  hdl[2].mask   = 0xFFF8;
  hdl[2].inb    = serport_inb;
  hdl[2].inw    = serport_inw;
  hdl[2].inl    = serport_inl;
  hdl[2].outb   = serport_outb;
  hdl[2].outw   = serport_outw;
  hdl[2].outl   = serport_outl;
  hdl[2].opaque = NULL;
  io_register_handler(&hdl[2]);

  hdl[3].base   = 0x02E8;
  hdl[3].mask   = 0xFFF8;
  hdl[3].inb    = serport_inb;
  hdl[3].inw    = serport_inw;
  hdl[3].inl    = serport_inl;
  hdl[3].outb   = serport_outb;
  hdl[3].outw   = serport_outw;
  hdl[3].outl   = serport_outl;
  hdl[3].opaque = NULL;
  io_register_handler(&hdl[3]);

  return 0;
}
