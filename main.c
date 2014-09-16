#include <stdio.h>
#include <linux/kvm.h>

#include "log.h"
#include "system.h"

int main(void) {
  struct sys *pSystem;

  pSystem = sys_create(1024*8,1); // RAM, VCPUs
  ASSERT(pSystem != NULL,"Failed to create system");

  LOG("pSystem = %p",pSystem);

  sys_run(pSystem);

  sys_destroy(pSystem);

  return 0;
}
