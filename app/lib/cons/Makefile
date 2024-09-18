#
# Makefile
#

CIRCLEHOME = ../../..

OBJS = keyboard.o vt.o cons.o

libcons.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
