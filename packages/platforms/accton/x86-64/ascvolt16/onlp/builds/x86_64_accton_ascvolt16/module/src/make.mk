###############################################################################
#
# 
#
###############################################################################

LIBRARY := x86_64_accton_ascvolt16
$(LIBRARY)_SUBDIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(BUILDER)/lib.mk
