_platform_map = {
    "pineapple": {
        "dtb_list": [
            {"name": "pineapple.dtb"},
            {"name": "pineapple-v2.dtb"},
            {
                "name": "pineapplep.dtb",
                "apq": True,
            },
            {
                "name": "pineapplep-v2.dtb",
                "apq": True,
            },
        ],
        "dtbo_list": [
            {"name": "pineapple-atp-overlay.dtbo"},
            {"name": "pineapple-cdp-nfc-overlay.dtbo"},
            {"name": "pineapple-cdp-overlay.dtbo"},
            {"name": "pineapple-mtp-nfc-overlay.dtbo"},
            {"name": "pineapple-mtp-overlay.dtbo"},
            {"name": "pineapple-qrd-overlay.dtbo"},
            {"name": "pineapple-qrd-sku2-overlay.dtbo"},
            {"name": "pineapple-rcm-overlay.dtbo"},
            {"name": "pineapplep-hdk-overlay.dtbo"},
            {"name": "pineapple-dpm-overlay.dtbo"},
            {"name": "pineapplep-aim500-overlay.dtbo"},
            {"name": "pineapplep-aim500-v2-overlay.dtbo"},
            {
                "name": "pineapple-rumi-overlay.dtbo",
                "apq": False,
            },
        ],
        "binary_compatible_with": ["cliffs", "volcano"],
    },
    "cliffs": {
        "dtb_list": [
            {"name": "cliffs.dtb"},
            {"name": "cliffs7.dtb"},
            {
                "name": "cliffsp.dtb",
                "apq": True,
            },
            {
                "name": "cliffs7p.dtb",
                "apq": True,
            },
        ],
        "dtbo_list": [
            {
                "name": "cliffs-rumi-overlay.dtbo",
                "apq": False,
            },
            {"name": "cliffs-atp-overlay.dtbo"},
            {"name": "cliffs-cdp-overlay.dtbo"},
            {"name": "cliffs-mtp-overlay.dtbo"},
            {"name": "cliffs-qrd-overlay.dtbo"},
            {"name": "cliffs-rcm-overlay.dtbo"},
            {"name": "cliffs-mtp-kiwi-2s-nfc-wcd9395-overlay.dtbo"},
            {"name": "cliffs-mtp-peach-2s-nfc-wcd9395-overlay.dtbo"},
            {"name": "cliffs-mtp-pm8550b-overlay.dtbo"},
            {"name": "cliffs-mtp-kiwi-2s-nfc-wcd9395-pm8550b-overlay.dtbo"},
            {"name": "cliffs-mtp-peach-2s-nfc-wcd9395-pm8550b-overlay.dtbo"},
            {"name": "cliffs-rcm-qhdp-overlay.dtbo"},
            {"name": "cliffs-rcm-fhdp-kiwi-overlay.dtbo"},
            {"name": "cliffs-rcm-qhdp-kiwi-overlay.dtbo"},
            {"name": "cliffs-mtp-kiwi-overlay.dtbo"},
            {"name": "cliffs-mtp-peach-overlay.dtbo"},
        ],
    },
    "volcano": {
        "dtb_list": [
            {"name": "volcano.dtb"},
            {"name": "volcano6.dtb"},
            {"name": "volcano6i.dtb"},
            {
                "name": "volcano6p.dtb",
                "apq": True,
            },
            {
                "name": "volcano6ip.dtb",
                "apq": True,
            },
        ],
        "dtbo_list": [
            {
                "name": "volcano-rumi-overlay.dtbo",
                "apq": False,
            },
            {"name": "volcano-atp-overlay.dtbo"},
            {"name": "volcano-idp-overlay.dtbo"},
            {"name": "volcano-idp-wcd9395-overlay.dtbo"},
            {"name": "volcano6i-idp-wcd9395-ganges-overlay.dtbo"},
            {"name": "volcano6i-idp-wcd9395-brahma-overlay.dtbo"},
            {"name": "volcano-mtp-overlay.dtbo"},
            {"name": "volcano-mtp-wcd9395-aatc-overlay.dtbo"},
            {"name": "volcano6i-mtp-wcd9395-ganges-overlay.dtbo"},
            {"name": "volcano6i-mtp-ganges-overlay.dtbo"},
            {"name": "volcano6i-mtp-wcd9395-ganges-hac2019-overlay.dtbo"},
            {"name": "volcano6i-mtp-wcd9395-brahma-overlay.dtbo"},
            {"name": "volcano6i-mtp-brahma-overlay.dtbo"},
            {"name": "volcano6i-mtp-wcd9395-moselle-overlay.dtbo"},
            {"name": "volcano6i-mtp-moselle-wingmate-overlay.dtbo"},
            {"name": "volcano-qrd-overlay.dtbo"},
            {"name": "volcano6i-qrd-moselle-overlay.dtbo"},
        ],
    },
    "fps": {
        "dtb_list": [
            {"name": "fp6.dtb"},
        ],
        "dtbo_list": [
            {"name": "fp6-sku-600-overlay.dtbo"},
            {"name": "fp6-sku-603-overlay.dtbo"},
            {"name": "fp6-sku-604-overlay.dtbo"},
            {"name": "fp6-sku-605-overlay.dtbo"},
            {"name": "fp6-sku-606-overlay.dtbo"},
            {"name": "fp6-sku-607-overlay.dtbo"},
            {"name": "fp6-sku-608-overlay.dtbo"},
        ],
        "binary_compatible_with": ["cliffs", "pineapple", "volcano"],
    },
}

def _get_dtb_lists(target, dt_overlay_supported):
    ret = {
        "dtb_list": [],
        "dtbo_list": [],
    }

    if not target in _platform_map:
        return ret

    for dtb_node in [target] + _platform_map[target].get("binary_compatible_with", []):
        ret["dtb_list"].extend(_platform_map[dtb_node].get("dtb_list", []))
        if dt_overlay_supported:
            ret["dtbo_list"].extend(_platform_map[dtb_node].get("dtbo_list", []))
        else:
            # Translate the dtbo list into dtbs we can append to main dtb_list
            for dtb in _platform_map[dtb_node].get("dtb_list", []):
                dtb_base = dtb["name"].replace(".dtb", "")
                for dtbo in _platform_map[dtb_node].get("dtbo_list", []):
                    if not dtbo.get("apq", True) and dtb.get("apq", False):
                        continue

                    dtbo_base = dtbo["name"].replace(".dtbo", "")
                    ret["dtb_list"].append({"name": "{}-{}.dtb".format(dtb_base, dtbo_base)})

    return ret

def get_dtb_list(target, dt_overlay_supported = True):
    return [dtb["name"] for dtb in _get_dtb_lists(target, dt_overlay_supported).get("dtb_list", [])]

def get_dtbo_list(target, dt_overlay_supported = True):
    return [dtb["name"] for dtb in _get_dtb_lists(target, dt_overlay_supported).get("dtbo_list", [])]

