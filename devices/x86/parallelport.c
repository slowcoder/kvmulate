/*
 * Parallel port
 */
#include <stdio.h>

#include "io.h"
#include "log.h"

#define DATA_PORT    0
#define STATUS_PORT  1
#define CONTROL_PORT 2

static uint8 parport_inb(struct io_handler *hdl,uint16 port) {
  //ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);

  return 0x00;
}

static uint16 parport_inw(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  return 0x0000;
}
static uint32 parport_inl(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04",hdl->base + port);
  return 0x00000000;
}

static void  parport_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  //ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static void  parport_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static void  parport_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

int x86_parallelport_init(void) {
  static struct io_handler hdl[2];

  hdl[0].base   = 0x0378;
  hdl[0].mask   = 0xFFFC;
  hdl[0].inb    = parport_inb;
  hdl[0].inw    = parport_inw;
  hdl[0].inl    = parport_inl;
  hdl[0].outb   = parport_outb;
  hdl[0].outw   = parport_outw;
  hdl[0].outl   = parport_outl;
  hdl[0].opaque = NULL;
  io_register_handler(&hdl[0]);

  hdl[1].base   = 0x0278;
  hdl[1].mask   = 0xFFFC;
  hdl[1].inb    = parport_inb;
  hdl[1].inw    = parport_inw;
  hdl[1].inl    = parport_inl;
  hdl[1].outb   = parport_outb;
  hdl[1].outw   = parport_outw;
  hdl[1].outl   = parport_outl;
  hdl[1].opaque = NULL;
  io_register_handler(&hdl[1]);

  return 0;
}
