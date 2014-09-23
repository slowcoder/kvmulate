/*
 * Floppy controller/drive
 * Intel 82077A
 */
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "irq.h"
#include "log.h"

#define REG_STATUS_A              0x3F0
#define REG_STATUS_B              0x3F1
#define REG_DIGITAL_OUT           0x3F2
#define REG_TAPE_DRIVE            0x3F3
#define REG_MAIN_STATUS           0x3F4
#define REG_DATARATE_SELECT       0x3F4
#define REG_DATA_FIFO             0x3F5
#define REG_DIGITAL_INPUT         0x3F7
#define REG_CONFIGURATION_CONTROL 0x3F7

#define CMD_READ_DATA       0x6
#define CMD_RECALIBRATE     0x7
#define CMD_SENSE_INTERRUPT 0x8
#define CMD_READ_ID         0xA
#define CMD_SEEK            0xF

static struct {
  // Registers
  uint8 sra,srb,msr,dor,ccr,dsr;

  uint8 st[3];

  uint8 fifo[512+10];
  int   fifondx,fifolen;

  // Seek info
  struct {
    uint8 cyl,head,sect;
  } pos;

  FILE *in;

  void *pDMABuffer;
} fdc;

#define DOR_NRESET (1<<2)
#define DOR_MOTOR0 (1<<4)
#define MSR_RQM   (1<<7)
#define MSR_DIO   (1<<6)
#define MSR_NDMA  (1<<5)
#define MSR_CB    (1<<4)

static uint8 floppy_inb(struct io_handler *hdl,uint16 port) {

  port += hdl->base;

  if( port == REG_MAIN_STATUS ) {
    LOGD("Read from MSR (msr=0x%02x)",fdc.msr);
    return fdc.msr;
  } else if( port == REG_DIGITAL_OUT ) {
    LOGD("Read from DOR (dor=0x%02x)",fdc.dor);
    return fdc.dor;
  } else if( port == REG_DATA_FIFO ) {
    uint8 r;

    LOGD("Read from data FIFO");
    r = fdc.fifo[fdc.fifondx];
    fdc.fifondx++;
    if( fdc.fifondx >= fdc.fifolen ) {
      fdc.msr = 0xA0;
      fdc.fifondx = 0;
      LOGD(" * Returned 0x%02x, FIFO empty",r);
      irq_set(6,0);
    } else {
      LOGD(" * Returned 0x%02x, %u bytes left in FIFO",r,fdc.fifolen - fdc.fifondx);
    }
    return r;
  } else {
    ASSERT(0,"Unimplemented read from 0x%04x",port);
  }

  return 0;
}

static void doReset(void) {
  LOGD("Resetting FDC");

  fdc.sra = fdc.srb = fdc.msr = fdc.dor = fdc.ccr = fdc.dsr = 0;

  fdc.msr = MSR_RQM;

  fdc.fifondx = fdc.fifolen = 0;
}

static unsigned int chs_to_lba(int c,int h,int s) { 
  unsigned int lba; 

  s--; 

  lba = 18 * 2 * c; 
  lba += h * 18; 
  lba += s; 

  return lba;
}

void dma_transfer_from_device(int ch,void *pData,unsigned int uDatalen);

