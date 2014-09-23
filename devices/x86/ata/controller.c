#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE     /* See feature_test_macros(7) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "system.h"
#include "io.h"
#include "irq.h"
#include "pci.h"
#include "log.h"
#include "devices/x86/ata/ata_internal.h"

static pcidev_t dev;

static void do_softreset(atacontroller_t *pCont,atadev_t *pDev) {
  memset(pDev->readBuffer,0,512);
  memset(pDev->reg,0,sizeof(uint8) * 0xC);

  // Accoding to 9.12
  pDev->reg[REG_SECTOR_COUNT] = 0x01;
  pDev->reg[REG_SECTOR_NUMBER] = 0x01;
  pDev->reg[REG_CYLINDER_LOW]  = 0x00;
  pDev->reg[REG_CYLINDER_HIGH] = 0x00;

  // Wild guess
  pDev->reg[REG_ERROR]  = 0;
  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];

  pCont->pCurrDev = &pCont->master;

  irq_set(14,0);

  LOG("SOFT RESET");
}

static void cmd_identify_packet_device(atacontroller_t *pCont,atadev_t *pDev) {
  if( pDev->type != eATADeviceType_DVDRom ) { // We dont implement PACKET for non WORM devices
    pDev->reg[REG_ERROR]  = BIT_ERROR_ABRT;
    pDev->reg[REG_STATUS] = BIT_STATUS_ERR | BIT_STATUS_DRDY;
    return;
  }
  ASSERT(0,"IDENTIFY PACKET not implemented");
}

static uint32 devNumSectors(atadev_t *pDev) {
  off64_t size;

  size = lseek64(pDev->backingfile,0,SEEK_END);

  return size / 512LL;
}

// ATA/ATAPI - Section 8.48
static void cmd_write_sectors(atacontroller_t *pCont) {
  atadev_t *pDev = pCont->pCurrDev;
  uint32 lba;

#if 1
  LOG("WRITE SECTOR(S)");
  LOG(" Sector count : %u",pDev->reg[REG_SECTOR_COUNT]);
  LOG(" Sector number: %u",pDev->reg[REG_SECTOR_NUMBER]);
  LOG(" Cylinder low : %u",pDev->reg[REG_CYLINDER_LOW]);
  LOG(" Cylinder high: %u",pDev->reg[REG_CYLINDER_HIGH]);
  LOG(" Device/Head  : %u",pDev->reg[REG_DEVICE_HEAD]);
  LOG(" Is LBA?      : %u",(pDev->reg[REG_DEVICE_HEAD] >> 6)&1);
#endif
  lba = ((pDev->reg[REG_DEVICE_HEAD] & 0xF) << 24) |
    (pDev->reg[REG_CYLINDER_HIGH] << 16) |
    (pDev->reg[REG_CYLINDER_LOW] << 8) |
    pDev->reg[REG_SECTOR_NUMBER];
  LOG(" LBA Address  : %u",lba);

  ASSERT( ((pDev->reg[REG_DEVICE_HEAD] >> 6)&1) == 1, "CHS Translation not implemented");

  pCont->pCurrDev->writeBufferOffset = 0;
  pCont->pCurrDev->writeBuffer = (uint8*)calloc(1,512);
  pCont->pCurrDev->writeBufferLBA = lba;

  // Signal that the data is ready
  pDev->reg[REG_ERROR]  = 0;
  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];

  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY | BIT_STATUS_DRQ;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];
}

