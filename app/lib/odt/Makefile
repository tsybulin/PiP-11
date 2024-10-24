#
# Makefile
#

CIRCLEHOME = ../../..

OBJS = odt.o

libodt.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
