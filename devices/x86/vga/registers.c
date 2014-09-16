/*
 * Dummy VGA registers
 */
#include <stdio.h>

#include "io.h"
#include "system.h"
#include "devices/x86/vga/vga.h"
#include "log.h"

void misc_output_write(vgactx_t *pVGA,uint8 val) {
  ASSERT( (val & 1) != 0, "I/OAS to 0x3Bx not handled yet");
  pVGA->misc_output_reg = val;
}

uint8 sequencer_read(vgactx_t *pVGA) {
  if( pVGA->seq_ndx == 0x02 ) { // Map Mask Register
    return pVGA->seq_reg[ pVGA->seq_ndx ];
  } else if( pVGA->seq_ndx == 0x04 ) { // Memory Mode Register
    return pVGA->seq_reg[ pVGA->seq_ndx ];
  } else if( pVGA->seq_ndx == -1 ) {
    return 0x00;
  } else {
    ASSERT(0,"Sequencer Reads from index 0x%02x not handled",pVGA->seq_ndx);
  }
}

void sequencer_write(vgactx_t *pVGA,uint8 val) {
  if( pVGA->seq_ndx == 0x04 ) {
    pVGA->seq_reg[4] = val;
  } else if( pVGA->seq_ndx == 0x00 ) {
    LOG("Sequecer RESET!");
  } else if( pVGA->seq_ndx == 0x01 ) {
    pVGA->seq_reg[1] = val;
  } else if( pVGA->seq_ndx == 0x02 ) {
    //LOG("MapMask changed to 0x%x (Memory Plane Write Enable)",val & 0xF);
    pVGA->seq_reg[2] = val & 0xF;
  } else if( pVGA->seq_ndx == 0x03 ) {
    pVGA->seq_reg[3] = val;
  } else if( pVGA->seq_ndx == -1 ) {
    LOGE("Unsupported sequencer register. Ndx=0x%02x, Val=0x%02x",pVGA->seq_ndx,val);
  } else {
    ASSERT(0,"Unsupported sequencer register. Ndx=0x%02x, Val=0x%02x",pVGA->seq_ndx,val);
  }
}

void palette_write(vgactx_t *pVGA,uint8 val) {
  pVGA->pel[pVGA->pel_write_addr] = val;
  pVGA->pel_write_addr++;
}

void graphics_controller_write(vgactx_t *pVGA,uint8 val) {
  if( pVGA->gcont_ndx != -1 )
    pVGA->gcont_reg[ pVGA->gcont_ndx  ] = val;

  if( pVGA->gcont_ndx == 4 )
    LOG("ReadMapSelect = 0x%02x",val & 3);

}

uint8 graphics_controller_read(vgactx_t *pVGA) {
  if( pVGA->gcont_ndx != -1 )
    return pVGA->gcont_reg[ pVGA->gcont_ndx  ];
  return 0x00;
}

uint8 mode_select_read(vgactx_t *pVGA) {
  return 0x00;
}

uint8 crtc_read(vgactx_t *pVGA) {
  if( pVGA->crtc_ndx == -1 )
    return 0x00;

  return pVGA->crtc_reg[ pVGA->crtc_ndx  ];
}

void crtc_write(vgactx_t *pVGA,uint8 val) {
#if 0
  if( pVGA->crtc_ndx == 0xC )
    LOG("CRTC StartAddr HIGH = 0x%02x",val);
  else if( pVGA->crtc_ndx == 0xD )
    LOG("CRTC StartAddr LOW = 0x%02x",val);
#endif

  pVGA->crtc_reg[ pVGA->crtc_ndx  ] = val;
}

uint8 attribute_read(vgactx_t *pVGA) {
  if( pVGA->attr_ndx == 0x10 )
    return pVGA->attr_reg[pVGA->attr_ndx];
  else if( pVGA->attr_ndx == 0x14 )
    return pVGA->attr_reg[pVGA->attr_ndx];
  else if( pVGA->attr_ndx > 0x14 )
    return 0x00;
  else
    ASSERT(0,"Read from unhandled attribute register 0x%02x",pVGA->attr_ndx);
}

static uint8 vgareg_inb(struct io_handler *hdl,uint16 port) {
  vgactx_t *pVGA = (vgactx_t*)hdl->opaque;
  port += hdl->base;

  if( port == 0x3B4 ) { // CRTC Address register
    return pVGA->crtc_ndx;
  } else if( port == 0x3C0 ) { // Attribute address register
    return pVGA->attr_ndx;
  } else if( port == 0x3C1 ) { // Attribute data register
    return attribute_read(pVGA);
  } else if( port == 0x3C4 ) { // Sequencer address register
    return pVGA->seq_ndx;
  } else if( port == 0x3C5 ) { // Sequencer data register
    return sequencer_read(pVGA);
  } else if( port == 0x3C9 ) { // DAC Data Register
    return pVGA->pel[ pVGA->pel_read_addr++ ];
  } else if( port == 0x3CA ) { // Feature Control Register
    return 0x00;
  } else if( port == 0x3CC ) { // Misc output register
    return pVGA->misc_output_reg;
  } else if( port == 0x3CE ) { // Graphics controller address register
    return pVGA->gcont_ndx;
  } else if( port == 0x3D3 ) { 
    return crtc_read(pVGA);
  } else if( port == 0x3D5 ) { // CRTC Data register
    return crtc_read(pVGA);
  } else if( port == 0x3D8 ) { // 6845 Mode select register (color)
    return mode_select_read(pVGA);
  } else if( port == 0x3DA ) { // Input Status #1
    return 0x00; // We're never "disabled" (CRT beam end->start transision), nor are we even in vertical retrace
  } else if( port == 0x3CF ) { // Graphics Controller Data Register
    return graphics_controller_read(pVGA);
  } else {
    ASSERT(0,"Unimplemented read from 0x%04x",port);
  }
  return 0xFF;
}
static uint16 vgareg_inw(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  return 0xFFFF;
}
static uint32 vgareg_inl(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04",hdl->base + port);
  return 0xFFFFFFFF;
}

