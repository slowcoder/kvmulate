#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "system.h"
#include "devices.h"
#include "pci.h"
#include "io.h"
#include "mmio.h"
#include "log.h"

/*
 * Maps memory located at saddr into the VM
 * at location daddr.
 * The size of the memory region is 'size'
 */
static int sys_map(sys_t *pCtx,uint64 daddr,void *saddr,uint64 size) {
  struct kvm_userspace_memory_region mem;

  pCtx->currmemslot++;

  mem.slot            = pCtx->currmemslot;
  mem.guest_phys_addr = daddr;
  mem.memory_size     = size;
  mem.userspace_addr  = (unsigned long)saddr;
  mem.flags           = 0;

  LOG("Mapping 0x%08llx (size 0x%08llx, host=0x%016llx)",
      mem.guest_phys_addr,mem.memory_size,mem.userspace_addr);

  if( ioctl(pCtx->vmfd, KVM_SET_USER_MEMORY_REGION, &mem) < 0 ) {
    LOG("KVM_SET_USER_MEMORY_REGION: %s",strerror(errno));
    return -1;
  }
  return 0;
}

static int checkExtensions(int fd) {
#if 0
  int r;

  r = ioctl(fd, KVM_CHECK_EXTENSION,KVM_CAP_SET_IDENTITY_MAP_ADDR);
  if( r != 0 )
    return -1;
#endif

  return 0;
}

static unsigned char *phys_lowmem[2] = {NULL,NULL};

void *sys_getLowMem(uint32 addr) {
  if( addr < 0xA0000 ) {
    LOGD("Foo!!!  %p",phys_lowmem[0]);
    return phys_lowmem[0] + addr;
  } else if( addr <= 0xFFFFF ) {
    return phys_lowmem[1] + (addr - 0xC0000);
  }

  ASSERT(0,"Request for LOWMEM (%u) either beyond 1M or inside ROM",addr);
  return NULL;
}

