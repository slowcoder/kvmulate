#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "io.h"
#include "log.h"
#include "system.h"

#define DEC2BCD(x) ( ((x/10)<<4) | (x % 10) )

#define BS_STATUSB_BCDBIN 2
#define BS_STATUSB_24H    1

typedef struct {
  uint8 indexreg;

  uint8 resetcode;

  struct {
    int divider;
    int rate;

    uint8 statusB;
    uint8 statusC;
    uint8 statusD;
  } rtc;

  sys_t *pSystemCtx;
} ctx_t;

/*
 * Gets the current (local) time for RTC representation
 */
static struct tm *getTime(void) {
  time_t rawtime;
  struct tm * timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  return timeinfo;
}

uint8 bin2bcd(ctx_t *pCtx,uint8 val) {
  if( pCtx->rtc.statusB & (1<<BS_STATUSB_BCDBIN) )
    return val;

  val = DEC2BCD(val);
  return val;
}

static uint8 cmos_inb(struct io_handler *hdl,uint16 port) {
  ctx_t *pCtx = (ctx_t*)hdl->opaque;
  uint8 ret;

  ASSERT(pCtx != NULL,"Called with NULL context");

  //LOGD("Read from port 0x%x (index reg = 0x%x)",port,pCtx->indexreg);

  if( port == 0x70 ) {
    return pCtx->indexreg;
  }

  if( pCtx->indexreg == 0x00 ) {
    struct tm *tiin = getTime();
    ret = tiin->tm_sec;
    //LOG("Returned SECOND=%u",ret);
    ret = bin2bcd(pCtx,ret);
    return ret;
  } else if( pCtx->indexreg == 0x01 ) { // Alarm - Second
    return 0xFF;
  } else if( pCtx->indexreg == 0x03 ) { // Alarm - Minute
    return 0xFF;
  } else if( pCtx->indexreg == 0x05 ) { // Alarm - Hour
    return 0xFF;
  } else if( pCtx->indexreg == 0x02 ) {
    struct tm *tiin = getTime();
    ret = tiin->tm_min;
    //LOG("Returned MINUTE=%u",ret);
    ret = bin2bcd(pCtx,ret);
    return ret;
  } else if( pCtx->indexreg == 0x04 ) {
    struct tm *tiin = getTime();
    ret = tiin->tm_hour;
    if( !(pCtx->rtc.statusB & (1<<BS_STATUSB_24H)) ) {
      if( ret > 12 ) {
        ret = 0x80 | bin2bcd(pCtx,ret-12);
        //LOG("Returned HOUR=%u",ret);
        return ret;
      }
    }
    ret = bin2bcd(pCtx,ret);
    //LOG("Returned HOUR=%u (statusB=0x%x)",ret,pCtx->rtc.statusB);
    return ret;
  } else if( pCtx->indexreg == 0x07 ) {
    struct tm *tiin = getTime();
    ret = tiin->tm_mday;
    //LOG("Returned DAY=%u",ret);
    ret = bin2bcd(pCtx,ret);
    return ret;
  } else if( pCtx->indexreg == 0x08 ) {
    struct tm *tiin = getTime();
    ret = tiin->tm_mon + 1;
    //LOG("Returned MONTH=%u",ret);
    ret = bin2bcd(pCtx,ret);
    return ret;
  } else if( pCtx->indexreg == 0x09 ) {
    struct tm *tiin = getTime();
    ret = tiin->tm_year;
    if( ret >= 100 ) ret -= 100;
    //LOG("Returned YEAR=%u",ret);
    ret = bin2bcd(pCtx,ret);
    return ret;    
  } else if( pCtx->indexreg == 0x0A ) { // Status register A (Time freq div/rate)
    ret = 0;

    pCtx->rtc.divider = 32768;
    pCtx->rtc.rate    = 1024;

    if( pCtx->rtc.rate == 1024 )
      ret |= 6;
    else
      ASSERT(0,"Lazy coder error");
    if( pCtx->rtc.divider == 32768 )
      ret |= 1<<5;
    else
      ASSERT(0,"Lazy coder error");

    return ret;

  } else if( pCtx->indexreg == 0x0B ) { // RTC Status reg B
    return pCtx->rtc.statusB;
  } else if( pCtx->indexreg == 0x0C ) { // RTC Status reg C
    return pCtx->rtc.statusC;
  } else if( pCtx->indexreg == 0x0D ) { // RTC Status reg D
    return pCtx->rtc.statusD;
  } else if( pCtx->indexreg == 0x0F ) { // Reset Code
    return pCtx->resetcode;
  } else if( pCtx->indexreg == 0x10 ) { // Installed floppy drives
    return (0x4<<4); // Master is 1.44MB 3.5"
    return 0; // None
  } else if( pCtx->indexreg == 0x12 ) { // Hard Disk Types
    return 0xF0; // HDD 0 = Type "47", HDD 1 = No drive
  } else if( pCtx->indexreg == 0x13 ) { // Typematic rate
    LOGW("----++++++++++++------------------++++++++++++++");
    return 0x00; // Disabled
  } else if( pCtx->indexreg == 0x17 ) { // Extended memory in K, Low byte
    return 0xFF;
  } else if( pCtx->indexreg == 0x18 ) { // Extended memory in K, High byte
    return 0xFF;
  } else if( pCtx->indexreg == 0x32 ) { // 
    struct tm *tiin = getTime();
    ret = tiin->tm_year;
    if( ret >= 100 ) ret = 1;
    else ret = 0;
    LOG("Returned CENTURY=%u",ret);
    ret = bin2bcd(pCtx,ret);
    return ret;    
  } else if( pCtx->indexreg == 0x34 ) { // ExtMem Low Byte
    uint64 rs;

    if( pCtx->pSystemCtx->ramsize > 0xE0000000 ) {
      rs = 0xE0000000;
    } else {
      rs = pCtx->pSystemCtx->ramsize;
    }
    rs -= 16LL * 1024LL * 1024LL;
    return (rs>>16) & 0xFF;
  } else if( pCtx->indexreg == 0x35 ) { // ExtMem High Byte
    uint64 rs;
    if( pCtx->pSystemCtx->ramsize > 0xE0000000 ) {
      rs = 0xE0000000;
    } else {
      rs = pCtx->pSystemCtx->ramsize;
    }
    rs -= 16LL * 1024LL * 1024LL;
    return (rs>>24) & 0xFF;
  } else if( pCtx->indexreg == 0x38 ) { // Boot flags #1
    return 1; // ???
  } else if( pCtx->indexreg == 0x39 ) { // Emulator disk translation
    return 1; // (0 = CHS, 1 = LBA)
  } else if( pCtx->indexreg == 0x3D ) { // Boot flags #2
    //return 1; // Boot drive 1 = FDD, Boot drive 2 = none
    return 2; // Boot drive 1 = HDD, Boot drive 2 = none
  } else if( pCtx->indexreg == 0x5b ) { // HighMem (>4GB) Low byte
    uint64 rs;

    if( pCtx->pSystemCtx->ramsize <= 0xE0000000 )
      rs = 0;
    else
      rs = pCtx->pSystemCtx->ramsize - 0xE0000000 - 0x100000;
    return (rs>>16) & 0xFF;
  } else if( pCtx->indexreg == 0x5c ) { // HighMem (>4GB) Mid byte
    uint64 rs;

    if( pCtx->pSystemCtx->ramsize <= 0xE0000000 )
      rs = 0;
    else
      rs = pCtx->pSystemCtx->ramsize - 0xE0000000 - 0x100000;
    return (rs>>24) & 0xFF;
  } else if( pCtx->indexreg == 0x5d ) { // HighMem (>4GB) High byte
    uint64 rs;

    if( pCtx->pSystemCtx->ramsize <= 0xE0000000 )
      rs = 0;
    else
      rs = pCtx->pSystemCtx->ramsize - 0xE0000000 - 0x100000;
    return (rs>>32) & 0xFF;
  } else if( pCtx->indexreg == 0x5f ) { // SMP Count
    return 0;
  } else {
    ASSERT(0,"Unimplemented CMOS read from reg 0x%02x",pCtx->indexreg);
  }
  return 0xFF;
}