// ATA/ATAPI - Section 8.27
static void cmd_read_sectors(atacontroller_t *pCont) {
  atadev_t *pDev = pCont->pCurrDev;
  uint32 lba;
#if 0
  LOG("READ SECTOR(S)");
  LOG(" Sector count : %u",pDev->reg[REG_SECTOR_COUNT]);
  LOG(" Sector number: %u",pDev->reg[REG_SECTOR_NUMBER]);
  LOG(" Cylinder low : %u",pDev->reg[REG_CYLINDER_LOW]);
  LOG(" Cylinder high: %u",pDev->reg[REG_CYLINDER_HIGH]);
  LOG(" Device/Head  : %u",pDev->reg[REG_DEVICE_HEAD]);
  LOG(" Is LBA?      : %u",(pDev->reg[REG_DEVICE_HEAD] >> 6)&1);
#endif
  if( ((pDev->reg[REG_DEVICE_HEAD] >> 6)&1) == 1 ) { // LBA addressing
    lba = ((pDev->reg[REG_DEVICE_HEAD] & 0xF) << 24) |
      (pDev->reg[REG_CYLINDER_HIGH] << 16) |
      (pDev->reg[REG_CYLINDER_LOW] << 8) |
      pDev->reg[REG_SECTOR_NUMBER];
  } else { // CHS addressing
    uint32 c,h,s;

    s = pDev->reg[REG_SECTOR_NUMBER];
    h = pDev->reg[REG_DEVICE_HEAD] & 0xF;
    c = (pDev->reg[REG_CYLINDER_HIGH] << 8) | pDev->reg[REG_CYLINDER_LOW];
    LOG("C/H/S = %u / %u / %u",c,h,s);

    lba = (((c * 255) + h) * 63) + s - 1;
    //ASSERT(0,"CHS addressing not supported");
  }

  //ASSERT( (pDev->reg[REG_DEVICE_HEAD] & (1<<6)) == 1, "CHS Translation not implemented");

  //LOG(" LBA Address  : %u",lba);

  //ASSERT(pDev->reg[REG_SECTOR_COUNT] == 1,"Multiple sector reads not supported");

  pDev->readBufferSize = pDev->reg[REG_SECTOR_COUNT];
  if( pDev->readBufferSize == 0 ) pDev->readBufferSize = 256;
  pDev->readBufferSize *= 512;

  //LOG("About to alloc/read %u bytes",pDev->readBufferSize);

  if( pDev->readBuffer != NULL ) free(pDev->readBuffer);
  pDev->readBuffer = (uint8*)calloc(1,pDev->readBufferSize);

  lseek64(pDev->backingfile,(uint64)lba * 512LL,SEEK_SET);
  read(pDev->backingfile,pDev->readBuffer,pDev->readBufferSize);
  pDev->readBufferOffset = 0;
  //pDev->readBufferSize   = 512;

  // Signal that the data is ready
  pDev->reg[REG_ERROR]  = 0;
  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY | BIT_STATUS_DRQ;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];

  irq_set(14,1);

  //ASSERT(0,"Implement more");
}

static void build_identify_data(atacontroller_t *pCont,atadev_t *pDev) {
  uint32 sectors;
  uint16 c,h,s;
  int i;

  sectors = devNumSectors(pDev);
  
  // LBA -> CHS
  if( sectors <= 8257536 ) {
    h = 16;
  } else if( sectors > 16514064 ) {
    h = 15;
  } else {
    h = 16; // ??
  }
  if( sectors > 1032192 ) {
    s = 63;
  } else {
    s = 63; // ?
  }
  c = sectors / (h * s);
  LOG("%u sectors translated to CHS %u %u %u",sectors,c,h,s);

  memset(pDev->identify,0,sizeof(pDev->identify));

  pDev->identify[0] = (1<<6) | (1<<1); // Not removable
  strncpy((char *)&pDev->identify[10],"Serial Number",20); 
  strncpy((char *)&pDev->identify[23],"Firmware",8);
  strncpy((char *)&pDev->identify[27],"KVMulate VirtuHD",40); // Model number
  pDev->identify[50] = (1<<14);
  pDev->identify[53] = (1<<0); // Words 54-58 are valid (needed for LBA)
  pDev->identify[57] = sectors & 0x0000FFFF;
  pDev->identify[58] = (sectors & 0xFFFF0000) >> 16;
  pDev->identify[60] = pDev->identify[57];
  pDev->identify[61] = pDev->identify[58];

  // CHS Emulation
  pDev->identify[3] = h; // Heads
  pDev->identify[6] = s; // Sectors
  pDev->identify[1] = c; // Cylinders

  // Fixup some data
  /* Convert model number to BE */
  for(i=0;i<20;i++) {
    uint16 tmp;

    tmp = pDev->identify[27+i];
    pDev->identify[27+i] = ((tmp & 0xFF)<<8) | ((tmp & 0xFF00)>>8);
  }

}

