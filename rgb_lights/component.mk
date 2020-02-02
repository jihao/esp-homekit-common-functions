# Component makefile for rgb_lights


INC_DIRS += $(rgb_lights_ROOT)


rgb_lights_INC_DIR = $(rgb_lights_ROOT)
rgb_lights_SRC_DIR = $(rgb_lights_ROOT)

$(eval $(call component_compile_rules,rgb_lights))
