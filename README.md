# spes-kernel

Hardened Linux `4.19-cip` kernel for the Xiaomi Redmi Note 11 / NFC (`spes` / `spesn`, Qualcomm `khaje` / SM6225).

## Platform & Compatibility

- **Devices:** Xiaomi Redmi Note 11 (`spes`, `spes-sm5602`) and Redmi Note 11 NFC (`spesn`)
- **Platform:** Qualcomm `khaje` (SM6225 / Bengal)
- **Device Tree:** `Angxddeep` (`LineageOS sm8250`) kernel/DTBO layout
- **Unsupported:** Murali device-tree ROMs

## Branches & Variants

| Branch | Variant | Description |
|---|---|---|
| `main` | `vanilla` | Unrooted hardened `4.19-cip` base with shared fixes and configuration |
| `bakasu` | `bakasu` | `main` with BakaSU (`CONFIG_KSU_MANUAL_HOOK=y`), without `SUSFS` or `NoMount` |
| `bakasu-susfs-nmnt` | `bakasu-susfs-nmnt` | `main` with BakaSU (`CONFIG_KSU_SUSFS=y`), `SUSFS v2.4.0`, and built-in `NoMount` VFS path redirection |

## Configuration

- **Defconfig:** `vendor/bengal-perf_defconfig` + `vendor/xiaomi/spes.config`
- **Toolchain:** Clang 18 / LLD 18 (`ARCH=arm64 LLVM=1`, Ubuntu 24.04)
- **Hardening:** ThinLTO, Clang CFI (`CONFIG_CFI_CLANG` + shadow), Shadow Call Stack, KFENCE, zero-initialized stack variables, hardened usercopy, KASLR, PAN / SW-PAN
- **Core Features:** `ZRAM` (`lzo-rle` default, `LZ4` / `ZSTD` available, `ZRAM_DEDUP`), `FQ_CODEL` default qdisc (`FQ` available), `CUBIC` default TCP congestion control (`BBR` available), `WireGuard`, built-in `exFAT`, `EROFS`, `F2FS`, `EXT4`, `FUSE_BPF`, `pstore` (`/proc/last_kmsg`), `st21nfc` (`CONFIG_NFC_ST21NFC=y`), and `uname` release reporting (`5.10.241`) for UID 0 (`root`-owned) Android BPF loaders (`netbpfload`, `netd`, `bpfloader`)

## Installation

1. Back up the existing `boot` and `dtbo` partitions.
2. Download the desired AnyKernel3 archive from [Releases](https://github.com/NablaSphere/spes-kernel/releases).
3. Flash the archive in recovery and reboot.
4. *(Optional, `bakasu-susfs-nmnt`)* Install [NablaSphere/spes-susfs-config](https://github.com/NablaSphere/spes-susfs-config) for BakaSU SUSFS defaults and dynamic cmdline/boot-state configuration.

### Root Variants

- **Manager:** Both `bakasu` and `bakasu-susfs-nmnt` enable `CONFIG_KSU_MULTI_MANAGER_SUPPORT=y`, supporting official [BakaSU](https://github.com/Baka-SU/BakaSU), [KernelSU](https://github.com/tiann/KernelSU), [KOWSU](https://github.com/KOWX712/KernelSU), and the custom `Mihon` (`org.nablasphere.mihon`) manager APK distributed with releases.
- **`bakasu-susfs-nmnt`:** Requires the matching [NoMount](https://github.com/maxsteeel/nomount) metamodule attached to each release to activate built-in VFS path redirection.

## References

- [Linux CIP 4.19 (`linux-4.19.y-cip`)](https://git.kernel.org/pub/scm/linux/kernel/git/cip/linux-cip.git)
- [Angxddeep/android_kernel_xiaomi_spes](https://github.com/Angxddeep/android_kernel_xiaomi_spes)
- [Baka-SU/BakaSU](https://github.com/Baka-SU/BakaSU)
- [simonpunk/susfs4ksu](https://gitlab.com/simonpunk/susfs4ksu)
- [maxsteeel/nomount](https://github.com/maxsteeel/nomount)
