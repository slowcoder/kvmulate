#include "devices.h"
#include "system.h"

int x86_cmos_init(sys_t *pCtx);
int x86_ps2_POS_init(void);
int x86_biosdebug_init(void);
int x86_i8237_dma_init(uint16 base);
int x86_pci_init(void);
int x86_qemuparavirt_init(sys_t *pCtx);
int x86_vga_init(sys_t *pSys);
int x86_keyboard_init(void);
int irq_init(sys_t *pCtx);
int x86_parallelport_init(void);
int x86_serialport_init(void);
int x86_ata_init(sys_t *pSys);
int x86_floppy_init(sys_t *pSys);

int devices_register_all(sys_t *pSys) {

  irq_init(pSys);

  x86_cmos_init(pSys);
  x86_ps2_POS_init();          // IO, Port 0x92
  x86_biosdebug_init();        // IO, Port 0x402
  x86_i8237_dma_init(0x00);    // IO, Port 0x00 -> 0x0F
  //x86_i8237_dma_init(0xC0);    // IO, Port 0xC0 -> 0xCF
  x86_pci_init();              // IO, Port 0xCF8, 0xCFC
  x86_qemuparavirt_init(pSys); // IO, Port 0x510,0x511
  x86_keyboard_init();
  x86_parallelport_init();
  x86_serialport_init();

  x86_vga_init(pSys);

  x86_floppy_init(pSys);
  x86_ata_init(pSys);

  return 0;
}
