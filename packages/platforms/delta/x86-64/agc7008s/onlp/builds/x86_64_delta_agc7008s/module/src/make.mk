###############################################################################
#
# 
#
###############################################################################


LIBRARY := x86_64_delta_agc7008s
$(LIBRARY)_SUBDIR := $(dir $(lastword $(MAKEFILE_LIST)))
#$(LIBRARY)_LAST := 1
include $(BUILDER)/lib.mk
