CC := gcc
CFLAGS := -Wall -g -Iinclude/ `pkg-config --cflags sdl`

OBJS := main.o log.o \
	system.o \
	io.o \
	mmio.o \
	irq.o \
	devices/devices.o \
	devices/x86/cmos.o \
	devices/x86/ps2_pos.o \
	devices/x86/bios_debug.o \
	devices/x86/i8237-dma.o \
	devices/x86/pci.o \
	devices/x86/qemu_paravirt.o \
	devices/x86/i8042.o \
	devices/x86/serialport.o \
	devices/x86/parallelport.o \
	devices/x86/vga/vga.o \
	devices/x86/vga/registers.o \
	devices/x86/vga/ram.o \
	devices/x86/vga/rom.o \
	devices/x86/vga/pci.o \
	devices/x86/vga/viewer.o \
	devices/x86/floppy.o \
	devices/x86/ata/controller.o

DEPS := $(subst .o,.d,$(OBJS))
SRCS := $(subst .o,.c,$(OBJS))

all: kvmulate

kvmulate: $(OBJS)
	$(CC) $(CFLAGS) -lSDL -pthread -o $@ $^ `pkg-config --libs sdl`

%.o: %.c
	@echo "CC       $<"
	@$(CC) -MM -MF $(subst .o,.d,$@) -MP -MT "$@ $(subst .o,.d,$@)" $(CFLAGS) $<
	@$(CC) -c -o $@ $(CFLAGS) $<

clean:
	@echo "CLEAN"
	@$(RM) -r $(OBJS) $(DEPS) kvmulate boot.bin

.PHONY: clean

ifneq ("$(MAKECMDGOALS)","clean")
-include $(DEPS)
endif
