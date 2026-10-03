# add-overlay defines the target with following naming convention:
# <base>-<board>-dtbs = base.dtb board.dtbo
#
# Combined dtb target is also generated using the fdt_overlay tool.
# dtb-y += <base>-<board>.dtb

add-overlays = $(foreach o,$1,$(foreach b,$2,$(eval $(basename $b)-$(basename $o)-dtbs = $b $o) $(basename $b)-$(basename $o).dtb))

# pineapple-dtb-y is list of DTBs to install when doing non-overlay build
# pineapple-overlays-dtb-y is list of DTBs and DTBOs to install when doing overlay-supported build
# see DTB_TYPES from scripts/Makefile.dtbinst and install_dtbs from build.config.msm.common
#

PINEAPPLE_BASE_DTB += pineapple.dtb pineapple-v2.dtb
PINEAPPLE_APQ_BASE_DTB += pineapplep.dtb pineapplep-v2.dtb

PINEAPPLE_BOARDS += \
		 pineapple-mtp-overlay.dtbo \
		 pineapple-mtp-nfc-overlay.dtbo \
		 pineapple-cdp-overlay.dtbo \
		 pineapple-cdp-nfc-overlay.dtbo \
		 pineapple-qrd-overlay.dtbo \
		 pineapple-qrd-sku2-overlay.dtbo \
		 pineapple-rcm-overlay.dtbo \
		 pineapple-atp-overlay.dtbo \
		 pineapplep-hdk-overlay.dtbo \
		 pineapple-dpm-overlay.dtbo \
		 pineapplep-aim500-overlay.dtbo \
		 pineapplep-aim500-v2-overlay.dtbo

NOAPQ_PINEAPPLE_BOARDS += \
		 pineapple-rumi-overlay.dtbo

pineapple-dtb-$(CONFIG_ARCH_PINEAPPLE) += \
	$(call add-overlays, $(PINEAPPLE_BOARDS) $(NOAPQ_PINEAPPLE_BOARDS),$(PINEAPPLE_BASE_DTB))\
	$(call add-overlays, $(PINEAPPLE_BOARDS) $(APQ_PINEAPPLE_BOARDS),$(PINEAPPLE_APQ_BASE_DTB))
pineapple-overlays-dtb-$(CONFIG_ARCH_PINEAPPLE) += $(PINEAPPLE_BOARDS) $(NOAPQ_PINEAPPLE_BOARDS) $(PINEAPPLE_BASE_DTB) $(PINEAPPLE_APQ_BASE_DTB)

CLIFFS_BASE_DTB += cliffs.dtb cliffs7.dtb
CLIFFS_APQ_BASE_DTB += cliffsp.dtb cliffs7p.dtb

CLIFFS_BOARDS += \
		 cliffs-atp-overlay.dtbo \
		 cliffs-cdp-overlay.dtbo \
		 cliffs-mtp-overlay.dtbo \
		 cliffs-qrd-overlay.dtbo \
		 cliffs-rcm-overlay.dtbo \
		 cliffs-mtp-kiwi-2s-nfc-wcd9395-overlay.dtbo \
		 cliffs-mtp-pm8550b-overlay.dtbo \
		 cliffs-mtp-kiwi-2s-nfc-wcd9395-pm8550b-overlay.dtbo \
		 cliffs-mtp-kiwi-overlay.dtbo \
		 cliffs-mtp-peach-2s-nfc-wcd9395-overlay.dtbo \
		 cliffs-mtp-peach-2s-nfc-wcd9395-pm8550b-overlay.dtbo \
		 cliffs-mtp-peach-overlay.dtbo \
		 cliffs-rcm-qhdp-overlay.dtbo \
		 cliffs-rcm-fhdp-kiwi-overlay.dtbo \
		 cliffs-rcm-qhdp-kiwi-overlay.dtbo

NOAPQ_CLIFFS_BOARDS += \
		 cliffs-rumi-overlay.dtbo

pineapple-dtb-$(CONFIG_ARCH_CLIFFS) += \
	$(call add-overlays, $(CLIFFS_BOARDS) $(NOAPQ_CLIFFS_BOARDS),$(CLIFFS_BASE_DTB))\
	$(call add-overlays, $(CLIFFS_BOARDS),$(CLIFFS_APQ_BASE_DTB))
pineapple-overlays-dtb-$(CONFIG_ARCH_CLIFFS) += $(CLIFFS_BOARDS) $(NOAPQ_CLIFFS_BOARDS) $(CLIFFS_BASE_DTB) $(CLIFFS_APQ_BASE_DTB)

VOLCANO_BASE_DTB += volcano.dtb volcano6.dtb volcano6i.dtb
VOLCANO_APQ_BASE_DTB += volcano6p.dtb volcano6ip.dtb

VOLCANO_BOARDS += \
		volcano-atp-overlay.dtbo \
		volcano-idp-overlay.dtbo \
		volcano-idp-wcd9395-overlay.dtbo \
		volcano6i-idp-wcd9395-ganges-overlay.dtbo \
		volcano6i-idp-wcd9395-brahma-overlay.dtbo \
		volcano-mtp-overlay.dtbo \
		volcano-mtp-wcd9395-aatc-overlay.dtbo \
		volcano-mtp-pm7550ba-overlay.dtbo \
		volcano6i-mtp-wcd9395-ganges-overlay.dtbo \
		volcano6i-mtp-ganges-overlay.dtbo \
		volcano6i-mtp-wcd9395-ganges-hac2019-overlay.dtbo \
		volcano6i-mtp-wcd9395-brahma-overlay.dtbo \
		volcano6i-mtp-brahma-overlay.dtbo \
		volcano6i-mtp-wcd9395-moselle-overlay.dtbo \
		volcano6i-mtp-moselle-wingmate-overlay.dtbo \
		volcano-qrd-overlay.dtbo \
		volcano6i-qrd-moselle-overlay.dtbo

NOAPQ_VOLCANO_BOARDS += \
		volcano-rumi-overlay.dtbo

pineapple-dtb-$(CONFIG_ARCH_VOLCANO) += \
	$(call add-overlays, $(VOLCANO_BOARDS) $(NOAPQ_VOLCANO_BOARDS),$(VOLCANO_BASE_DTB))\
	$(call add-overlays, $(VOLCANO_BOARDS), $(VOLCANO_APQ_BASE_DTB))
pineapple-overlays-dtb-$(CONFIG_ARCH_VOLCANO) += $(VOLCANO_BOARDS) $(NOAPQ_VOLCANO_BOARDS) $(VOLCANO_BASE_DTB) $(VOLCANO_APQ_BASE_DTB)

dtb-y += $(pineapple-dtb-y)

FPS_BASE_DTB += fp6.dtb
FPS_BOARDS += fp6-sku-600-overlay.dtbo
FPS_BOARDS += fp6-sku-603-overlay.dtbo
FPS_BOARDS += fp6-sku-604-overlay.dtbo
FPS_BOARDS += fp6-sku-605-overlay.dtbo
FPS_BOARDS += fp6-sku-606-overlay.dtbo
FPS_BOARDS += fp6-sku-607-overlay.dtbo
FPS_BOARDS += fp6-sku-608-overlay.dtbo

fps-dtb-$(CONFIG_ARCH_FPSPRING) += $(call add-overlays, $(FPS_BOARDS),$(FPS_BASE_DTB))

fps-overlays-dtb-$(CONFIG_ARCH_FPSPRING) += $(FPS_BOARDS) $(FPS_BASE_DTB)

dtb-y += $(fps-dtb-y)

always-y	:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
