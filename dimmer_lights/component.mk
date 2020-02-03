# Component makefile for dimmer_lights


INC_DIRS += $(dimmer_lights_ROOT)


dimmer_lights_INC_DIR = $(dimmer_lights_ROOT)
dimmer_lights_SRC_DIR = $(dimmer_lights_ROOT)

$(eval $(call component_compile_rules,dimmer_lights))
