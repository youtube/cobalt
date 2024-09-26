# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.mac builder group."""

load("//lib/args.star", "args")
load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "reclient", "sheriff_rotations", "xcode")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium.mac",
    pool = ci.DEFAULT_POOL,
    os = os.MAC_DEFAULT,
    sheriff_rotations = sheriff_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    thin_tester_cores = 8,
)

consoles.console_view(
    name = "chromium.mac",
    branch_selector = [
        branches.selector.IOS_BRANCHES,
        branches.selector.MAC_BRANCHES,
    ],
    ordering = {
        None: ["release"],
        "release": consoles.ordering(short_names = ["bld"]),
        "debug": consoles.ordering(short_names = ["bld"]),
        "ios|default": consoles.ordering(short_names = ["dev", "sim"]),
    },
)

consoles.console_view(
    name = "sheriff.ios",
    title = "iOS Sheriff Console",
    ordering = {
        "*type*": consoles.ordering(short_names = ["dev", "sim"]),
        None: ["chromium.mac", "chromium.fyi"],
        "chromium.mac": "*type*",
        "chromium.fyi|13": "*type*",
    },
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("sheriff_rotations", sheriff_rotations.IOS)
    kwargs.setdefault("xcode", xcode.x14main)
    return ci.builder(name = name, **kwargs)

ci.builder(
    name = "Mac Builder",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
    reclient_instance = reclient.instance.DEFAULT_TRUSTED,
    reclient_jobs = reclient.jobs.DEFAULT,
)

ci.builder(
    name = "Mac Builder (dbg)",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    os = os.MAC_ANY,
    console_view_entry = consoles.console_view_entry(
        category = "debug",
        short_name = "bld",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.builder(
    name = "mac-arm64-on-arm64-rel",

    # TODO(crbug.com/1186823): Expand to more branches when all M1 bots are
    # rosettaless.
    # branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "a64",
    ),
)

ci.builder(
    name = "mac-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    os = os.MAC_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "bld",
    ),
)

ci.builder(
    name = "mac-intel-on-arm64-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    sheriff_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "a64",
    ),
)

ci.thin_tester(
    name = "mac11-arm64-rel-tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/mac-arm64-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "11",
    ),
)

ci.thin_tester(
    name = "mac12-arm64-rel-tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/mac-arm64-rel"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "release|arm64",
        short_name = "12",
    ),
)

ci.thin_tester(
    name = "Mac10.13 Tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "13",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac10.14 Tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "14",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac10.15 Tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = consoles.console_view_entry(
        category = "release",
        short_name = "15",
    ),
    cq_mirrors_console_view = "mirrors",
)

ci.thin_tester(
    name = "Mac11 Tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "11",
    ),
)

ci.thin_tester(
    name = "Mac12 Tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Mac Builder"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "12",
    ),
)

ci.thin_tester(
    name = "Mac12 Tests (dbg)",
    branch_selector = branches.selector.MAC_BRANCHES,
    triggered_by = ["ci/Mac Builder (dbg)"],
    builder_spec = builder_config.builder_spec(
        execution_mode = builder_config.execution_mode.TEST,
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    sheriff_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "debug",
        short_name = "12",
    ),
    cq_mirrors_console_view = "mirrors",
)

ios_builder(
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    name = "ios-catalyst",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    tree_closing = False,
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "ctl",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "ctl",
        ),
    ],
)

ios_builder(
    name = "ios-device",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "dev",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "dev",
        ),
    ],
)

ios_builder(
    name = "ios-simulator",
    branch_selector = branches.selector.IOS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = [
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "sim",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "sim",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.selector.IOS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
            apply_configs = [
                "use_clang_coverage",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "ful",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "ful",
        ),
    ],
    cq_mirrors_console_view = "mirrors",
)

ios_builder(
    name = "ios-simulator-noncq",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "ios",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
                "mac_toolchain",
            ],
            build_config = builder_config.build_config.DEBUG,
            target_bits = 64,
            target_platform = builder_config.target_platform.IOS,
        ),
        build_gs_bucket = "chromium-mac-archive",
    ),
    console_view_entry = [
        consoles.console_view_entry(
            category = "ios|default",
            short_name = "non",
        ),
        consoles.console_view_entry(
            branch_selector = branches.selector.MAIN,
            console_view = "sheriff.ios",
            category = "chromium.mac",
            short_name = "non",
        ),
    ],
    # We don't have necessary capacity to run this configuration in CQ, but it
    # is part of the main waterfall
    xcode = xcode.x14main,
)
