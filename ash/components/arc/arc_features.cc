// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_features.h"

#include "base/feature_list.h"

namespace arc {

// Controls whether to always start ARC automatically, or wait for the user's
// action to start it later in an on-demand manner.
BASE_FEATURE(kArcOnDemandFeature,
             "ArcOnDemand",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ACTION_BOOT_COMPLETED broadcast for third party applications on ARC.
// When disabled, third party apps will not receive this broadcast.
BASE_FEATURE(kBootCompletedBroadcastFeature,
             "ArcBootCompletedBroadcast",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether independent ARC container app killer is enabled to replace
// the ARC container app killing in TabManagerDelegate.
BASE_FEATURE(kContainerAppKiller,
             "ContainerAppKiller",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental Custom Tabs feature for ARC.
BASE_FEATURE(kCustomTabsExperimentFeature,
             "ArcCustomTabsExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to handle files with unknown size.
BASE_FEATURE(kDocumentsProviderUnknownSizeFeature,
             "ArcDocumentsProviderUnknownSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we automatically send ARCVM into Doze mode
// when it is mostly idle - even if Chrome is still active.
BASE_FEATURE(kEnableArcIdleManager,
             "ArcIdleManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

// For test purposes, ignore battery status changes, allowing Doze mode to
// kick in even if we do not receive powerd changes related to battery.
const base::FeatureParam<bool> kEnableArcIdleManagerIgnoreBatteryForPLT{
    &kEnableArcIdleManager, "ignore_battery_for_test", false};

// Controls whether files shared to ARC Nearby Share are shared through the
// FuseBox filesystem, instead of the default method (through a temporary path
// managed by file manager).
BASE_FEATURE(kEnableArcNearbyShareFuseBox,
             "ArcNearbyShareFuseBox",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable ARCVM /data migration. It does not take effect
// when kEnableVirtioBlkForData is set, in which case virtio-blk is used for
// /data without going through the migration.
BASE_FEATURE(kEnableArcVmDataMigration,
             "ArcVmDataMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether WebView Zygote is lazily initialized in ARC.
BASE_FEATURE(kEnableLazyWebViewInit,
             "LazyWebViewInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether crosvm for ARCVM does per-VM core scheduling on devices with
// MDS/L1TF vulnerabilities. When this feature is disabled, crosvm does per-vCPU
// core scheduling which is more secure.
//
// How to safely disable this feature for security (or other) reasons:
//
// 1) Visit go/stainless and verify arc.Boot.vm_with_per_vcpu_core_scheduling is
//    green (it should always be because arc.Boot is a critical test.)
// 2) Change the default value of this feature to FEATURE_DISABLED_BY_DEFAULT.
// 3) Monitor arc.Boot.vm at go/stainless after Chrome is rolled.
// 4) Ask ARC team (//ash/components/arc/OWNERS) to update arc.CPUSet.vm test
//    so the Tast test uses the updated ArcEnablePerVmCoreScheduling setting.
BASE_FEATURE(kEnablePerVmCoreScheduling,
             "ArcEnablePerVmCoreScheduling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether app permissions are read-only in the App Management page.
// Only applies on Android T+.
BASE_FEATURE(kEnableReadOnlyPermissions,
             "ArcEnableReadOnlyPermissions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether we should delegate audio focus requests from ARC to Chrome.
BASE_FEATURE(kEnableUnifiedAudioFocusFeature,
             "ArcEnableUnifiedAudioFocus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARC handles unmanaged->managed account transition.
BASE_FEATURE(kEnableUnmanagedToManagedTransitionFeature,
             "ArcEnableUnmanagedToManagedTransitionFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use virtio-blk for Android /data instead of using
// virtio-fs.
BASE_FEATURE(kEnableVirtioBlkForData,
             "ArcEnableVirtioBlkForData",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to allow Android apps to access external storage devices
// like USB flash drives and SD cards.
BASE_FEATURE(kExternalStorageAccess,
             "ArcExternalStorageAccess",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether new UI style for ARC ghost window.
BASE_FEATURE(kGhostWindowNewStyle,
             "ArcGhostWindowNewStyle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used for overriding config params for the virtio-blk feature above.
BASE_FEATURE(kVirtioBlkDataConfigOverride,
             "ArcVirtioBlkDataConfigOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the LVM-provided disk as the backend device for
// Android /data instead of using the concierge-provided disk.
const base::FeatureParam<bool> kVirtioBlkDataConfigUseLvm{
    &kVirtioBlkDataConfigOverride, "use_lvm", false};

// Indicates whether LVM application containers feature is supported.
BASE_FEATURE(kLvmApplicationContainers,
             "ArcLvmApplicationContainers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental file picker feature for ARC.
BASE_FEATURE(kFilePickerExperimentFeature,
             "ArcFilePickerExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the guest zram is enabled. This is only for ARCVM.
BASE_FEATURE(kGuestZram, "ArcGuestZram", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the size of the guest zram by an absolute value. Ignored if
// "size_percentage" is set.
const base::FeatureParam<int> kGuestZramSize{&kGuestZram, "size", 0};

// Controls the size of the guest zram by a percentage of the VM memory size.
const base::FeatureParam<int> kGuestZramSizePercentage{&kGuestZram,
                                                       "size_percentage", 0};

// Controls swappiness for the ARCVM guest.
const base::FeatureParam<int> kGuestZramSwappiness{&kGuestZram, "swappiness",
                                                   0};

// Controls whether to do per-process reclaim from the ARCVM guest.
const base::FeatureParam<bool> kGuestReclaimEnabled{
    &kGuestZram, "guest_reclaim_enabled", false};

// Controls whether only anonymous pages are reclaimed from the ARCVM guest.
// Ignored when the "guest_reclaim_enabled" param is false.
const base::FeatureParam<bool> kGuestReclaimOnlyAnonymous{
    &kGuestZram, "guest_reclaim_only_anonymous", false};

// Enables/disables ghost when user launch ARC app from shelf/launcher when
// App already ready for launch.
BASE_FEATURE(kInstantResponseWindowOpen,
             "ArcInstantResponseWindowOpen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables/disables mlock() of guest memory for ARCVM.
// Often used in combination with kGuestZram.
BASE_FEATURE(kLockGuestMemory,
             "ArcLockGuestMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls keyboard shortcut helper integration feature in ARC.
BASE_FEATURE(kKeyboardShortcutHelperIntegrationFeature,
             "ArcKeyboardShortcutHelperIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM MGLRU reclaim feature.
BASE_FEATURE(kMglruReclaim,
             "ArcMglruReclaim",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the interval between MGLRU reclaims in milliseconds
// A value of 0 will disable the MGLRU reclaim feature.
// Current value is the best tuning from the ChromeOSARCVMAppRescue experiment.
const base::FeatureParam<int> kMglruReclaimInterval{&kMglruReclaim, "interval",
                                                    30000};

// Controls the swappiness of MGLRU reclaims, in the range of 0 to 200
// 0 means only filecache will be used while 200 means only swap will be used
// any value in between will be a mix of both
// The lower the value, the higher the ratio of freeing filecache pages
// Implementation and a more detailed description can be found in ChromeOS
// src/third_party/kernel/v5.10-arcvm/mm/vmscan.c
const base::FeatureParam<int> kMglruReclaimSwappiness{&kMglruReclaim,
                                                      "swappiness", 0};

// Toggles between native bridge implementations for ARC.
// Note, that we keep the original feature name to preserve
// corresponding metrics.
BASE_FEATURE(kNativeBridgeToggleFeature,
             "ArcNativeBridgeExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, utility processes are spawned to perform hardware decode
// acceleration on behalf of ARC++/ARCVM instead of using the GPU process.
BASE_FEATURE(kOutOfProcessVideoDecoding,
             "OutOfProcessVideoDecoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARC picture-in-picture feature. If this is enabled, then Android
// will control which apps can enter PIP. If this is disabled, then ARC PIP
// will be disabled.
BASE_FEATURE(kPictureInPictureFeature,
             "ArcPictureInPicture",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRoundedWindowCompat,
             "ArcRoundedWindowCompat",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kRoundedWindowCompatStrategy[] = "RoundedWindowCompatStrategy";
// The following values must be matched with `RoundedWindowCompatStrategy` enum
// defined in //ash/components/arc/mojom/chrome_feature_flags.mojom.
const char kRoundedWindowCompatStrategy_BottomOnlyGesture[] = "1";
const char kRoundedWindowCompatStrategy_LeftRightBottomGesture[] = "2";

// Controls ARCVM real time vcpu feature on a device with 2 logical cores
// online.
// When you change the default, you also need to change the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// match it to the new default.
BASE_FEATURE(kRtVcpuDualCore,
             "ArcRtVcpuDualCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 3+ logical cores
// online.
// When you change the default, you also need to modify the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// add ArcRtVcpuQuadCore there. Otherwise, the test will start failing.
BASE_FEATURE(kRtVcpuQuadCore,
             "ArcRtVcpuQuadCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, tracing raw files are saved in order to help debug failures.
BASE_FEATURE(kSaveRawFilesOnTracing,
             "ArcSaveRawFilesOnTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, CertStoreService will talk to KeyMint instead of Keymaster on
// ARC-T.
BASE_FEATURE(kSwitchToKeyMintOnT,
             "ArcSwitchToKeyMintOnT",
             base::FEATURE_ENABLED_BY_DEFAULT);

// On boards that blocks KeyMint at launch, enable this feature to force enable
// KeyMint.
BASE_FEATURE(kSwitchToKeyMintOnTOverride,
             "ArcSwitchToKeyMintOnTOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ARC will pass install priority to Play in sync install
// requests.
BASE_FEATURE(kSyncInstallPriority,
             "ArcSyncInstallPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, touch screen emulation for compatibility is enabled on specific
// apps.
BASE_FEATURE(kTouchscreenEmulation,
             "ArcTouchscreenEmulation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARC should be enabled on unaffiliated devices on client side
BASE_FEATURE(kUnaffiliatedDeviceArcRestriction,
             "UnaffiliatedDeviceArcRestriction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARC USB Storage UI feature.
// When enabled, chrome://settings and Files.app will ask if the user wants
// to expose USB storage devices to ARC.
BASE_FEATURE(kUsbStorageUIFeature,
             "ArcUsbStorageUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC dalvik memory profile in ARCVM.
// When enabled, Android tries to use dalvik memory profile tuned based on the
// device memory size.
BASE_FEATURE(kUseDalvikMemoryProfile,
             "ArcUseDalvikMemoryProfile",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the system/vendor images are mounted without specifying a
// block size.
BASE_FEATURE(kUseDefaultBlockSize,
             "ArcVmUseDefaultBlockSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether ARC uses VideoDecoder-backed video decoding.
// When enabled, GpuArcVideoDecodeAccelerator will use VdVideoDecodeAccelerator
// to delegate decoding tasks to VideoDecoder implementations, instead of using
// VDA implementations created by GpuVideoDecodeAcceleratorFactory.
BASE_FEATURE(kVideoDecoder,
             "ArcVideoDecoder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to continuously log PSI memory pressure data to Chrome.
BASE_FEATURE(kVmMemoryPSIReports,
             "ArcVmMemoryPSIReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls how frequently memory pressure data is logged
const base::FeatureParam<int> kVmMemoryPSIReportsPeriod{&kVmMemoryPSIReports,
                                                        "period", 10};

// Controls whether a custom memory size is used when creating ARCVM. When
// enabled, ARCVM is sized with the following formula:
//  min(max_mib, ram_percentage / 100 * RAM + shift_mib)
// If disabled, memory is sized by concierge which, at the time of writing, uses
// RAM - 1024 MiB.
BASE_FEATURE(kVmMemorySize,
             "ArcVmMemorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the amount to "shift" system RAM when sizing ARCVM. The default
// value of 0 means that ARCVM's memory will be thr same as the system.
const base::FeatureParam<int> kVmMemorySizeShiftMiB{&kVmMemorySize, "shift_mib",
                                                    0};

// Controls the maximum amount of memory to give ARCVM. The default value of
// INT32_MAX means that ARCVM's memory is not capped.
const base::FeatureParam<int> kVmMemorySizeMaxMiB{&kVmMemorySize, "max_mib",
                                                  INT32_MAX};

// Controls the percentage of system RAM for calculation of ARCVM size. The
// default value of 100 means the whole system RAM will be used in ARCM size
// calculation.
const base::FeatureParam<int> kVmMemorySizePercentage{&kVmMemorySize,
                                                      "ram_percentage", 100};

// Controls experimental key to enable pre-ANR handling for BroadcastQueue in
// ARCVM.
BASE_FEATURE(kVmBroadcastPreNotifyANR,
             "ArcVmBroadcastPreAnrHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental key to enable ghost window when launch app under ARCVM
// swap out state.
BASE_FEATURE(kVmmSwapoutGhostWindow,
             "ArcVmmSwapoutGhostWindow",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental key to enable Vmm swap for ARCVM by keyboard shortcut.
BASE_FEATURE(kVmmSwapKeyboardShortcut,
             "ArcvmSwapoutKeyboardShortcut",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental key to enable and swap out ARCVM by policy.
BASE_FEATURE(kVmmSwapPolicy,
             "ArcVmmSwapPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the time interval between create staging memory and swap out. The
// default value is 10 seconds.
const base::FeatureParam<int> kVmmSwapOutDelaySecond{&kVmmSwapPolicy,
                                                     "delay_sec", 10};

// Controls the time interval between two swap out. The default value is 12
// hours.
const base::FeatureParam<int> kVmmSwapOutTimeIntervalSecond{
    &kVmmSwapPolicy, "swapout_interval_sec", 60 * 60 * 12};

// Controls the time interval of ARC silence. The default value is 15 minutes.
const base::FeatureParam<int> kVmmSwapArcSilenceIntervalSecond{
    &kVmmSwapPolicy, "arc_silence_interval_sec", 60 * 15};

// When enabled, ARC uses XDG-based Wayland protocols.
BASE_FEATURE(kXdgMode, "ArcXdgMode", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the feature to delay low memory kills of high priority apps when the
// memory pressure is below foreground.
BASE_FEATURE(kPriorityAppLmkDelay,
             "ArcPriorityAppLmkDelay",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the time to wait for inactivity of a high priority app before
// considering it to be killed. The default value is 5 minutes.
const base::FeatureParam<int> kPriorityAppLmkDelaySecond{
    &kPriorityAppLmkDelay, "priority_app_lmk_delay_sec", 60 * 5};

// Controls the list of apps to be considered as high priority that would have a
// delay before considered to be killed.
const base::FeatureParam<std::string> kPriorityAppLmkDelayList{
    &kPriorityAppLmkDelay, "priority_app_lmk_delay_list", ""};

// Controls the feature to update the minimum Android process state to be
// considered to be killed under perceptible memory pressure. This is to prevent
// top Android apps from being killed that result in bad user experience.
BASE_FEATURE(kLmkPerceptibleMinStateUpdate,
             "ArcLmkPerceptibleMinStateUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace arc
