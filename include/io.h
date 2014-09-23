#pragma once

#include "types.h"

typedef struct io_handler {
  uint16  base,mask;
  void   *opaque;
  uint8  (*inb )(struct io_handler *,uint16 address);
  uint16 (*inw )(struct io_handler *,uint16 address);
  uint32 (*inl )(struct io_handler *,uint16 address);
  void   (*outb)(struct io_handler *,uint16 address,uint8 val);
  void   (*outw)(struct io_handler *,uint16 address,uint16 val);
  void   (*outl)(struct io_handler *,uint16 address,uint32 val);

  char *pzDesc;
} io_handler_t;

struct iohdl *io_register_handler(io_handler_t *handler);
int           io_remap_handler(struct iohdl *hdl,io_handler_t *pNewHandler);

uint8  io_inb(uint16 port);
uint16 io_inw(uint16 port);
uint32 io_inl(uint16 port);
void   io_outb(uint16 port,uint8 val);
void   io_outw(uint16 port,uint16 val);
void   io_outl(uint16 port,uint32 val);
