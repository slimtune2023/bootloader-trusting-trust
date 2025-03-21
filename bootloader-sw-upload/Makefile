# NOTE: "make check" is extremely weak for today --- 
# we don't know what is on your microsd so can't check
# in general.  its mainly useful so you can run
# a bunch of tests automatically and get notified if
# something failed.
#
# when developing: you should probably run "make" 
# and look at the output to make sure it makes sense.

LIBS += $(CS140E_2025_PATH)/lib/libgcc.a

COMMON_SRC += mbr.c
COMMON_SRC += fat32.c
COMMON_SRC += nrf-hw-support.c nrf-public.c nrf-driver.c

STAFF_OBJS += $(CS140E_2025_PATH)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(CS140E_2025_PATH)/libpi/staff-objs/kmalloc.o 

# the tests in decreasing order of difficulty.
# PROGS := boot-sw-upload.c
# PROGS += boot-sw-upload-led.c
PROGS += expose-fs.c

CFLAGS_EXTRA  = -Iexternal-code

# a list of all of your object files.
COMMON_SRC += pi-sd.c mbr-helpers.c fat32-helpers.c fat32-lfn-helpers.c external-code/unicode-utf8.c external-code/emmc.c#  external-code/mbox.c 

# external-code/bzt-sd.c 

TTYUSB = 
BOOTLOADER = my-install
RUN = 0

EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|PASS:\|TEST:'
include $(CS140E_2025_PATH)/libpi/mk/Makefile.robust

checkoff:
	make -C ./ "PROGS=$(ALL_PROGS)"

clean::
	rm -f tests/*.list tests/*.o tests/*.elf tests/*~ tests/*.bin
	make -C external-code clean