static void doExecuteCommand(void) {
  unsigned int lba;

  switch(fdc.fifo[0] & 0xF) {
    case CMD_READ_DATA:

      LOGD("Executing command \"Read Data\"");

      LOGW("!!!!! DO STUFF HERE !!!!!!");
      LOGD(" CHS=%u/%u/%u",fdc.fifo[2],fdc.fifo[3],fdc.fifo[4]);
      LOGD(" SectSize: %u (%u bytes)",fdc.fifo[5],(1<<fdc.fifo[5]) * 128);
      LOGD(" TrakLen=%u",fdc.fifo[6]);
      LOGD(" GAP3Len=%u",fdc.fifo[7]);
      LOGD(" DataLen=%u",fdc.fifo[8]);
      ASSERT(fdc.fifo[6] <= 128,"Number of sectors to read too large");

      fdc.pos.cyl  = fdc.fifo[2];
      fdc.pos.head = fdc.fifo[3];
      lba = chs_to_lba(fdc.pos.cyl,fdc.pos.head,fdc.fifo[4]);
      LOGD(" LBA=%u",lba);
      fseek(fdc.in,lba * 512,SEEK_SET);
      fread(fdc.pDMABuffer,512,fdc.fifo[6],fdc.in);
      dma_transfer_from_device(2,fdc.pDMABuffer,fdc.fifo[6] * 512);

      fdc.msr = MSR_RQM | MSR_DIO;
      fdc.fifolen = 7;
      fdc.fifondx = 0;
      fdc.fifo[0] = fdc.st[0];
      fdc.fifo[1] = fdc.st[1];
      fdc.fifo[2] = fdc.st[2];
      fdc.fifo[3] = fdc.pos.cyl;
      fdc.fifo[4] = fdc.pos.head;
      fdc.fifo[5] = fdc.pos.sect;
      fdc.fifo[6] = 2; // 512b sectors
      irq_set(6,1);
      break;
    case CMD_RECALIBRATE:
      LOGD("Executing command \"Recalibrate\"");
      fdc.pos.cyl = 0;
      fdc.msr = MSR_RQM;
      fdc.fifolen = 1;
      fdc.fifondx = 0;
      irq_set(6,0);
      irq_set(6,1);
      break;
    case CMD_SENSE_INTERRUPT:
      LOGD("Executing command \"Sense Interrupt\"");
      fdc.msr = MSR_RQM | MSR_DIO;
      fdc.fifolen = 2;
      fdc.fifondx = 0;
      fdc.fifo[0] = fdc.st[0];
      fdc.fifo[1] = fdc.pos.cyl; 
      irq_set(6,0);
      break;
    case CMD_READ_ID:
      LOGD("Executing command \"Read ID\"");
      fdc.msr = MSR_RQM | MSR_DIO;
      fdc.fifolen = 7;
      fdc.fifondx = 0;
      fdc.fifo[0] = fdc.st[0];
      fdc.fifo[1] = fdc.st[1];
      fdc.fifo[2] = fdc.st[2];
      fdc.fifo[3] = fdc.pos.cyl;
      fdc.fifo[4] = fdc.pos.head;
      fdc.fifo[5] = fdc.pos.sect;
      fdc.fifo[6] = 2; // 512b sectors
      irq_set(6,1);
      break;
    case CMD_SEEK:
      LOGD("Executing command \"Seek\"");
      fdc.msr = MSR_RQM | MSR_DIO;
      LOGD("Head: %u",fdc.fifo[1]>>2);
      LOGD("Cyl : %u",fdc.fifo[2]);
      //ASSERT(0,"Seek not implemented");

      fdc.pos.head = fdc.fifo[1] >> 2;
      fdc.pos.cyl  = fdc.fifo[2];
      fdc.fifolen = 0;
      fdc.fifondx = 0;
      fdc.msr = 0xA0;
      irq_set(6,1);
      break;
  }
}

static void  floppy_outb(struct io_handler *hdl,uint16 port,uint8 val) {

  port += hdl->base;

  if( port == REG_DIGITAL_OUT ) {
    LOGD("Write to DOR (Val=0x%02x)",val);

    // Is it a reset ?
    if( !(val & DOR_NRESET) ) {
        doReset();
        irq_set(6,1);
    }
    fdc.dor = val & ~DOR_NRESET;
  } else if( port == REG_DATA_FIFO ) {
    LOGD("Write to FIFO (Val=0x%02x,ndx=%i)",val,fdc.fifondx);

    if( fdc.fifondx == 0 ) {
      switch(val&0xF) {
        case CMD_READ_DATA:
          fdc.fifolen = 9;
          break;
        case CMD_RECALIBRATE:
          fdc.fifolen = 2;
          break;
        case CMD_SENSE_INTERRUPT:
          fdc.fifolen = 1;
          break;
        case CMD_READ_ID:
          fdc.fifolen = 2;
          break;
        case CMD_SEEK:
          fdc.fifolen = 3;
          break;
        default:
          ASSERT(0,"Unimplemented command 0x%02x",val);
          break;
      }

      fdc.msr |= MSR_CB;
    }

    fdc.fifo[ fdc.fifondx++ ] = val;

    // If we have all the parameters we need for this command, execute it
    if( fdc.fifondx >= fdc.fifolen ) {
      doExecuteCommand();
    }
  } else if( port == REG_CONFIGURATION_CONTROL ) {
    fdc.ccr = val;
    fdc.dsr = (fdc.dsr & 0xFC) | (val & 3); 
  } else {
    ASSERT(0,"Unimplemented write to 0x%04x (Val=0x%02x)",port,val);
  }
}

int x86_floppy_init(void) {
  static struct io_handler hdl;

  hdl.base   = 0x3F0;
  hdl.mask   = 0xFFF8;
  hdl.inb    = floppy_inb;
  hdl.outb   = floppy_outb;
  hdl.opaque = NULL;
  io_register_handler(&hdl);

  memset(&fdc,0,sizeof(fdc));
  doReset();
  fdc.in = fopen("floppy.img","rb");
  fdc.pos.sect = 1;

  fdc.pDMABuffer = malloc(64 * 1024);

  return 0;
}
