/*
 * Keyboard (Controller port B)
 */
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "irq.h"
#include "log.h"

#define PORT_DATA    0
#define CONTROL_PORT 1
#define PORT_STATUS  4
#define PORT_COMMAND 4

static struct {
  uint8 status;
  uint8 command_byte;

  uint8 buf_output[8];
  int   ndx_output;

  uint8 cmdparams[8];
  int   cmdparams_left;
  uint8 cmdparams_cmd;

  uint8 kbdparams[8];
  int   kbdparams_left;
  uint8 kbdparams_cmd;

  uint8 control;
} regs;


void kbd_inject(uint8 sc,int bIsPressed) {
  LOG("INJECT!!! (bIsPressed=%i)",bIsPressed);

#if 1
  if( bIsPressed ) {
    regs.buf_output[0] = sc;
    regs.ndx_output = 0;
  } else {
#if 1
    regs.buf_output[0] = sc | 0x80;
    regs.ndx_output = 0;
#else
    regs.buf_output[0] = 0xF0;
    regs.buf_output[1] = 0x26;
    regs.ndx_output = 1;
#endif
  }
  regs.status |= (1<<0);
  irq_set(1,1);  
#else
  if( bIsPressed ) {
    regs.buf_out = 0x04; // '3' pressed SCS1
    //regs.buf_out = 0x26; // '3' pressed SCS2
    //bIsPressed = 1;
  } else {
    regs.buf_out = 0x84; // '3' released SCS1
    //regs.buf_out = 0xF0; // '3' released SCS2 (incomplete)
    //bIsPressed = 0;
  }
  regs.status |= (1<<0);
  irq_set(1,1);
#endif
}

static void handle_command_withparams(uint8 cmd,uint8 *pParams) {
  // NOTE!! Parameters are in reverse order..

  if( cmd == 0x60 ) { // Write command byte
    regs.command_byte = pParams[0];
  } else if( cmd == 0xD3 ) { // Write next byte to first PS/2 port input buffer
    // do nothing..
  } else if( cmd == 0xD4 ) { // Write next byte to second PS/2 port input buffer
    // do nothing..
  } else if( cmd == 0xD1 ) { // Write next byte to controller output port
    //ASSERT(0,"Val=0x%02x",pParams[0]);
  } else {
    ASSERT(0,"Implement me (cmd=0x%02x)",cmd);
  }
}

static void handle_command(uint8 cmd) {
  switch(cmd) {
  case 0x20: // Read command byte
    regs.buf_output[0] = regs.command_byte;
    regs.ndx_output = 0;
    regs.status |= (1<<0); // Flag output buffer full
  case 0x60: // Write command byte
    regs.cmdparams_left = 1;
    regs.cmdparams_cmd = cmd;
    break;
  case 0xA9:
    regs.buf_output[0] = 0x01; // Clock line stuck low
    regs.ndx_output = 0;
    regs.status |= (1<<0); // Flag output buffer full
    break;
  case 0xAA: // Controller self-test
    regs.buf_output[0] = 0x55;
    regs.ndx_output = 0;
    regs.status |= (1<<0); // Flag output buffer full
    break;
  case 0xAD: // Disable first PS/2 port
    //LOG("!!!! DISABLE FIRST PS/2 PORT !!!!");
    break;
  case 0xAE: // Enable first PS/2 port
    //LOG("!!!! ENABLE FIRST PS/2 PORT !!!!");
    break;
  case 0xAB: // Keyboard interface test
    regs.buf_output[0] = 0x00;
    regs.ndx_output = 0;
    regs.status |= (1<<0); // Flag output buffer full
    break;
  case 0xD1: // Write next byte to controller output port
    regs.cmdparams_left = 1;
    regs.cmdparams_cmd = cmd;
    break;
  case 0xD3: // Write next byte to first PS/2 port
    regs.cmdparams_left = 1;
    regs.cmdparams_cmd = cmd;
    break;
  case 0xD4: // Write next byte to second PS/2 port
    regs.cmdparams_left = 1;
    regs.cmdparams_cmd = cmd;
    break;
  default:
    if( cmd == 0xFF ) return;
    ASSERT(0,"Unimplemented command 0x%02x",cmd);
    break;
  }
}

static void handle_kbd_command_withparams(uint8 cmd,uint8 *pParams) {
  if( cmd == 0xED ) {
    LOG("Set LEDs to 0x%02x",pParams[0]);
    regs.buf_output[0] = 0xFA; // ACK
    regs.ndx_output = 0;
    regs.status = (1<<0);
  } else if( cmd == 0xF0 ) {
    ASSERT(pParams != 0,"Getting scancode set is not implemented");
    //LOGE("!!!! Set scancode set to 0x%02x",pParams[0]);
    regs.buf_output[0] = 0xFA; // ACK
    regs.ndx_output = 0;
    regs.status = (1<<0);
  } else {
    ASSERT(0,"Implement me (cmd=0x%02x)",cmd);
  }
}

