# kernel_qcom-6.1

DiamaneOS kernel sources for Qualcomm devices on Linux 6.1 (Android common
kernel android14-6.1). Devices: Fairphone 6.

## Layout

The layout of Qualcomm's kernel workspace, so the Qualcomm build files work
unchanged:

- `kernel_platform/`: the Qualcomm vendor kernel (`msm-kernel`), the build
  system (`build/kernel`, Bazel rules), the bootloader (`bootable/bootloader/edk2`)
  and host tools.
- `kernel_platform/common`: submodule,
  [DiamaneOS/kernel_common-6.1](https://github.com/DiamaneOS/kernel_common-6.1),
  GrapheneOS's common kernel with our changes.
- `vendor/`: Qualcomm, NXP, ST and Samsung driver modules and device trees.
- `prebuilts.json`: the toolchains (Clang, build tools, Bazel, JDK and others).
  They are fetched, not committed.

## History

Each upstream project was imported once, in a commit naming its source and
commit; Fairphone's FP6 changes are part of those imports. DiamaneOS changes
are separate commits on top. The `upstream` branch holds only the imports.

## Get the sources

```bash
git clone --recurse-submodules https://github.com/DiamaneOS/kernel_qcom-6.1.git
cd kernel_qcom-6.1
./sync_prebuilts.sh
```

## Build

`diamaneos kernel build` from
[diamaneos-tools](https://github.com/DiamaneOS/diamaneos-tools) builds the
kernel, modules and device trees and packages them; see its `docs/FP6-KERNEL.md`.
Published builds:
[device_fairphone_FP6-kernels](https://github.com/DiamaneOS/device_fairphone_FP6-kernels).

## Updating from upstream

On `upstream`, replace one project's folder with the new upstream state in a
single commit naming the source commit, then merge `upstream` into
`android17`. Git keeps our changes and reports a conflict only where upstream
changed the same lines.

## Licence

Each project keeps its own licence files. The kernel and most modules are
GPL-2.0; see the files in each folder.
