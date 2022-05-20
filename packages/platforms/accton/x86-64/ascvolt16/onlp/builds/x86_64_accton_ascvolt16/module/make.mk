###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_accton_ascvolt16_INCLUDES := -I $(THIS_DIR)inc
x86_64_accton_ascvolt16_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_accton_ascvolt16_DEPENDMODULE_ENTRIES := init:x86_64_accton_ascvolt16 ucli:x86_64_accton_ascvolt16