static void handle_kbd_command(uint8 cmd) {
  if( cmd == 0x05 ) { // ????

  } else if( cmd == 0xED ) { // Set LEDs
    regs.kbdparams_left = 1;
    regs.kbdparams_cmd = cmd;

    //regs.buf_output[0] = 0xFA; // ACK
    //regs.ndx_output = 0;
    //regs.status = (1<<0);
  } else if( cmd == 0xF0 ) { // Get/Set current scancode
    //LOGE("!!!!! GET/SET SCANCODE HERE !!!!!");
    regs.kbdparams_left = 1;
    regs.kbdparams_cmd = cmd;

    regs.buf_output[0] = 0xFA; // ACK
    regs.ndx_output = 0;
    regs.status = (1<<0);
  } else if( cmd == 0xF2 ) { // Identify keyboard
    regs.buf_output[0] = 0xFA; // ACK
    regs.ndx_output = 0;
    regs.status = (1<<0);
  } else if( cmd == 0xF4 ) { // Enable scanning
    //LOGE("!!!!! ENABLE SCANNING HERE !!!!!");

    regs.buf_output[0] = 0xFA; // ACK
    regs.ndx_output = 0;
    regs.status = (1<<0);
  } else if( cmd == 0xF5 ) { // Disable scanning
    //LOGE("!!!!! DISABLE SCANNING HERE !!!!!");

    regs.buf_output[0] = 0xFA; // ACK
    regs.ndx_output = 0;
    regs.status = (1<<0);
  } else if( cmd == 0xFF ) { // Restart and self-test
    regs.buf_output[0] = 0xAA;
    regs.buf_output[1] = 0xFA;
    regs.ndx_output = 1;
    regs.status = (1<<1); // Flag input buffer full (we got this from the "keyboard")
    //regs.status &= ~(1<<0); // Clear OBF
    //regs.status = 0x14;
    regs.status = 1;
    //LOG("Restart and self-test");
  } else {
    ASSERT(0,"Implement me (cmd=0x%02x)",cmd);
  }
}

static uint8 kbd_inb(struct io_handler *hdl,uint16 port) {
  port += hdl->base;

  //LOG("Read from 0x%x",port);

  if( port == 0x64 ) { // Read status register
    //LOG(" Status is 0x%02x (CCB=0x%02x, XLTon=%i)",regs.status,regs.command_byte,(regs.command_byte>>6)&1);
    return regs.status;
  } else if( port == 0x60 ) { // Read data
    uint8 ret;

    //ASSERT(regs.ndx_output >= 0,"Read with empty output buffer...");
    if(regs.ndx_output < 0) {
      LOGE("Read with empty output buffer...");
      //ASSERT(0,"Read with empty output buffer...");
      return 0x00;
    }

    if( (regs.status & 1) == 0 )
      LOGE("Reading when OBF is 0?");

    ret = regs.buf_output[ regs.ndx_output ];
    regs.ndx_output--;
    irq_set(1,0);

    if( regs.ndx_output < 0 ) 
      regs.status &= ~(1<<0); // Clear OBF
    else
      irq_set(1,1);
    return ret;
  }

  ASSERT(0,"Unimplemented read from 0x%04x",port);

  return 0x00;
}

static uint16 kbd_inw(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04x",hdl->base + port);
  return 0x0000;
}
static uint32 kbd_inl(struct io_handler *hdl,uint16 port) {
  ASSERT(0,"Unimplemented read from 0x%04",hdl->base + port);
  return 0x00000000;
}

static void  kbd_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  port += hdl->base;

  //LOG("Write to 0x%x, val=0x%02x",port,val);

  if( port == 0x64 ) { // Command port
    handle_command(val);
  } else if( (port == 0x60) && (regs.cmdparams_left > 0) ) { // Parameters to a command
    regs.cmdparams[regs.cmdparams_left - 1] = val;
    regs.cmdparams_left--;
    if( regs.cmdparams_left <= 0 )
      handle_command_withparams(regs.cmdparams_cmd,regs.cmdparams);
  } else if( (port == 0x60) && (regs.kbdparams_left > 0) ) { // Parameters to a keyboard command
    regs.kbdparams[regs.kbdparams_left - 1] = val;
    regs.kbdparams_left--;
    if( regs.kbdparams_left <= 0 )
      handle_kbd_command_withparams(regs.kbdparams_cmd,regs.kbdparams);
  } else if( port == 0x60 ) { // If we're not expecting parameters, it's for the keyboard
    handle_kbd_command(val);
  } else {
    ASSERT(0,"Unimplemented write to 0x%04x (val=0x%02x)",port,val);
  }
}

static void  kbd_outw(struct io_handler *hdl,uint16 port,uint16 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

static void  kbd_outl(struct io_handler *hdl,uint16 port,uint32 val) {
  ASSERT(0,"Unimplemented write to 0x%04x",hdl->base + port);
}

void kbd_timer2_set(int bState) {
  regs.control |= ((!!bState)<<5);
}

int x86_keyboard_init(void) {
  static struct io_handler hdl;

  hdl.base   = 0x0060;
  hdl.mask   = 0xFFF8;
  hdl.inb    = kbd_inb;
  hdl.inw    = kbd_inw;
  hdl.inl    = kbd_inl;
  hdl.outb   = kbd_outb;
  hdl.outw   = kbd_outw;
  hdl.outl   = kbd_outl;
  hdl.opaque = NULL;
  io_register_handler(&hdl);

  regs.status = 0x14;

  memset(&regs,0,sizeof(regs));

  return 0;
}
