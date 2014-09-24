#include <unistd.h>
#include <pthread.h>
#include <SDL/SDL.h>

#include "devices/x86/vga/vga.h"
#include "devices/x86/vga/font.h"
#include "log.h"

static SDL_Surface *pScreen = NULL;

#if 1
static void dump_info(vgactx_t *pVGA) {
  if( (pVGA->gcont_reg[0x06] & 1) == 0 ) {
    LOG("Text mode");
  } else {
    uint16 vtot,vrs,vde;
    uint16 ehd;
    uint8  cw;

    //return;
    LOG("Graphics mode");

    // Vertical total
    vtot = (((pVGA->crtc_reg[0x07]>>5)&1)<<9) |
      (((pVGA->crtc_reg[0x07]>>0)&1)<<8) |
      pVGA->crtc_reg[0x06];
    LOG("Vertical Total: %d",vtot);

    // Vertical Retrace Start
    vrs = (((pVGA->crtc_reg[0x07]>>7)&1)<<9) |
      (((pVGA->crtc_reg[0x07]>>2)&1)<<8) |
      pVGA->crtc_reg[0x10];
    LOG("Vertical retrace start: %i",vrs);

    vde = (((pVGA->crtc_reg[0x07]>>6)&1)<<9) |
      (((pVGA->crtc_reg[0x07]>>1)&1)<<8) |
      pVGA->crtc_reg[0x12];
    LOG("Vertical display end: %i",vde);

    ehd = pVGA->crtc_reg[0x01];
    LOG("End horizontal display: %i (in character clocks)",ehd);

    // Character width
    cw = 9 - (pVGA->seq_reg[0x01]&1);

    LOG("Assumed resolution: %i x %i",(ehd+1) * cw,vde + 1);

    LOG("Chain-4: %i",(pVGA->seq_reg[0x04]>>3)&1);
    LOG("O/E Dis: %i",(pVGA->seq_reg[0x04]>>2)&1);
    LOG("ExtMem: %i",(pVGA->seq_reg[0x04]>>1)&1);

    LOG("Shift256 : %i",(pVGA->gcont_reg[0x05]>>6)&1);
    LOG("ShiftReg : %i",(pVGA->gcont_reg[0x05]>>5)&1);
    LOG("ReadMode : %i",(pVGA->gcont_reg[0x05]>>3)&1);
    LOG("WriteMode: %i",(pVGA->gcont_reg[0x05]>>0)&3);

    LOG("DotClockRate: %i",(pVGA->seq_reg[0x01]>>3)&1);
    LOG("ShiftLoadRate: %i",(pVGA->seq_reg[0x01]>>2)&1);
    LOG("SeqReg 0x01: %i",pVGA->seq_reg[0x01]);

    LOG("CRTC SD: %i",(pVGA->crtc_reg[0x09]>>7)&1);
    LOG("CRTC MCR: 0x%02x",pVGA->crtc_reg[0x17]);

    LOG("StartLow: 0x%x",pVGA->crtc_reg[0xD]);
    LOG("StartHig: 0x%x",pVGA->crtc_reg[0xC]);
  }
}
#endif

static int isTextMode(vgactx_t *pVGA) {
  if( (pVGA->gcont_reg[0x06] & 1) == 0 ) {
    return 1;
  }
  return 0;
}

static void drawGlyphAt(vgactx_t *pVGA,uint8 ch,uint8 attr,int sx,int sy) {
  uint32  pitch;
  uint32 *pFB;
  uint32  fgcol,bgcol;
  int     x,y;
  int     col;

  // Figure out where in the LFB we're starting
  pitch = pScreen->pitch / 4;
  pFB = pScreen->pixels;
  pFB += (sy*16) * pitch;
  pFB += sx * 8;

  // Get the 32bit ARGB values corresponding to fg/bg colors
  col = (attr & 0x0F) * 3;
  fgcol = (pVGA->pel[col + 0] << 18) |
    (pVGA->pel[col + 1] << 10) |
    (pVGA->pel[col + 2] << 2);

  col = ((attr>>4) & 0x0F) * 3;
  bgcol = (pVGA->pel[col + 0] << 18) |
    (pVGA->pel[col + 1] << 10) |
    (pVGA->pel[col + 2] << 2);

  // Draw the glyph
  for(y=0;y<16;y++) {
    for(x=0;x<8;x++) {
      if( font8x16[ch][y] & (0x80>>x) )
        pFB[x] = fgcol;
      else
        pFB[x] = bgcol;
    }
    
    pFB += pitch;
  }
}

