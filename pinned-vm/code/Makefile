BOOTLOADER = my-install

# PROGS := tests/1-test-basic-tutorial.c
# PROGS += tests/1-test-one-addr.c
# PROGS += tests/1-test-two-addr.c
# PROGS += tests/2-test-lookup.c
# PROGS += tests/2-test-procmap.c
# PROGS += tests/my_test.c
PROGS += tests/catch_uart.c
# PROGS += tests/put8_test.c

# your code will go here.
COMMON_SRC += pinned-vm.c
COMMON_SRC += mmu.c
COMMON_SRC += your-mmu-asm.S

O = $(CS140E_2025_PATH)/libpi
STAFF_OBJS += $(O)/staff-objs/staff-full-except.o
STAFF_OBJS += $(O)/staff-objs/staff-full-except-asm.o
STAFF_OBJS += $(O)/staff-objs/staff-switchto-asm.o
# STAFF_OBJS += staff-mmu-asm.o  
# STAFF_OBJS += staff-mmu-except.o  
# STAFF_OBJS += staff-mmu.o  
# STAFF_OBJS += staff-pinned-vm.o
STAFF_OBJS += $(O)/staff-objs/kmalloc.o

# define this if you need to give the device for your pi
TTYUSB = 

# set RUN = 1 if you want the code to automatically run after building.
RUN = 1

include $(CS140E_2025_PATH)/libpi/mk/Makefile.robust

clean::
	make -C tests clean
