#include <stdio.h>

#include "io.h"
#include "log.h"

#define MAX_HANDLERS 128

static int iohandlers = 0;
static io_handler_t *iohandler[MAX_HANDLERS] = { 0 };

typedef struct iohdl {
  int foo;
} iohdl_t;

static io_handler_t *lookup_handler(uint16 address) {
  static uint32 i;
 
  for(i=0;i<iohandlers;i++) {
    //    LOG("Addr=0x%x  ioh.m=0x%x  ioh.b=0x%x",address,iohandler[i]->mask,iohandler[i]->base);
    if( (address & iohandler[i]->mask) == iohandler[i]->base )  {
      //LOGD("Returning handler %p (Base=0x%x,Mask=0x%x,Desc=%s) for req addr 0x%x",iohandler[i],iohandler[i]->base,iohandler[i]->mask,iohandler[i]->pzDesc,address);
      return iohandler[i];
    }
  }
  return NULL;
}

struct iohdl *io_register_handler(io_handler_t *handler) {

  LOG("Registering IO handler for 0x%04x (Mask 0x%04x)",
      handler->base,handler->mask);

  if( iohandlers < (MAX_HANDLERS-1) ) {
    iohandler[iohandlers] = handler;
    iohandlers++;
    return (iohdl_t*)&iohandler[iohandlers-1];
  }
  return NULL;
}

int           io_remap_handler(struct iohdl *hdl,io_handler_t *pNewHandler) {
  io_handler_t **iohdl = (io_handler_t**)hdl;

  *iohdl = pNewHandler;

  return 0;
}

void io_outb(uint16 port,uint8 val) {
  io_handler_t *handler;

  handler = lookup_handler(port);

  //LOGE("outb @ 0x%04x, val=0x%x",port,val);

  if( handler != NULL ) {
    ASSERT(handler->outb != NULL,"NULL handler for outb @ 0x%04x",port);
    handler->outb(handler,port - handler->base,val);
    return;
  }

  if( handler == NULL)
    LOGW("No handler for writes to port 0x%04x (Val=0x%02x)",port,val);
}

void io_outw(uint16 port,uint16 val) {
  io_handler_t *handler;

  handler = lookup_handler(port);

  //LOGE("outw @ 0x%04x, val=0x%x (handler=%p)",port,val,handler);

  if( handler != NULL ) {
    ASSERT(handler->outw != NULL,"NULL handler for outw @ 0x%04x",port);
    handler->outw(handler,port - handler->base,val);
    return;
  }

#if BREAK_ON_UNHANDLED
  ASSERT(handler != NULL,"No handler for writes to port 0x%04x",port);
#endif
}

void io_outl(uint16 port,uint32 val) {
  io_handler_t *handler;

  handler = lookup_handler(port);

  //LOGE("outl @ 0x%04x, val=0x%x",port,val);

  if( handler != NULL ) {
    ASSERT(handler->outl != NULL,"NULL handler for outl @ 0x%04x",port);
    handler->outl(handler,port - handler->base,val);
    return;
  }

#if BREAK_ON_UNHANDLED
  ASSERT(handler != NULL,"No handler for writes to port 0x%04x",port);
#endif
}

uint8 io_inb(uint16 port) {
  io_handler_t *handler;

  handler = lookup_handler(port);

  //LOGE("inb @ 0x%04x",port);

  if( handler != NULL ) {
    return handler->inb(handler,port - handler->base);
  }

  if( handler == NULL)
    LOGW("No handler for reads from port 0x%04x",port);
  return 0xFF;
}

uint16 io_inw(uint16 port) {
  io_handler_t *handler;

  handler = lookup_handler(port);

  //LOGE("inw @ 0x%04x",port);

  if( handler != NULL ) {
    return handler->inw(handler,port - handler->base);
  }

#if BREAK_ON_UNHANDLED
  ASSERT(handler != NULL,"No handler for reads from port 0x%04x",port);
#endif
  return 0;
}

uint32 io_inl(uint16 port) {
  io_handler_t *handler;

  handler = lookup_handler(port);

  //LOGE("inl @ 0x%04x",port);

  if( handler != NULL ) {
    return handler->inl(handler,port - handler->base);
  }

#if BREAK_ON_UNHANDLED
  ASSERT(handler != NULL,"No handler for reads from port 0x%04x",port);
#endif
  return 0;
}