static void  cmos_outb(struct io_handler *hdl,uint16 port,uint8 val) {
  ctx_t *pCtx = (ctx_t*)hdl->opaque;

  //LOG("Write to port 0x%x (val=0x%x,index reg=0x%x)",port,val,pCtx->indexreg);

  if( port == 0x00 ) {
    pCtx->indexreg = val & ~0x80; // Top bit determines if NMI is enabled or not
    return;
  }

  switch( pCtx->indexreg ) {
  case 0x0A:
    if( (val & 0x0F) == 0x06 ) pCtx->rtc.rate = 1024;
    else ASSERT(0,"Unsupported RTC rate");
    if( (val & 0x70) == 0x20 ) pCtx->rtc.divider = 32768;
    else ASSERT(0,"Unsupported RTC divider (val=0x%02x)",val);
    break;
  case 0x0B:
    pCtx->rtc.statusC = val;
    break;
  case 0x0F:
    pCtx->resetcode = val;
    break;
  case 0x00:
  case 0x02:
  case 0x04:
  case 0x07:
  case 0x08:
  case 0x09:
  case 0x17:
  case 0x32:
    // Skip...
    break;
  default:
    ASSERT(0,"Unhandled write to CMOS Data register (index reg = 0x%02x)",pCtx->indexreg);
  }
}

int x86_cmos_init(sys_t *pCtx) {
  static struct io_handler hdl;
  static ctx_t ctx;
  struct iohdl *h;

  memset(&ctx,0,sizeof(ctx_t));

  ctx.rtc.divider = 32768;
  ctx.rtc.rate    = 1024;
  ctx.rtc.statusD = 1<<7; // CMOS Battery good
  ctx.pSystemCtx  = pCtx;

  hdl.base   = 0x70;
  hdl.mask   = 0xFE;
  hdl.inb    = cmos_inb;
  hdl.outb   = cmos_outb;
  hdl.opaque = &ctx;
  h = io_register_handler(&hdl);

  io_remap_handler(h,&hdl);

  return 0;
}