// ATA/ATAPI - Section 8.12
static void cmd_identify_device(atacontroller_t *pCont,atadev_t *pDev) {
  //uint16 *pWord;
  //uint32  lbas;
  //int     i;

  if( pDev->type == eATADeviceType_DVDRom ) { // If we are PACKET, dont respond
    do_softreset(pCont,pDev);
    return;
  }

  LOG("Building IDENTIFY data");
#if 1
  if( pDev->readBuffer != NULL ) free(pDev->readBuffer);
  pDev->readBuffer = (uint8*)calloc(1,512);

  memcpy(pDev->readBuffer,pDev->identify,512);
#else
  lbas = devNumSectors(pDev);

  if( pDev->readBuffer != NULL ) free(pDev->readBuffer);
  pDev->readBuffer = (uint8*)calloc(1,512);
  //memset(pDev->readBuffer,0,512);
  pWord = (uint16*)pDev->readBuffer;

  pWord[0] = (1<<6) | (1<<1); // Not removable
  strncpy((char *)&pWord[10],"Serial Number",20); 
  strncpy((char *)&pWord[23],"Firmware",8);
  strncpy((char *)&pWord[27],"KVMulate VirtuHD",40); // Model number
  pWord[50] = (1<<14);
  pWord[53] = (1<<0); // Words 54-58 are valid (needed for LBA)
  pWord[57] = lbas & 0x0000FFFF;
  pWord[58] = (lbas & 0xFFFF0000) >> 16;
  pWord[60] = pWord[57];
  pWord[61] = pWord[58];

  // CHS Emulation
  pWord[3] = 255; // Heads
  pWord[6] = 63; // Sectors
  pWord[1] = lbas / 255 / 63; // Cylinders

  // Fixup some data
  /* Convert model number to BE */
  for(i=0;i<20;i++) {
    uint16 tmp;

    tmp = pWord[27+i];
    pWord[27+i] = ((tmp & 0xFF)<<8) | ((tmp & 0xFF00)>>8);
  }
#endif

  pDev->readBufferOffset = 0;
  pDev->readBufferSize   = 512;

  // Signal that the data is ready
  pDev->reg[REG_ERROR]  = 0;
  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY | BIT_STATUS_DRQ;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];
}

static void cmd_initialize_device_parameters(atacontroller_t *pCont,atadev_t *pDev) {
  LOG("Logical sectors per logical track: %u",pDev->reg[REG_SECTOR_COUNT]);
  LOG("Maximum head number: %u",pDev->reg[REG_DEVICE_HEAD] & 0xF);

  //ASSERT(0,"Not implemented");

  pDev->reg[REG_ERROR]  = 0;
  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];

  //pDev->reg[REG_ERROR]  = BIT_ERROR_ABRT;
  //pDev->reg[REG_STATUS] = BIT_STATUS_ERR;
  irq_set(14,1);
}

static void cmd_recalibrate(atacontroller_t *pCont,atadev_t *pDev) {
  pDev->reg[REG_ERROR]  = 0;
  pDev->reg[REG_STATUS] = BIT_STATUS_DRDY;
  pDev->reg[REG_ALTERNATE_STATUS] = pDev->reg[REG_STATUS];

  irq_set(14,1);
}
#if 0
static void cmd_abort(atacontroller_t *pCont,atadev_t *pDev) {
  pDev->reg[REG_ERROR]  = BIT_ERROR_ABRT;
  pDev->reg[REG_STATUS] = BIT_STATUS_ERR;
  irq_set(14,1);
}
#endif
static void do_command(atacontroller_t *pCont,atadev_t *pDev,uint8 cmd) {
  //LOG("Trying to run command 0x%x",cmd);

  irq_set(14,0);

  switch(cmd) {
  case 0x10: // "Recalibrate" ( Obsolete )
    cmd_recalibrate(pCont,pDev);
    break;
  case CMD_WRITE_SECTORS:
    cmd_write_sectors(pCont);
    break;
  case CMD_READ_SECTORS:
    cmd_read_sectors(pCont);
    break;
  case CMD_IDENTIFY_DEVICE:
    cmd_identify_device(pCont,pDev);
    break;
  case CMD_IDENTIFY_PACKET_DEVICE:
    cmd_identify_packet_device(pCont,pDev);
    break;
  case CMD_INITIALIZE_DEV_PARAMS:
    cmd_initialize_device_parameters(pCont,pDev);
    break;
  default:
    ASSERT(0,"Unhandled command 0x%02x",cmd);
  }
}

