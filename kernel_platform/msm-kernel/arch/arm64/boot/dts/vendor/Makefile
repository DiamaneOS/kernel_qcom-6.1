vendor := $(srctree)/$(src)
# if build for fps, skip qcom folder
ifndef CONFIG_ARCH_FPSPRING
ifneq "$(wildcard $(vendor)/qcom)" ""
	subdir-y += qcom
endif
else
ifneq "$(wildcard $(vendor)/fps_overlay)" ""
	subdir-y += fps_overlay
endif
endif
