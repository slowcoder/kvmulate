#pragma once

#define PORT_R_ALTERNATE_STATUS  0x206 // AN AAN
#define PORT_W_DEVICE_CONTROL    0x206 // AN AAN
#define PORT_R_STATUS            0x007 // NA AAA
#define PORT_W_COMMAND           0x007 // NA AAA
#define PORT_RW_CYLINDER_HIGH    0x005 // NA ANA
#define PORT_RW_CYLINDER_LOW     0x004 // NA ANN
//#define PORT_RW_DATA_PORT        // NN XXX   Physical port only used for DMA transfers. Not IO accessible
#define PORT_RW_DATA_REGISTER    0x000 // NA NNN
#define PORT_RW_DEVICE_HEAD      0x006 // NA AAN
#define PORT_R_ERROR             0x001 // NA NNA
#define PORT_W_FEATURES          0x001 // NA NNA
#define PORT_RW_SECTOR_COUNT     0x002 // NA NAN
#define PORT_RW_SECTOR_NUMBER    0x003 // NA NAA

#define PORT_FLOPPY0 0x202

#define REG_DATA             0x00
#define REG_ERROR            0x01
#define REG_FEATURES         0x02
#define REG_SECTOR_COUNT     0x03
#define REG_SECTOR_NUMBER    0x04
#define REG_CYLINDER_LOW     0x05
#define REG_CYLINDER_HIGH    0x06
#define REG_DEVICE_HEAD      0x07
#define REG_STATUS           0x08
#define REG_COMMAND          0x09
#define REG_DEVICE_CONTROL   0x0A
#define REG_ALTERNATE_STATUS 0x0B

#define BIT_STATUS_BSY  (1<<7)
#define BIT_STATUS_DRDY (1<<6)
#define BIT_STATUS_DF   (1<<5)
#define BIT_STATUS_DRQ  (1<<3)
#define BIT_STATUS_ERR  (1<<0)
#define BIT_ERROR_ABRT  (1<<2)
#define BIT_DEVICE_CONTROL_SRST (1<<2)
#define BIT_DEVICE_CONTROL_NIEN (1<<1)

#define CMD_READ_SECTORS           0x20
#define CMD_WRITE_SECTORS          0x30
#define CMD_INITIALIZE_DEV_PARAMS  0x91
#define CMD_IDENTIFY_PACKET_DEVICE 0xA1
#define CMD_IDENTIFY_DEVICE        0xEC

typedef enum {
  eATADeviceType_Invalid = 0,
  eATADeviceType_Disk,
  eATADeviceType_DVDRom,
} eATADeviceType;

typedef struct atadev {
  int            bIsPresent;
  eATADeviceType type;
  int            backingfile;

  uint16 identify[256];

  uint8  reg[0x0C];

  uint8  *readBuffer;
  int     readBufferOffset,readBufferSize; 

  uint8  *writeBuffer;
  int     writeBufferOffset,writeBufferSize; 
  uint64  writeBufferLBA;
} atadev_t;

typedef struct atacontroller {
  atadev_t master;
  atadev_t slave;
  atadev_t *pCurrDev;
} atacontroller_t;
