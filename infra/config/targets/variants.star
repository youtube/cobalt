# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/targets.star", "targets")

targets.variant(
    name = "DISABLE_FIELD_TRIAL_CONFIG",
    identifier = "Disable Field Trial Config",
    args = [
        "--disable-field-trial-config",
        "--webview-verbose-logging",
    ],
)

targets.variant(
    name = "DISABLE_FIELD_TRIAL_CONFIG_WEBVIEW_COMMANDLINE",
    identifier = "Disable Field Trial Config",
    args = [
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--webview-verbose-logging",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR",
    identifier = "Single Group Per Study Prefer Existing Behavior Field Trial Config",
    args = [
        "--variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_existing_behavior/seed.json",
        "--accept-empty-variations-seed-signature",
        "--webview-verbose-logging",
        "--disable-field-trial-config",
        "--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR",
    identifier = "Single Group Per Study Prefer New Behavior Field Trial Config",
    args = [
        "--variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_new_behavior/seed.json",
        "--accept-empty-variations-seed-signature",
        "--webview-verbose-logging",
        "--disable-field-trial-config",
        "--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_EXISTING_BEHAVIOR_WEBVIEW_COMMANDLINE",
    identifier = "Single Group Per Study Prefer Existing Behavior Field Trial Config",
    args = [
        "--webview-variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_existing_behavior/seed.json",
        "--webview-command-line-arg=--accept-empty-variations-seed-signature",
        "--webview-command-line-arg=--webview-verbose-logging",
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "SINGLE_GROUP_PER_STUDY_PREFER_NEW_BEHAVIOR_WEBVIEW_COMMANDLINE",
    identifier = "Single Group Per Study Prefer New Behavior Field Trial Config",
    args = [
        "--webview-variations-test-seed-path=../../third_party/chromium-variations/single_group_per_study_prefer_new_behavior/seed.json",
        "--webview-command-line-arg=--accept-empty-variations-seed-signature",
        "--webview-command-line-arg=--webview-verbose-logging",
        "--webview-command-line-arg=--disable-field-trial-config",
        "--webview-command-line-arg=--fake-variations-channel=stable",
    ],
)

targets.variant(
    name = "IPHONE_7_15_4_1",
    identifier = "iPhone 7 15.4.1",
    swarming = targets.swarming(
        dimensions = {
            "os": "iOS-15.4.1",
            "device": "iPhone9,1",
        },
    ),
)

# This set of variants is encoded in a json file so that
# chrome/official.infra/lacros-version-skew-roller can update the variant
# definitions
[targets.variant(
    name = name,
    identifier = v["identifier"],
    description = v["description"],
    args = v["args"],
    swarming = targets.swarming(
        cipd_packages = [targets.cipd_package(
            package = p["cipd_package"],
            location = p["location"],
            revision = p["revision"],
        ) for p in v["swarming"]["cipd_packages"]],
    ),
) for name, v in json.decode(io.read_file("./lacros-version-skew-variants.json")).items()]

targets.variant(
    name = "LINUX_INTEL_UHD_630_STABLE",
    identifier = "UHD 630",
    mixins = [
        "linux_intel_uhd_630_stable",
    ],
)

targets.variant(
    name = "LINUX_NVIDIA_GTX_1660_STABLE",
    identifier = "GTX 1660",
    mixins = [
        "linux_nvidia_gtx_1660_stable",
    ],
)

targets.variant(
    name = "MAC_MINI_INTEL_GPU_STABLE",
    identifier = "8086:3e9b",
    mixins = [
        "mac_mini_intel_gpu_stable",
    ],
)

targets.variant(
    name = "MAC_RETINA_AMD_GPU_STABLE",
    identifier = "1002:67ef",
    mixins = [
        "mac_retina_amd_gpu_stable",
    ],
)

targets.variant(
    name = "MAC_RETINA_NVIDIA_GPU_STABLE",
    identifier = "10de:0fe9",
    mixins = [
        "mac_retina_nvidia_gpu_stable",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_2_15_5",
    identifier = "iPad Air 2 15.5",
    mixins = [
        "ios_runtime_cache_15_5",
    ],
    args = [
        "--platform",
        "iPad Air 2",
        "--version",
        "15.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_5TH_GEN_15_5",
    identifier = "iPad Air (5th generation) 15.5",
    mixins = [
        "ios_runtime_cache_15_5",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "15.5",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_5TH_GEN_16_4",
    identifier = "iPad Air (5th generation) 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPAD_AIR_5TH_GEN_17_0",
    identifier = "iPad Air (5th generation) 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPad Air (5th generation)",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_6TH_GEN_16_4",
    identifier = "iPad Pro (12.9-inch) (6th generation) 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPad Pro (12.9-inch) (6th generation)",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPAD_PRO_6TH_GEN_17_0",
    identifier = "iPad Pro (12.9-inch) (6th generation) 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPad Pro (12.9-inch) (6th generation)",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_16_4",
    identifier = "iPad (10th generation) 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPAD_10TH_GEN_17_0",
    identifier = "iPad (10th generation) 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPad (10th generation)",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_6S_15_5",
    identifier = "iPhone 6s 15.5",
    mixins = [
        "ios_runtime_cache_15_5",
    ],
    args = [
        "--platform",
        "iPhone 6s",
        "--version",
        "15.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_13_15_5",
    identifier = "iPhone 13 15.5",
    mixins = [
        "ios_runtime_cache_15_5",
    ],
    args = [
        "--platform",
        "iPhone 13",
        "--version",
        "15.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_16_4",
    identifier = "iPhone 14 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_17_0",
    identifier = "iPhone 14 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPhone 14",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PLUS_16_4",
    identifier = "iPhone 14 Plus 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone 14 Plus",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PLUS_17_0",
    identifier = "iPhone 14 Plus 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPhone 14 Plus",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PRO_MAX_16_4",
    identifier = "iPhone 14 Pro Max 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone 14 Pro Max",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPHONE_14_PRO_MAX_17_0",
    identifier = "iPhone 14 Pro Max 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPhone 14 Pro Max",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_15_5",
    identifier = "iPhone SE (3rd generation) 15.5",
    mixins = [
        "ios_runtime_cache_15_5",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "15.5",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_16_4",
    identifier = "iPhone SE (3rd generation) 16.4",
    mixins = [
        "ios_runtime_cache_16_4",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "16.4",
    ],
)

targets.variant(
    name = "SIM_IPHONE_SE_3RD_GEN_17_0",
    identifier = "iPhone SE (3rd generation) 17.0",
    mixins = [
        "ios_runtime_cache_17_0",
    ],
    args = [
        "--platform",
        "iPhone SE (3rd generation)",
        "--version",
        "17.0",
    ],
)

targets.variant(
    name = "SIM_IPHONE_X_15_5",
    identifier = "iPhone X 15.5",
    mixins = [
        "ios_runtime_cache_15_5",
    ],
    args = [
        "--platform",
        "iPhone X",
        "--version",
        "15.5",
    ],
)

targets.variant(
    name = "WEBVIEW_TRICHROME_FULL_CTS_TESTS",
    identifier = "full_mode",
    swarming = targets.swarming(
        shards = 2,
    ),
)

targets.variant(
    name = "WEBVIEW_TRICHROME_INSTANT_CTS_TESTS",
    identifier = "instant_mode",
    args = [
        "--exclude-annotation",
        "AppModeFull",
        "--test-apk-as-instant",
    ],
)

# This set of variants is encoded in a json file so that
# chrome/official.infra/lacros-skylab-tests-cros-img-roller can update the
# variant definitions
[targets.variant(
    name = name,
    enabled = variant.get("enabled"),
    identifier = variant["identifier"],
    # The cros_chrome_version field isn't used by the generator: it's used by
    # the cros skylab test image roller to compare against other data sources
    skylab = targets.skylab(**{
        k: v
        for k, v in variant["skylab"].items()
        if k != "cros_chrome_version"
    }),
) for name, variant in json.decode(io.read_file("./cros-skylab-variants.json")).items()]

targets.variant(
    name = "LACROS_AMD64_GENERIC",
    identifier = "amd64-generic",
    args = [
        "--board=amd64-generic",
        "--use-vm",
    ],
    swarming = targets.swarming(
        dimensions = {
            "cpu": "x86-64",
            "kvm": "1",
            "os": "Ubuntu-22.04",
        },
    ),
)

targets.variant(
    name = "LACROS_ASH_TOT",
    identifier = "Ash ToT",
    args = [
        "--deploy-lacros",
    ],
)

targets.variant(
    name = "LACROS_EVE",
    identifier = "eve",
    args = [
        "--board=eve",
        "--flash",
    ],
    # TODO: crbug.com/1479528 - We have limited eve ChromeOS capacity in
    # swarming, switch the try builders to use skylab or add additional ChromeOS
    # capacity if we want to test eve on the CQ
    ci_only = True,
    swarming = targets.swarming(
        dimensions = {
            "os": "ChromeOS",
            "device_type": "eve",
        },
    ),
)

targets.variant(
    name = "WIN10_INTEL_UHD_630_STABLE",
    identifier = "8086:9bc5",
    mixins = [
        "swarming_containment_auto",
        "win10_intel_uhd_630_stable",
    ],
)

targets.variant(
    name = "WIN10_NVIDIA_GTX_1660_STABLE",
    identifier = "10de:2184",
    mixins = [
        "win10_nvidia_gtx_1660_stable",
    ],
)

# Model validation tests with no args as they are passed in from Google3.
targets.variant(
    name = "MODEL_VALIDATION_BASE",
    identifier = "MODEL_VALIDATION_BASE",
)

targets.variant(
    name = "MODEL_VALIDATION_TRUNK",
    identifier = "MODEL_VALIDATION_TRUNK",
    linux_args = [
        "--chromedriver",
        "chromedriver",
        "--binary",
        "chrome",
    ],
    mac_args = [
        "--chromedriver",
        "chromedriver",
        "--binary",
        "Chromium.app/Contents/MacOS/Chromium",
    ],
    win64_args = [
        "--chromedriver",
        "chromedriver.exe",
        "--binary",
        "Chrome.exe",
    ],
)