static void drawCursor(vgactx_t *pVGA) {
  int location,sl_start,sl_end;
  uint32 *pFB,pitch;
  int y,x,cx,cy;

  // Figure out the location of the cursor
  location = (pVGA->crtc_reg[0xE]<<8) + pVGA->crtc_reg[0xF];
  sl_start = pVGA->crtc_reg[0xA] & 0x1F;
  sl_end = pVGA->crtc_reg[0xB] & 0x1F;

  location -= (pVGA->crtc_reg[0xC] << 8) | pVGA->crtc_reg[0xD];

  cy = location / 80;
  cx = location % 80;

  pitch = pScreen->pitch / 4;
  pFB = pScreen->pixels;
  pFB += (cy*16) * pitch;
  pFB += cx * 8;
  pFB += pitch * sl_start;

  for(y=sl_start;y<sl_end;y++) {
    for(x=0;x<8;x++) {
      //pFB[x] = fgcol;
      pFB[x] = 0xFFFFFF;
    }
    pFB += pitch;
  }
}

static void updateTextmode(vgactx_t *pVGA) {
  int x,y;
  uint8 ch,attr;
  uint32 vram_off;

  vram_off = (pVGA->crtc_reg[0xC] << 8) | pVGA->crtc_reg[0xD];
  vram_off *= 2;

  for(y=0;y<25;y++) {
    for(x=0;x<80;x++) {
      ch   = pVGA->pVRAM[0][0x18000 + vram_off + (y*80+x)*2+0];
      attr = pVGA->pVRAM[0][0x18000 + vram_off + (y*80+x)*2+1];

      drawGlyphAt(pVGA,ch,attr,x,y);
    }
  }

  if( x == 128 )
  drawCursor(pVGA);
}

int updateGraphics(vgactx_t *pVGA) {
  int y,x,w,h;
  int o;
  uint16 ehd,vde;
  uint8  cw;
  uint32 *pFB;

  cw = 9 - (pVGA->seq_reg[0x01]&1);
  ehd = pVGA->crtc_reg[0x01];
  vde = (((pVGA->crtc_reg[0x07]>>6)&1)<<9) |
    (((pVGA->crtc_reg[0x07]>>1)&1)<<8) |
    pVGA->crtc_reg[0x12];

  w = (ehd+1) * cw;
  h = vde + 1;

  o = (pVGA->crtc_reg[0xC] << 8) | pVGA->crtc_reg[0xD];

  pFB = pScreen->pixels;

  w = 320;
//  h = 400;
  h = 200;
  for(y=0;y<h;y++) {
    for(x=0;x<w;x++) {
      uint8 palndx;
      //palndx = *(uint8*)( pVGA->pVRAM[x&3] + (((y*w)+x)/4) + o );
      palndx = *(uint8*)( pVGA->pVRAM[x&3] + (((y*w)+x)/1) + o );

      pFB[y*640+x] = (pVGA->pel[palndx*3+0]<<18) |
                    	(pVGA->pel[palndx*3+1]<<10) |
                    	(pVGA->pel[palndx*3+2]<<2);
    }
  }

  return 0;
}

//void kbd_inject(int bIsPressed);
void kbd_inject(uint8 sc,int bIsPressed);

typedef struct {
  SDLKey key;
  uint8   sc;
} kssc_t;

