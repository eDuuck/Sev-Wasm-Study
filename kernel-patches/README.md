# Kernel Patches

BE AWARE THAT CLAUDE GENERATED THIS README.
If things fail to properly integrate into the sev-step-host-kernel, 
I would recommend to look at the patch changes and manually add the changes
yourself.

Patch 0001 and 0002 are needed for more page fault info. Although 0001 can be
skipped. The changes are removed in 0002.

Patch 0003 added a freeze error when page tracking was called during the pause
of a single step. Turns out that the TLB flush called by page tracking was unable to
proceed when the spinlock was taken. Since a TLB flush is done by every single-step 
however, there is no need to perform this TLB flush. So the page-tracking call checks
if single-stepping is active, and if so, it then does not call for a TLB flush.


This directory contains patches that must be applied to the
[SEV-STEP kernel](https://github.com/sev-step/sev-step-host-kernel)
before building, in order to support the features used by this userland.

## Base Commit

The patches apply on top of:

```
12347789b5e6 - Update README
Branch: sm-sev-step-main-5-19-kern-release
```

## What the Patches Do

| Patch | Description |
|---|---|
| `0001` | Adds `fault_mode` field to the `usp_page_fault_event_v` structure |
| `0002` | Sends additional data to userland on page-fault events |
| `0003` | Removes TLB flush when single-stepping is enabled with TLB-flush active |

## Applying the Patches

```bash
# 1. Clone the SEV-STEP kernel
git clone https://github.com/sev-step/sev-step-host-kernel
cd sev-step-host-kernel

# 2. Check out the correct base commit
git checkout 12347789b5e6

# 3. Apply all patches in order
git am /path/to/this-repo/kernel-patches/*.patch
```

If a patch fails to apply, check that you are on the correct base commit:
```bash
git log --oneline -1
# Should show: 12347789b5e6 Update README
```

## Building the Kernel

After applying the patches, build and install the kernel:

```bash
# Copy the current system config as a starting point
cp /boot/config-$(uname -r) .config
make olddefconfig

# Build (adjust -j to your core count)
make -j$(nproc)

# Install modules and kernel
sudo make modules_install
sudo make install

# Update bootloader and reboot
sudo update-grub
sudo reboot
```

After rebooting, verify the correct kernel is running:
```bash
uname -r
```
