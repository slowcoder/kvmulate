#pragma once

#include "types.h"

typedef struct mmio_device {
  uint32 base,mask;
  void  *opaque;

  uint8  (*readb )(struct mmio_device *,uint64 address);
  uint16 (*readw )(struct mmio_device *,uint64 address);
  uint32 (*readl )(struct mmio_device *,uint64 address);
  uint64 (*readq )(struct mmio_device *,uint64 address);
  void   (*writeb)(struct mmio_device *,uint64 address,uint8 val);
  void   (*writew)(struct mmio_device *,uint64 address,uint16 val);
  void   (*writel)(struct mmio_device *,uint64 address,uint32 val);
  void   (*writeq)(struct mmio_device *,uint64 address,uint64 val);
} mmio_device_t;

int   mmio_init(void);

void *mmio_register_device(mmio_device_t *pDev);
int   mmio_remap_device(void *pHandle,mmio_device_t *pDev);

int   mmio_write(uint64 address,uint32 len,uint8 *pData);
int   mmio_read(uint64 address,uint32 len,uint8 *pData);
