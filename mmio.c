#include "mmio.h"
#include "log.h"

typedef struct mmiolist {
  mmio_device_t   *pDev;
  struct mmiolist *pNext;
} mmiolist_t;

static mmiolist_t *pHead = NULL;

static mmio_device_t *getDevice(uint64 addr) {
  mmiolist_t *pCurr;
  
  pCurr = pHead;
  while(pCurr != NULL) {

    if( pCurr->pDev->base != 0 ) {
#if 0
      LOG(" (0x%08x & 0x%08x) == 0x%08x ",
	  addr,pCurr->pDev->mask,pCurr->pDev->base);
#endif
      if( (addr & pCurr->pDev->mask) == pCurr->pDev->base )
        return pCurr->pDev;
    }

    pCurr = pCurr->pNext;
  }

  return NULL;
}

int   mmio_init(void) {
  pHead = NULL;

  return 0;
}

void *mmio_register_device(mmio_device_t *pDev) {
  mmiolist_t *pCurr;

  if( getDevice(pDev->base) != NULL ) {
    mmio_device_t *pPrev;

    LOG("Asked to register for an already registered MMIO address");
    LOG("pDev->base=0x%x",pDev->base);
    pPrev = getDevice(pDev->base);
    LOG("Already had: 0x%x (Mask 0x%x)",pPrev->base,pPrev->mask);
    //return NULL;
  }

  LOG("Registering MMIO handler for 0x%08x (Mask 0x%x)",
      pDev->base,pDev->mask);

  if( pHead == 0 ) {
    pHead = (mmiolist_t*)calloc(1,sizeof(mmiolist_t));
    pHead->pDev = pDev;
    return pHead;
  }

  pCurr = pHead;
  while(pCurr->pNext != NULL) pCurr = pCurr->pNext;

  pCurr->pNext = (mmiolist_t*)calloc(1,sizeof(mmiolist_t));
  pCurr->pNext->pDev = pDev;

  return pCurr->pNext;
}

int   mmio_remap_device(void *pHandle,mmio_device_t *pDev) {
  ASSERT(0,"Not implemented");
  return -1;
}

int mmio_write(uint64 address,uint32 len,uint8 *pData) {
  mmio_device_t *pDev;

  //LOG("write - address=0x%x,len=%u",address,len);

  pDev = getDevice(address);
  if( pDev == NULL ) return -1;

  switch(len) {
  case 1:
    ASSERT(pDev->writeb != NULL,"NULL handler for MMIO writeb to 0x%x",address);
    pDev->writeb(pDev,address,*(uint8*)pData);
    break;
  case 2:
    ASSERT(pDev->writew != NULL,"NULL handler for MMIO writew to 0x%x",address);
    pDev->writew(pDev,address,*(uint16*)pData);
    break;
  case 4:
    ASSERT(pDev->writel != NULL,"NULL handler for MMIO writel to 0x%x",address);
    pDev->writel(pDev,address,*(uint32*)pData);
    break;
  case 8:
    ASSERT(pDev->writeq != NULL,"NULL handler for MMIO writeq to 0x%x",address);
    pDev->writeq(pDev,address,*(uint64*)pData);
    break;
  default:
    ASSERT(0,"Non natural aligned write?");
  }

  return 0;
}

int mmio_read(uint64 address,uint32 len,uint8 *pData) {
  mmio_device_t *pDev;

  //LOG("read - address=0x%x,len=%u",address,len);

  pDev = getDevice(address);
  if( pDev == NULL ) return -1;

  switch(len) {
  case 1:
    ASSERT(pDev->readb != NULL,"NULL handler for MMIO readb @ 0x%x",address);
    *(uint8*)pData = pDev->readb(pDev,address);
    break;
  case 2:
    ASSERT(pDev->readw != NULL,"NULL handler for MMIO readw");
    *(uint16*)pData = pDev->readw(pDev,address);
    break;
  case 4:
    ASSERT(pDev->readl != NULL,"NULL handler for MMIO readl");
    *(uint32*)pData = pDev->readl(pDev,address);
    break;
  case 8:
    ASSERT(pDev->readq != NULL,"NULL handler for MMIO readq @ 0x%llx",address);
    *(uint32*)pData = pDev->readq(pDev,address);
    break;
  default:
    ASSERT(0,"Non natural aligned read? (len=%i)",len);
  }

  return 0;
}