static void pio_outb(io_handler_t *handler,uint16 address,uint8 val) {
  atacontroller_t *pCont = (atacontroller_t*)handler->opaque;

  //LOG("Address=0x%x (val=0x%x)",address,val);

  if( address == PORT_FLOPPY0 )
    return;

  if( address == PORT_RW_DEVICE_HEAD ) {
    if( (val >> 4) & 1 ) {
      pCont->pCurrDev = &pCont->slave;
      //LOG("Selecting device: SLAVE  <--------");
    } else {
      pCont->pCurrDev = &pCont->master;
      //LOG("Selecting device: MASTER  <--------");
    }
    pCont->pCurrDev->reg[REG_DEVICE_HEAD] = val;
    return;
  }

  if( !pCont->pCurrDev->bIsPresent ) {
    if( pCont->pCurrDev == &pCont->slave ) {
      LOG("Slave device not present..");
    } else {
      LOG("Master device not present..");
    }
    return;
  }

  switch(address) {
  case PORT_W_COMMAND:
    pCont->pCurrDev->reg[REG_COMMAND] = val;
    irq_set(14,0);
    do_command(pCont,pCont->pCurrDev,val);
    break;
  case PORT_W_DEVICE_CONTROL:
    if( val & BIT_DEVICE_CONTROL_SRST )
      do_softreset(pCont,pCont->pCurrDev);
    pCont->pCurrDev->reg[REG_DEVICE_CONTROL] = val;
  case PORT_W_FEATURES:
    pCont->pCurrDev->reg[REG_FEATURES] = val;
    break;
  case PORT_RW_CYLINDER_HIGH:
    pCont->pCurrDev->reg[REG_CYLINDER_HIGH] = val;
    break;
  case PORT_RW_CYLINDER_LOW:
    pCont->pCurrDev->reg[REG_CYLINDER_LOW] = val;
    break;
  case PORT_RW_SECTOR_COUNT:
    pCont->pCurrDev->reg[REG_SECTOR_COUNT] = val;
    break;
  case PORT_RW_SECTOR_NUMBER:
    pCont->pCurrDev->reg[REG_SECTOR_NUMBER] = val;
    break;
  default:
    ASSERT(0,"Write to unhandled port 0x%x (val=0x%x)",address,val);
    break;
  }
  
}

static uint8 pio_inb(io_handler_t *handler,uint16 address)  {
  atacontroller_t *pCont = (atacontroller_t*)handler->opaque;

  //LOG("Address=0x%x",address);

  if( address == PORT_FLOPPY0 )
    return 0x00;

  if( handler->base+address == 0x3F4 )
    return 0x00; //??

  if( address == PORT_RW_DEVICE_HEAD ) {
    if( pCont->pCurrDev == &pCont->slave )
      return 0xB0;
    else
      return 0xA0;
  }

  if( !pCont->pCurrDev->bIsPresent ) {
    LOG("Device not present... Returning 0x00");
    return 0x00;
  }

  switch(address) {
  case PORT_R_ERROR:
    return pCont->pCurrDev->reg[REG_ERROR];
    break;
  case PORT_RW_SECTOR_COUNT:
    //LOG("Returning 0x%x",pCont->pCurrDev->reg[REG_SECTOR_COUNT]);
    return pCont->pCurrDev->reg[REG_SECTOR_COUNT];
    break;
  case PORT_RW_SECTOR_NUMBER:
    //LOG("Returning 0x%x",pCont->pCurrDev->reg[REG_SECTOR_NUMBER]);
    return pCont->pCurrDev->reg[REG_SECTOR_NUMBER];
    break;
  case PORT_R_STATUS:
    //LOG("Returning 0x%x",pCont->pCurrDev->reg[REG_STATUS]);
    irq_set(14,0);
    return pCont->pCurrDev->reg[REG_STATUS];
    break;
  case PORT_R_ALTERNATE_STATUS:
    return pCont->pCurrDev->reg[REG_ALTERNATE_STATUS];
    break;
  default:
    //if( address == 0x204 ) return 0x00;
    ASSERT(0,"Read from unhandled port 0x%x",handler->base+address);
    return 0xFF;
    break;
  }

  return 0xFF;
}

