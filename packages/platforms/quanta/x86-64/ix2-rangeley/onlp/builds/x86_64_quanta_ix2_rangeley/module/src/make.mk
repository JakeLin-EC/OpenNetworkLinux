###############################################################################
#
#
#
###############################################################################

LIBRARY := x86_64_quanta_ix2_rangeley
$(LIBRARY)_SUBDIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(BUILDER)/lib.mk