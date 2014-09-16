#include <stdio.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

#include "types.h"
#include "irq.h"
#include "log.h"
#include "system.h"

static sys_t *pCtx = NULL;

int irq_init(sys_t *pInCtx) {

  pCtx = pInCtx;

  return 0;
}

int irq_set(int irq,int level) {
#if 0
  {
    int r;
    struct kvm_interrupt sirq;

    sirq.irq = irq;

    r = ioctl(vcpufd,KVM_INTERRUPT,&sirq);
    if( r != 0 ) {
      LOG("Failed to inject IRQ %i (fd=%i)",sirq,kvmfd);
    }
  }
#else
  //KVM_IRQ_LINE
  {
    int r;
    struct kvm_irq_level lvl;

    lvl.irq = irq;
    lvl.level = level;

    r = ioctl(pCtx->vmfd,KVM_IRQ_LINE,&lvl);
    if( r != 0 ) {
      LOGE("Failed to set IRQ level to %i for IRQ %i",level,irq);
      perror("KVM_IRQ_LINE");
      return -1;
    }
    return 0;
  }
#endif
}


