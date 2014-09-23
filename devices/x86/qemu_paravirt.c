/*
 * QEmu Paravirtualization Interface
 */
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "log.h"

typedef struct {
  uint16 reg;

  struct {
    int off;
    uint8 buff[1024];
  } ret;
} ctx_t;

static uint8 qemupara_inb(struct io_handler *hdl,uint16 port) {
  ctx_t *pCtx = (ctx_t*)hdl->opaque;

  ASSERT(pCtx->ret.off < 1024,"Return-buffer overflow");

  return pCtx->ret.buff[ pCtx->ret.off++ ];
  //ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  //return 0xFF;
}
static uint16 qemupara_inw(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  return 0xFFFF;
}
static uint32 qemupara_inl(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04",hdl->base + port);
  return 0xFFFFFFFF;
}

static void  qemupara_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

#pragma pack(push,1)
typedef struct e820entry {
  uint64 addr;
  uint64 length;
  uint32 type;
} e820entry_t;
typedef struct e820table {
  uint32 entries;
  e820entry_t entry[];
} e820table_t;
#pragma pack(pop)

#define E820_RAM          1
#define E820_RESERVED     2
#define E820_ACPI         3
#define E820_NVS          4
#define E820_UNUSABLE     5
#define E820_HOLE         ((uint32)-1) // Useful for removing entries

static void  qemupara_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  ctx_t *pCtx = (ctx_t*)hdl->opaque;

  if( port == 0x00 ) {
    pCtx->reg = val;

    if( val == 0x00 ) {
      pCtx->ret.off = 0;
      memcpy(pCtx->ret.buff,"QEMU",4);
    } else if( val == 0xD ) { // NUMA count
      *(uint32*)(  pCtx->ret.buff + 0  ) = 1; // 1
      //LOG("NUMA Count=0x%02x",v)
    } else if( val == 0xE ) { // Boot menu
      pCtx->ret.off = 0;
      pCtx->ret.buff[0] = 0;
      pCtx->ret.buff[1] = 1; // Do show boot menu
    } else if( val == 0x8000 ) { // ACPI Table
      pCtx->ret.off = 0;
    } else if( val == 0x8003 ) { // E820 Table
      e820table_t *pTable;

      pTable = (e820table_t*)calloc(1,sizeof(e820table_t) + (sizeof(e820entry_t) * 4));
      pTable->entries = 3;
      pTable->entry[0].addr   = 0x0;
      pTable->entry[0].length = 0xA0000;
      pTable->entry[0].type   = E820_RAM;
      pTable->entry[1].addr   = 0x100000;
      pTable->entry[1].length = 0xE0000000 - 0x100000;
      pTable->entry[1].type   = E820_RAM;
      pTable->entry[2].addr   = 0xFFFBC000;
      pTable->entry[2].length = 0x100000000 - 0xFFFBC000;
      pTable->entry[2].type   = E820_RESERVED;

      pCtx->ret.off = 0;
      memcpy(pCtx->ret.buff,pTable,4 + sizeof(e820entry_t) * 3);

      LOG("Built e820 map");
    } else if( val == 0x19 ) { // QEmu "Config File" ?
      pCtx->ret.off = 0;
      *(uint32*)(  pCtx->ret.buff + 0  ) = 0; // No entries
    } else if( val == 0x0F ) { // Get Max CPUs
      pCtx->ret.off = 0;
      *(uint32*)(  pCtx->ret.buff + 0  ) = 32; // 1 CPUs
    } else if( val == 0x8001 ) { // SMBios Entries
      pCtx->ret.off = 0;
      *(uint16*)(  pCtx->ret.buff + 0  ) = 0; // No entries
    } else if( val == 0x8002 ) { // IRQ0 Override
      pCtx->ret.off = 0;
      *(uint8*)(  pCtx->ret.buff + 0  ) = 0; // No override
    } else {
      ASSERT(0,"Unsupported QEmu function 0x%x",val);
    }
  } else
    ASSERT(0,"Unimplemented write to 0x%04x (Val=0x%x)",hdl->base + port,val);
}

static void  qemupara_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static struct io_handler hdl;
static ctx_t ctx;
int x86_qemuparavirt_init(void) {

  hdl.base   = 0x0510;
  hdl.mask   = 0xFFFE;
  hdl.inb    = qemupara_inb;
  hdl.inw    = qemupara_inw;
  hdl.inl    = qemupara_inl;
  hdl.outb   = qemupara_outb;
  hdl.outw   = qemupara_outw;
  hdl.outl   = qemupara_outl;
  hdl.opaque = &ctx;
  io_register_handler(&hdl);

  return 0;
}