static uint16 pio_inw(io_handler_t *handler,uint16 address)  {
  atacontroller_t *pCont = (atacontroller_t*)handler->opaque;
  uint16 val;

  //ASSERT(0,"Address=0x%x",address);

  ASSERT(address == 0,"Not implemented inw port");

  //LOG("Read with SC = %u",pCont->pCurrDev->reg[REG_SECTOR_COUNT]);

  val = *(uint16*)( &pCont->pCurrDev->readBuffer[ pCont->pCurrDev->readBufferOffset ] );
  pCont->pCurrDev->readBufferOffset += 2;

  if( pCont->pCurrDev->readBufferOffset >= pCont->pCurrDev->readBufferSize ) { // Last word transmitted
    pCont->pCurrDev->reg[REG_STATUS] = BIT_STATUS_DRDY;
    pCont->pCurrDev->reg[REG_ALTERNATE_STATUS] = pCont->pCurrDev->reg[REG_STATUS];
    //irq_set(14,1);
  } else  if( (pCont->pCurrDev->readBufferOffset & 0x1FF) == 0 ) { // Last word of a block is transmitted
    pCont->pCurrDev->reg[REG_ERROR]  = 0;
    pCont->pCurrDev->reg[REG_STATUS] = BIT_STATUS_DRDY | BIT_STATUS_DRQ;
    pCont->pCurrDev->reg[REG_ALTERNATE_STATUS] = pCont->pCurrDev->reg[REG_STATUS];
    //irq_set(14,1);
  }

  return val;
}

static void pio_outw(io_handler_t *handler,uint16 address,uint16 val) {
  atacontroller_t *pCont = (atacontroller_t*)handler->opaque;
  
  ASSERT(address == 0,"Not implemented outw port");

  *(uint16*)( &pCont->pCurrDev->writeBuffer[pCont->pCurrDev->writeBufferOffset] ) = val;
  pCont->pCurrDev->writeBufferOffset += 2;
  //LOG("Data written at 0x%04x, offset is now %i",address,pCont->pCurrDev->writeBufferOffset);

  if( pCont->pCurrDev->writeBufferOffset == 512 ) {
    int r;

    LOG("Writing 512bytes to LBA %u",pCont->pCurrDev->writeBufferLBA);
    r = lseek64(pCont->pCurrDev->backingfile,
		(uint64)pCont->pCurrDev->writeBufferLBA * 512LL,SEEK_SET);
    if(r==-1) perror("Seek-for-write");

    r = write(pCont->pCurrDev->backingfile,
	      pCont->pCurrDev->writeBuffer,
	      512);
    if(r==-1) perror("Write-for-write");

    pCont->pCurrDev->reg[REG_STATUS] = BIT_STATUS_DRDY;
    pCont->pCurrDev->reg[REG_ALTERNATE_STATUS] = pCont->pCurrDev->reg[REG_STATUS];
    irq_set(14,1);
  }
}

static uint8 pciata_readb(int reg) {
  uint8 *pRet;

  pRet = ((uint8*)&dev.conf) + reg;
  //LOG("Read from reg 0x%x (0x%x)",reg,*pRet);
  return *pRet;
}

static uint16 pciata_readw(int reg) {
  uint8 *pRet;

  pRet = ((uint8*)&dev.conf) + reg;
  //LOG("Read from reg 0x%x (0x%x)",reg,*pRet);
  //ASSERT(0,"bah");
  return *(uint16*)pRet;
}

