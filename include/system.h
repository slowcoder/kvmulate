#pragma once

#include <linux/kvm.h>
#include <sys/mman.h>
#include "types.h"


// Should go in internal header-file
typedef struct vcpu {
  int fd;
  int id;
  struct kvm_run *stat;
  size_t stat_sz;

#pragma pack(push,1)
  struct kvm_cpuid2 cpuid;
  struct kvm_cpuid_entry2 cpuid_data[100];
#pragma pack(pop)

} vcpu_t;

typedef struct sys {
  int     kvmfd; // FD to /dev/kvm
  int     vmfd;  // FD to the created VM
  uint8   uNumVCPUs;
  vcpu_t *vcpu;

  int     currmemslot;
  uint64  ramsize;
  void   *ram_low,*ram_high;
} sys_t;

struct sys *sys_create(uint32 uNumMegsRAM,uint8 uNumVCPUs);
void        sys_destroy(struct sys *pSys);
int         sys_run(struct sys *pSys);

void       *sys_getLowMem(uint32 addr);
