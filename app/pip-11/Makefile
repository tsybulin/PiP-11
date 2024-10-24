#
# Makefile
#

CIRCLEHOME = ../..

OBJS	= main.o kernel.o ini.o mcore.o firmware.o api.o

LIBS	= $(CIRCLEHOME)/lib/libcircle.a \
          $(CIRCLEHOME)/lib/usb/libusb.a \
          $(CIRCLEHOME)/lib/input/libinput.a \
          $(CIRCLEHOME)/lib/fs/libfs.a \
    	  $(CIRCLEHOME)/app/lib/fatfs/libfatfs.a \
          $(CIRCLEHOME)/app/lib/sdcard/libsdcard.a \
          $(CIRCLEHOME)/lib/sched/libsched.a \
          $(CIRCLEHOME)/lib/net/libnet.a \
          $(CIRCLEHOME)/app/lib/util/libutil.a \
          $(CIRCLEHOME)/app/lib/cons/libcons.a \
          $(CIRCLEHOME)/app/lib/odt/libodt.a \
          $(CIRCLEHOME)/app/lib/arm11/libarm11.a


include ../Rules.mk

-include $(DEPS)