static uint32 pciata_readl(int reg) {
  uint8 *pRet;

  pRet = ((uint8*)&dev.conf) + reg;
  LOG("Read from reg 0x%x (0x%x)",reg,*(uint32*)pRet);
  //ASSERT(0,"bah");
  return *(uint32*)pRet;
}

static void pciata_writeb(int reg,uint8 val) {
  //uint8 *pConf;

  //pConf = ((uint8*)&dev.conf) + reg;

  //LOG("Reg=0x%x, Val=0x%x",reg,val);

  //*(uint8*)pConf = val;
  //pciata_dumpWrite(reg,val);
  //ASSERT(0,"Not implemented");
}
static void pciata_writew(int reg,uint16 val) {
  //uint8 *pConf;

  //pConf = ((uint8*)&dev.conf) + reg;

  //pciata_dumpWrite(reg,val);
  //ASSERT(0,"Not implemented (reg=0x%x,val=0x%x)",reg,val);
}

static void pciata_writel(int reg,uint32 val) {
  uint8 *pConf;

  LOG("Setting reg 0x%x to 0x%x",reg,val);

  pConf = ((uint8*)&dev.conf) + reg;

  if( (reg == PCI_CFG_REG_BAR0) && (val == 0xFFFFFFFF) ) {
    //*(uint32*)pConf = | 1; // 128KB
    dev.conf.bar[0] =
      0xFFFFFF0 | // 256B
      (0<<1) | // Reserved in PCI 2.2 spec
      (1<<0);  // IO
    *(uint32*)pConf = 0;
    return;
  } else if( (reg == PCI_CFG_REG_BAR1) ||
             (reg == PCI_CFG_REG_BAR2) ||
             (reg == PCI_CFG_REG_BAR3) ||
             (reg == PCI_CFG_REG_BAR4) ||
             (reg == PCI_CFG_REG_BAR5) ||
             (reg == PCI_CFG_REG_EXPROM) ) {
    *(uint32*)pConf = 0;
    return;
  } else {
    *(uint32*)pConf = val;
  }

  //ASSERT(0,"Not implemented");
  //pciata_dumpWrite(reg,val);
}

int x86_ata_init(sys_t *pCtx) {
  static io_handler_t io;
  atacontroller_t *pCont;

  pCont = (atacontroller_t*)calloc(1,sizeof(atacontroller_t));
  pCont->pCurrDev = &pCont->master;
  pCont->master.bIsPresent = 1;
  pCont->master.type = eATADeviceType_Disk;

  pCont->master.backingfile = open("hdd.img",O_RDWR | O_LARGEFILE);
  ASSERT(pCont->master.backingfile != -1,"Failed to open backing file for HDD emulation");
  pCont->master.reg[REG_STATUS] = BIT_STATUS_DRDY;

  build_identify_data(pCont,&pCont->master);

  memset(&io,0,sizeof(io_handler_t));
  io.opaque = pCont;
  io.base   = 0x1F0;
  io.mask   = 0xFFF8;
  io.mask   = 0xFDF8;
  io.inb    = pio_inb;
  io.inw    = pio_inw;
  io.outb   = pio_outb;
  io.outw   = pio_outw;
  io_register_handler(&io);

  memset(&dev.conf,0,sizeof(dev.conf));

  dev.conf.vendorid    = 0xf00d;
  dev.conf.deviceid    = 0xdead;
  dev.conf.commandreg  = 0x0003;
  dev.conf.statusreg   = 0x0280;
  dev.conf.classandrev = 
    (0x01 << 24) | // Class Code
    (0x01 << 16) | // Sub-Class code
    (((1<<7)+3) <<  8) | // Prog-IF
    (0x00 <<  0);  // Revision

  dev.readb  = pciata_readb;
  dev.readw  = pciata_readw;
  dev.readl  = pciata_readl;
  dev.writeb = pciata_writeb;
  dev.writew = pciata_writew;
  dev.writel = pciata_writel;
  pci_register_dev(-1,&dev);


  return 0;
}
