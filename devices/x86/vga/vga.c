#include "io.h"
#include "mmio.h"
#include "system.h"
#include "devices/x86/vga/vga.h"
#include "log.h"
#include "utils.h"

int x86_vga_pci_init(sys_t *pCtx);
int x86_vga_rom_init(sys_t *pCtx);
int x86_vga_ram_init(sys_t *pCtx,vgactx_t *pVGA);
int x86_vga_registers_init(sys_t *pCtx,vgactx_t *pVGA);
int x86_vga_viewer_init(vgactx_t *pVGA);

int x86_vga_init(sys_t *pCtx) {
  vgactx_t *pVGA;

  pVGA = (vgactx_t*)calloc(1,sizeof(vgactx_t));

  x86_vga_pci_init(pCtx);
  x86_vga_rom_init(pCtx);
  x86_vga_ram_init(pCtx,pVGA);
  x86_vga_registers_init(pCtx,pVGA);

  x86_vga_viewer_init(pVGA);

  return 0;
}
