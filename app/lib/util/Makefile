#
# Makefile
#

CIRCLEHOME = ../../..

OBJS = queue.o

libutil.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