static kssc_t kssc[] = { { SDLK_1,  0x02 },
			 { SDLK_2,  0x03 },
			 { SDLK_3,  0x04 },
			 { SDLK_4,  0x05 },
			 { SDLK_5,  0x06 },
			 { SDLK_6,  0x07 },
			 { SDLK_7,  0x08 },
			 { SDLK_8,  0x09 },
			 { SDLK_9,  0x0A },
			 { SDLK_0,  0x0B },
			 { SDLK_a,  0x1E },
			 { SDLK_b,  0x30 },
			 { SDLK_c,  0x2E },
			 { SDLK_d,  0x20 },
			 { SDLK_e,  0x12 },
			 { SDLK_f,  0x21 },
			 { SDLK_g,  0x22 },
			 { SDLK_h,  0x23 },
			 { SDLK_i,  0x17 },
			 { SDLK_j,  0x24 },
			 { SDLK_k,  0x25 },
			 { SDLK_l,  0x26 },
			 { SDLK_m,  0x32 },
			 { SDLK_n,  0x31 },
			 { SDLK_o,  0x18 },
			 { SDLK_p,  0x19 },
			 { SDLK_q,  0x10 },
			 { SDLK_r,  0x13 },
			 { SDLK_s,  0x1F },
			 { SDLK_t,  0x14 },
			 { SDLK_u,  0x16 },
			 { SDLK_v,  0x2F },
			 { SDLK_w,  0x11 },
			 { SDLK_x,  0x2D },
			 { SDLK_y,  0x15 },
			 { SDLK_z,  0x2C },
			 { SDLK_F1, 0x3B },
			 { SDLK_F2, 0x3C },
			 { SDLK_F3, 0x3D },
			 { SDLK_F4, 0x3E },
			 { SDLK_F5, 0x3F },
			 { SDLK_F6, 0x40 },
			 { SDLK_F7, 0x41 },
			 { SDLK_F8, 0x42 },
			 { SDLK_F9, 0x43 },
			 { SDLK_F10,0x44 },
			 { SDLK_RETURN, 0x1C },
			 { SDLK_SPACE , 0x39 },
			 { SDLK_LALT  , 0x38 },
			 { SDLK_LSHIFT, 0x2A },
			 { SDLK_LCTRL , 0x1D },
			 { SDLK_ESCAPE, 0x01 },
			 { SDLK_TAB   , 0x0F },
			 { SDLK_BACKSPACE,0x0E },
			 { SDLK_DOWN  , 0x50 },
			 { SDLK_UP    , 0x48 },
			 { SDLK_LEFT  , 0x4B },
			 { SDLK_RIGHT , 0x4D },
			 { SDLK_COMMA , 0x33 },
       { SDLK_PERIOD, 0x34 },
       { SDLK_SEMICOLON, 0x27 },
			 { 0,0 } };

int keysym_to_scancode(SDL_keysym *pSym,uint8 *pSC) {
  int i;

  i = 0;
  while( kssc[i].key != 0 ) {
    if( kssc[i].key == pSym->sym ) {
      *pSC = kssc[i].sc;
      return 0;
    }
    i++;
  }
  return -1;
}

static void *viewer_thread(void *pArg) {
  vgactx_t *pVGA = (vgactx_t*)pArg;
  SDL_Event ev;
  int bDone = 0;


  LOG("pVRAM[0] = %p",pVGA->pVRAM[0]);

  pScreen = SDL_SetVideoMode(640,400,32,SDL_HWSURFACE | SDL_DOUBLEBUF);
  SDL_WM_SetCaption("KVMulate","KVMulate");

  while(!bDone) {

    //dump_info(pVGA);
    if( isTextMode(pVGA) ) {
      updateTextmode(pVGA);
    } else {
      updateGraphics(pVGA);
    }
    SDL_Flip(pScreen);

    while( SDL_PollEvent(&ev) != 0 ) {
      uint8 sc;

      switch(ev.type) {
      case SDL_KEYDOWN:
        if( ev.key.keysym.sym == SDLK_BACKQUOTE )
          dump_info(pVGA);
      	if( keysym_to_scancode(&ev.key.keysym,&sc) == 0 )
      	  kbd_inject(sc,1);
      	break;
      case SDL_KEYUP:
      	if( keysym_to_scancode(&ev.key.keysym,&sc) == 0 )
      	  kbd_inject(sc,0);
      	break;
      }
    }
    usleep(1000000LL / 60LL);
  }

  return NULL;
}

int x86_vga_viewer_init(vgactx_t *pVGA) {
  pthread_t thid;

  LOG("pVRAM[0] = %p",pVGA->pVRAM[0]);
  pthread_create(&thid,NULL,viewer_thread,pVGA);

  return 0;
}
