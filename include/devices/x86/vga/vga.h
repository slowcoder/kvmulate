#pragma once

#include "types.h"

typedef struct {
  uint8 misc_output_reg;
  uint8 mode_select_reg;

  int   seq_ndx;
  uint8 seq_reg[5];

  uint8 attr_ndx;
  uint8 attr_reg[0x15];

  int   gcont_ndx;
  uint8 gcont_reg[9];

  uint8 crtc_ndx;
  uint8 crtc_reg[0x19];

  uint16 pel_write_addr;
  uint16 pel_read_addr;
  uint8  pel[256*3];
  uint8  palette[32];

  uint8 *pVRAM[6]; // 6 for now. B0000/B8000 overlaps one of the A0000 pages...
} vgactx_t;