static void  vgareg_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  vgactx_t *pVGA = (vgactx_t*)hdl->opaque;
  port += hdl->base;

  if( port == 0x3C0 ) { // Attribute Controller registers
    pVGA->attr_ndx = val;
  } else if( port == 0x3C2 ) {
    misc_output_write(pVGA,val);
  } else if( port == 0x3C4 ) {
    //ASSERT(val <= 4,"Invalid sequencer index 0x%02x",val);
    if( val <= 0x4 )
      pVGA->seq_ndx = val;
    else
      pVGA->seq_ndx = -1;
  } else if( port == 0x3C5 ) {
    sequencer_write(pVGA,val);
  } else if( port == 0x3C6 ) { // VGA_PEL_MASK
    ASSERT(val == 0xFF,"PEL Masks not supported");
  } else if( port == 0x3C7 ) { // VGA_PEL_READ_ADDRESS / DAC Read Address
    pVGA->pel_read_addr = val * 3;
  } else if( port == 0x3C8 ) { // VGA_PEL_WRITE_ADDRESS / DAC Write Address
    pVGA->pel_write_addr = val * 3;
  } else if( port == 0x3C9 ) { // VGA_PEL_DATA / DAC Data register
    palette_write(pVGA,val);
  } else if( port == 0x3CE ) { // Graphics controller Address Register
    //ASSERT(val <= 8,"Invalid graphics controller register selected (0x%02x)",val);
    if( val <= 8 ) {
      pVGA->gcont_ndx = val;
    } else {
      pVGA->gcont_ndx = -1;
      LOGE("Invalid graphics controller register selected (0x%02x)",val);
    }
  } else if( port == 0x3CF ) { // Graphics controller Data Register
    graphics_controller_write(pVGA,val);
  } else if( port == 0x3D4 ) { // CRTC Address register
    ASSERT(val <= 0x18,"Invalid CRTC register selected");
    pVGA->crtc_ndx = val;
  } else if( port == 0x3D5 ) { // CRTC Data register
    crtc_write(pVGA,val);
  } else if( port == 0x3D8 ) { // 6845 Mode select register (color)
    pVGA->misc_output_reg = val;
  } else {
    ASSERT(0,"Unimplemented write to 0x%04x (Val=0x%02x)",port,val);
  }
}

static void  vgareg_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  port += hdl->base;

  if( port == 0x3C4 ) { // Sequencer address register
    vgareg_outb(hdl,port - hdl->base + 0,val & 0xFF);
    vgareg_outb(hdl,port - hdl->base + 1,val >> 8);
 } else if( port == 0x3D4 ) { // CRTC Address register
    vgareg_outb(hdl,port - hdl->base + 0,val & 0xFF);
    vgareg_outb(hdl,port - hdl->base + 1,val >> 8);
  } else if( port == 0x3CE ) {
    vgareg_outb(hdl,port - hdl->base + 0,val & 0xFF);
    vgareg_outb(hdl,port - hdl->base + 1,val >> 8);
  } else {
    ASSERT(0,"Unimplemented write to 0x%04x. Val=0x%04x",port,val);
  }
}

static void  vgareg_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static uint8 dbg_inb(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Input from debug port?");
  return 0;
}

static void  dbg_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  //printf("%c",val);
}

static uint8 vbe_inb(struct io_handler *hdl,uint16 port) {
  LOG("VBE ReadB (0x%x)",port);
  return 0;
}

static uint16 vbe_inw(struct io_handler *hdl,uint16 port) {
  LOG("VBE ReadW (0x%x)",port);
  return 0;
}

static void  vbe_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  LOG("VBE Write (Addr 0x%x, Val 0x%x)",port,val);
}

static void  vbe_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  LOG("VBE Write (Addr 0x%x, Val 0x%x)",port,val);
}

int x86_vga_registers_init(sys_t *pCtx,vgactx_t *pVGA) {
  static io_handler_t   isaio[2];
  static io_handler_t   vgadbg;
  static struct io_handler vbe;


  isaio[0].opaque = pVGA;
  isaio[0].base   = 0x03C0;
  isaio[0].mask   = 0xFFF0;
  isaio[0].inb    = vgareg_inb;
  isaio[0].inw    = vgareg_inw;
  isaio[0].inl    = vgareg_inl;
  isaio[0].outb   = vgareg_outb;
  isaio[0].outw   = vgareg_outw;
  isaio[0].outl   = vgareg_outl;

  isaio[1].opaque = pVGA;
  isaio[1].base    = 0x03D0;
  isaio[1].mask    = 0xFFF0;
  isaio[1].inb       = vgareg_inb;
  isaio[1].inw       = vgareg_inw;
  isaio[1].inl       = vgareg_inl;
  isaio[1].outb      = vgareg_outb;
  isaio[1].outw      = vgareg_outw;
  isaio[1].outl      = vgareg_outl;

  io_register_handler(&isaio[0]);
  io_register_handler(&isaio[1]);

  vgadbg.base = 0x500;
  vgadbg.mask = 0xFFFF;
  vgadbg.inb  = dbg_inb;
  vgadbg.outb = dbg_outb;
  io_register_handler(&vgadbg);

  vbe.base = 0x1CE;
  vbe.mask = 0xFFFE;
  vbe.inb  = vbe_inb;
  vbe.inw  = vbe_inw;
  vbe.outb = vbe_outb;
  vbe.outw = vbe_outw;
  io_register_handler(&vbe);

  return 0;
}
