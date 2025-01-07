#
# Makefile
#

CIRCLEHOME = ../../..

OBJS = arm11.o disasm.o dl11.o fp11.o kb11.o kl11.o kt11.o \
       kw11.o lp11.o pc11.o rk11.o rl11.o tc11.o unibus.o

libarm11.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