static int map_ram(sys_t *pCtx,uint32 uNumMegsRAM) {
  void *addr;
  uint64 ram_end;

  ram_end = (uint64)uNumMegsRAM * 1024LL * 1024LL;

  // Always map the first 1M
  addr = mmap(0, 0xA0000, PROT_EXEC | PROT_READ | PROT_WRITE,
	      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sys_map(pCtx,0x0,addr,0xA0000);
  phys_lowmem[0] = addr;

  addr = mmap(0, 0x20000, PROT_EXEC | PROT_READ | PROT_WRITE,
	      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sys_map(pCtx,0xC0000,addr,0x20000);
  phys_lowmem[1] = addr;

  if( ram_end <= 0xe0000000 ) {
    addr = mmap(0, ram_end - 0x100000, PROT_EXEC | PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sys_map(pCtx,0x100000,addr,ram_end - 0x100000);
  } else { 
    // More than 0xE0000000 bytes of RAM. Needs two mappings, so we can have
    // a hole in the top of the 4GB area for APICs, BIOS, PCI devices, etc.

    addr = mmap(0, 0xE0000000 - 0x100000, PROT_EXEC | PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sys_map(pCtx,0x100000,addr,0xE0000000 - 0x100000);

    addr = mmap(0, ram_end - 0xE0100000, PROT_EXEC | PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sys_map(pCtx,0x100000000,addr,ram_end - 0xE0100000);
  }

  pCtx->ramsize = ram_end;

  return 0;
}

static int load_bios_vga(sys_t *pCtx) {
  int in;
  int flen,mlen,r;
  void *tmp;

  in = open("vgabios.rom",O_RDONLY);
  if( in == -1 ) {
    LOG("Failed to open BIOS ROM file: %s",strerror(errno));
    return -1;
  }
  flen = lseek(in,0,SEEK_END);
  lseek(in,0,SEEK_SET);
  mlen = flen;
  if( flen & 0xFFF ) {
    mlen = flen & ~0xFFF;
    mlen += 0x1000;
  }

  ASSERT(mlen <= 0x20000,"File \"%s\" too big for BIOS area","seabios.rom");

  r = posix_memalign(&tmp,4096,0x20000);
  ASSERT(r == 0,"Failed to allocate aligned memory for BIOS");
  read(in,tmp,flen);

  sys_map(pCtx,0xC0000,tmp,0x20000);    // Legacy location
//  sys_map(pCtx,0xfffe0000,tmp,0x20000); // Shadow location

  // Can't free 'tmp' here, as it's now in use..
  // TODO
  // LEAK

  return 0;
}

static int load_bios(sys_t *pCtx) {
  int in;
  int flen,mlen,r;
  void *tmp;

  in = open("seabios.rom",O_RDONLY);
  if( in == -1 ) {
    LOG("Failed to open BIOS ROM file: %s",strerror(errno));
    return -1;
  }
  flen = lseek(in,0,SEEK_END);
  lseek(in,0,SEEK_SET);
  mlen = flen;
  if( flen & 0xFFF ) {
    mlen = flen & ~0xFFF;
    mlen += 0x1000;
  }

  ASSERT(mlen <= 0x20000,"File \"%s\" too big for BIOS area","seabios.rom");

  r = posix_memalign(&tmp,4096,0x20000);
  ASSERT(r == 0,"Failed to allocate aligned memory for BIOS");
  read(in,tmp,flen);

  sys_map(pCtx,0xe0000,tmp,0x20000);    // Legacy location
  sys_map(pCtx,0xfffe0000,tmp,0x20000); // Shadow location

  // Can't free 'tmp' here, as it's now in use..
  // TODO
  // LEAK

  load_bios_vga(pCtx);

  return 0;
}

static void fixupCPUID(sys_t *pCtx,int cpu) {
  vcpu_t *pCPU;

  pCPU = &pCtx->vcpu[cpu];

  memset(&pCPU->cpuid     ,0,sizeof(struct kvm_cpuid2));
  memset(&pCPU->cpuid_data,0,sizeof(struct kvm_cpuid_entry2)*100);

  pCPU->cpuid_data[0].function = 0x0;
  pCPU->cpuid_data[0].eax = 0x4;
  pCPU->cpuid_data[0].ebx = 0x756e6547; // 'Genu'
  pCPU->cpuid_data[0].edx = 0x49656e69; // 'ineI'
  pCPU->cpuid_data[0].ecx = 0x6c65746e; // 'ntel'
  pCPU->cpuid.nent++;

  pCPU->cpuid_data[1].function = 0x80000000;
  pCPU->cpuid_data[1].eax = 0x8000000a;
  pCPU->cpuid_data[1].ebx = 0x756e6547; // 'Genu'
  pCPU->cpuid_data[1].edx = 0x49656e69; // 'ineI'
  pCPU->cpuid_data[1].ecx = 0x6c65746e; // 'ntel'
  pCPU->cpuid.nent++;

  pCPU->cpuid_data[2].function = 0x80000002;
  pCPU->cpuid_data[2].eax = 0x554d4551; // 'QEMU'
  pCPU->cpuid_data[2].ebx = 0x72695620; // ' Vir'
  pCPU->cpuid_data[2].ecx = 0x6c617574; // 'tual'
  pCPU->cpuid_data[2].edx = 0x55504320; // ' CPU'
  pCPU->cpuid.nent++;

  pCPU->cpuid_data[3].function = 0x80000003;
  pCPU->cpuid_data[3].eax = 0x72657620; // ' ver'
  pCPU->cpuid_data[3].ebx = 0x6e6f6973; // 'sion' 
  pCPU->cpuid_data[3].ecx = 0x342e3120; // ' 1.4'
  pCPU->cpuid_data[3].edx = 0x302e;     // '.0'
  pCPU->cpuid.nent++;

  pCPU->cpuid_data[4].function = 0x80000001;
  pCPU->cpuid_data[4].eax = 0x623;
  pCPU->cpuid_data[4].ebx = 0x0;
  pCPU->cpuid_data[4].ecx = 0x1;
  pCPU->cpuid_data[4].edx = 0x2191abfd;
  pCPU->cpuid.nent++;

  pCPU->cpuid_data[5].function = 0x1;
  pCPU->cpuid_data[5].eax = 0x623;
  pCPU->cpuid_data[5].ebx = 0x800;
  pCPU->cpuid_data[5].ecx = 0x80802001;
  pCPU->cpuid_data[5].edx = 0x78bfbfd;
  pCPU->cpuid.nent++;

  if( ioctl(pCPU->fd, KVM_SET_CPUID2, &pCPU->cpuid) < 0 ) {
    ASSERT(0,"Failed to set CPUID2: %s",strerror(errno));
  }
}

struct sys *sys_create(uint32 uNumMegsRAM,uint8 uNumVCPUs) {
  sys_t *pSys = NULL;
  uint64 identity_base = 0xfffbc000;
  int cpu;
  int r;

  LOG("sizeof(unsigned long)==%i",sizeof(unsigned long));

  pSys = (sys_t*)calloc(1,sizeof(sys_t));
  pSys->vcpu = (vcpu_t*)calloc(uNumVCPUs,sizeof(vcpu_t));

  // Open KVM
  pSys->kvmfd = open("/dev/kvm",O_RDWR);
  if( pSys->kvmfd == -1) {
    LOG("Failed to open KVM interface: %s",strerror(errno));
    goto err_exit;
  }

  // Verify that header-files are up-to-date with the running kernel
  if( ioctl(pSys->kvmfd, KVM_GET_API_VERSION, NULL) != KVM_API_VERSION ) {
    LOG("KVM module version mismatched");
    goto err_exit;
  }
  
  // Create the VM
  pSys->vmfd = ioctl(pSys->kvmfd, KVM_CREATE_VM, NULL);
  if( pSys->vmfd < 0 ) {
    LOG("Error creating VM: %s",strerror(errno));
    goto err_exit;
  }

  if( checkExtensions(pSys->vmfd) != 0 ) {
    LOG("In-kernel KVM doesn't support all required extensions");
    goto err_exit;
  }

#if 0
  // Set EPT identity map
  identity_base = 0xfeffc000;
  r = ioctl(pSys->kvmfd, KVM_SET_IDENTITY_MAP_ADDR, &identity_base);
  if(r < 0) {
    LOG("Failed to set EPT identity map: %s",strerror(errno));
    goto err_exit;
  }
#endif
  // Set TSS base to one page after BIOS (or EPT if present)
  r = ioctl(pSys->vmfd, KVM_SET_TSS_ADDR, identity_base + 0x1000);
  if(r < 0) {
    LOG("Failed to set TSS base address: %s",strerror(errno));
    goto err_exit;
  }

  // Create in-kernel IRQ chips (2*PIC,LAPIC,IOAPIC)
  r = ioctl(pSys->vmfd,KVM_CREATE_IRQCHIP);
  if(r != 0) {
    LOG("Failed to create IRQCHIP: %s",strerror(errno));
    goto err_exit;
  }

  // Create in-kernel PIT chip
  r = ioctl(pSys->vmfd,KVM_CREATE_PIT);
  if(r != 0) {
    LOG("Failed to create PIT: %s",strerror(errno));
    goto err_exit;
  }

  // Create the VCPUs
  pSys->uNumVCPUs = uNumVCPUs;
  for(cpu=0;cpu<uNumVCPUs;cpu++) {
    void *pAddr;
    int   mmapsize;

    pSys->vcpu[cpu].id   = cpu;
    pSys->vcpu[cpu].stat = (struct kvm_run*)-1;

    pSys->vcpu[cpu].fd   = ioctl(pSys->vmfd,KVM_CREATE_VCPU,0);
    if( pSys->vcpu[cpu].fd < 0 ) {
      LOG("Failed to create VCPU: %s",strerror(errno));
      goto err_exit;
    }

    // Retrieve the size of per-VCPU state structure
    mmapsize = ioctl(pSys->kvmfd, KVM_GET_VCPU_MMAP_SIZE, (void*)0);
    if( mmapsize < 0 ) {
      LOG("Failed to get VCPU mmap size: %s",strerror(errno));
      goto err_exit;
    }

    pSys->vcpu[cpu].stat_sz = (size_t)mmapsize;

    // mmap the per-VCPU state structure onto our process space
    pAddr = mmap(0, pSys->vcpu[cpu].stat_sz, PROT_READ | PROT_WRITE, MAP_SHARED,
                pSys->vcpu[cpu].fd, 0);
    if( pAddr == (void*)-1 )
      goto err_exit;

    pSys->vcpu[cpu].stat = pAddr;

    fixupCPUID(pSys,cpu);
  }

  //irq_init(pSys);

  if( load_bios(pSys) != 0 ) {
    LOG("Error while loading BIOS");
    goto err_exit;
  }

  map_ram(pSys,uNumMegsRAM);

  mmio_init();
  devices_register_all(pSys);

  return pSys;

err_exit:
  return NULL;
}

void        sys_destroy(struct sys *pSys) {
  if( pSys->kvmfd > 0 ) close(pSys->kvmfd);
  if( pSys->vcpu != NULL ) free(pSys->vcpu);
  if( pSys != NULL ) free(pSys);
}

static int get_regs(sys_t *pCtx,uint8 cpu,
                    struct kvm_regs *regs, struct kvm_sregs *sregs) {
  if(ioctl(pCtx->vcpu[cpu].fd, KVM_GET_REGS, regs) < 0)
    return errno;
  if(ioctl(pCtx->vcpu[cpu].fd, KVM_GET_SREGS, sregs) < 0)
    return errno;

  return 0;
}

static inline int handle_io(struct kvm_run *pStat) {
  int c;

  if( pStat->io.direction == KVM_EXIT_IO_OUT ) {
    //LOG("IO_OUT Port=0x%x",pStat->io.port);
    for(c=0;c<pStat->io.count;c++) {
      if(        pStat->io.size == 1 ) {
        io_outb(pStat->io.port,*(uint8*)((uint8*)pStat + pStat->io.data_offset) );
      } else if( pStat->io.size == 2 ) {
        io_outw(pStat->io.port,*(uint16*)((uint8*)pStat + pStat->io.data_offset) );
      } else if( pStat->io.size == 4 ) {
        io_outl(pStat->io.port,*(uint32*)((uint8*)pStat + pStat->io.data_offset) );
      } else {
        ASSERT(0,"Unknown out-size %i",pStat->io.size);
      }
      pStat->io.data_offset += pStat->io.size;
    }
    return 0;
  } else { // KVM_EXIT_IO_IN
    //LOG("IO_IN Port=0x%x",pStat->io.port);
    for(c=0;c<pStat->io.count;c++) {
      if(        pStat->io.size == 1 ) {
        *(uint8*)( (uint8*)pStat + pStat->io.data_offset ) = io_inb(pStat->io.port);
      } else if( pStat->io.size == 2 ) {
        *(uint16*)( (uint8*)pStat + pStat->io.data_offset ) = io_inw(pStat->io.port);
      } else if( pStat->io.size == 4 ) {
        *(uint32*)( (uint8*)pStat + pStat->io.data_offset ) = io_inl(pStat->io.port);
      } else {
        ASSERT(0,"Unknown in-size %i",pStat->io.size);
      }
      pStat->io.data_offset += pStat->io.size;
    }
    return 0;
  }
  return -1;
}

static inline int handle_mmio(struct kvm_run *pStat) {
  int r;

  if( pStat->mmio.is_write ) {
    r = mmio_write(pStat->mmio.phys_addr,
                   pStat->mmio.len,
                   pStat->mmio.data);
  } else { // Is read
    r = mmio_read(pStat->mmio.phys_addr,
                  pStat->mmio.len,
                  pStat->mmio.data);
  }
  if( r != 0 ) {
    ASSERT(0,"Unhandled MMIO to 0x%llx (rip=0x%016llx)",pStat->mmio.phys_addr);
  }
  return 0;
}

static int sys_run_vcpu(sys_t *pCtx,uint8 cpu) {
  int bDone = 0;
  int r;

  LOG("Starting to run VCPU %d",cpu);

  while(!bDone) {
    struct kvm_run  *pStat;

    r = ioctl(pCtx->vcpu[cpu].fd, KVM_RUN, 0);
    if( r != 0 ) {
      struct kvm_regs   regs;
      struct kvm_sregs sregs;

      LOG("Failed to run VCPU%d: %s",cpu,strerror(r));
      get_regs(pCtx,cpu,&regs,&sregs);
      LOG("rip=0x%llx cs=0x%llx Exit_reason = %d",
          regs.rip,sregs.cs.base,pCtx->vcpu[cpu].stat->exit_reason);
      LOG(" rflags=0x%llx",regs.rflags);

      bDone = 1;
      break;
    }

    if(0){
      struct kvm_regs   regs;
      struct kvm_sregs sregs;

      get_regs(pCtx,cpu,&regs,&sregs);
      LOG("KVM_RUN returned. rip=0x%llx cs=0x%llx Exit_reason = %d",
          regs.rip,sregs.cs.base,pCtx->vcpu[cpu].stat->exit_reason);
      LOG(" rflags=0x%llx",regs.rflags);
      //return -1;
    }

    pStat = pCtx->vcpu[cpu].stat;
    if( pStat->exit_reason == KVM_EXIT_IO ) {
      //if( handle_pci(pStat) != 0 ) { // Not PCI, fallback to ISA
      	if( handle_io(pStat) != 0 ) {
      	  ASSERT(0,"KVM EXIT - IO (port=0x%x) - No handler",pStat->io.port);
      	}
      //}
    } else if( pStat->exit_reason == KVM_EXIT_DEBUG ) {
      ASSERT(0,"KVM EXIT - DEBUG");
    } else if( pStat->exit_reason == KVM_EXIT_MMIO ) {
      //if( handle_pci(pStat) != 0 ) {
      	if( handle_mmio(pStat) != 0 ) {
      	  ASSERT(0,"KVM EXIT - MMIO (Address=0x%x) - No handler",pStat->mmio.phys_addr);
      	}
      //}
    } else {
      LOG("Unhandled KVM exit reason %u on VCPU%i",pStat->exit_reason,cpu);
      LOG(" errno = %s",strerror(errno));
      ASSERT(0,"Aborting");
    }
  }
  return 0;
}

int         sys_run(struct sys *pCtx) {
  sys_run_vcpu(pCtx,0);

  return 0;
}
