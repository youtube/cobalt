# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the tryserver.chromium.mac builder group."""

load("//lib/branches.star", "branches")
load("//lib/builder_config.star", "builder_config")
load("//lib/builders.star", "cpu", "os", "reclient", "siso", "xcode")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")

try_.defaults.set(
    executable = try_.DEFAULT_EXECUTABLE,
    builder_group = "tryserver.chromium.mac",
    pool = try_.DEFAULT_POOL,
    builderless = True,
    os = os.MAC_ANY,
    ssd = True,
    execution_timeout = try_.DEFAULT_EXECUTION_TIMEOUT,
    orchestrator_cores = 2,
    orchestrator_reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    reclient_instance = reclient.instance.DEFAULT_UNTRUSTED,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_project = siso.project.DEFAULT_UNTRUSTED,
)

def ios_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("os", os.MAC_DEFAULT)
    kwargs.setdefault("ssd", None)
    kwargs.setdefault("xcode", xcode.x15main)
    return try_.builder(name = name, **kwargs)

consoles.list_view(
    name = "tryserver.chromium.mac",
    branch_selector = [
        branches.selector.MAC_BRANCHES,
        branches.selector.IOS_BRANCHES,
    ],
)

try_.builder(
    name = "mac-arm64-clobber-rel",
    mirrors = [
        "ci/mac-arm64-archive-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-arm64-on-arm64-rel",
    mirrors = [
        "ci/mac-arm64-on-arm64-rel",
    ],
    builderless = False,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-clobber-rel",
    mirrors = [
        "ci/mac-archive-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-osxbeta-rel",
    mirrors = [
        "ci/mac-osxbeta-rel",
    ],
    builderless = False,
    os = os.MAC_13,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

# This trybot mirrors the trybot mac-rel
try_.builder(
    name = "mac-inverse-fieldtrials-fyi-rel",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac13 Tests",
        "ci/GPU Mac Builder",
        "ci/Mac Release (Intel)",
        "ci/Mac Retina Release (AMD)",
    ],
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac-intel-on-arm64-rel",
    mirrors = [
        "ci/mac-intel-on-arm64-rel",
    ],
    builderless = False,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-fieldtrial-tester",
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac-fieldtrial-tester",
    ],
    os = os.MAC_DEFAULT,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-builder-next",
    mirrors = ["ci/Mac Builder Next"],
    builderless = False,
    os = os.MAC_13,
    cpu = cpu.ARM64,
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac-perfetto-rel",
    mirrors = [
        "ci/mac-perfetto-rel",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "mac-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac13 Tests",
        "ci/GPU Mac Builder",
        "ci/Mac Release (Intel)",
        "ci/Mac Retina Release (AMD)",
    ],
    compilator = "mac-rel-compilator",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    # TODO (crbug.com/1372179): Use orchestrator pool once overloaded test pools
    # are addressed
    #use_orchestrator_pool = True,
)

try_.compilator_builder(
    name = "mac-rel-compilator",
    branch_selector = branches.selector.MAC_BRANCHES,
    os = os.MAC_DEFAULT,
    # Allow both x64 and arm64 bots.
    cpu = None,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "mac-siso-rel",
    description_html = """\
This builder shadows mac-rel builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating mac-rel from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/mac-rel"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    compilator = "mac-siso-rel-compilator",
    contact_team_email = "chrome-build-team@google.com",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "mac-siso-rel-compilator",
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    contact_team_email = "chrome-build-team@google.com",
    main_list_view = "try",
    siso_enabled = True,
)

try_.builder(
    name = "mac10.15-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac10.15-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac11-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac11-arm64-rel-tests",
    ],
    builderless = True,
)

try_.builder(
    name = "mac11-arm64-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac11-arm64-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac11-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac11-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac12-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac12-arm64-rel-tests",
    ],
    builderless = True,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "mac13-arm64-rel",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/mac-arm64-rel",
        "ci/mac13-arm64-rel-tests",
    ],
    compilator = "mac13-arm64-rel-compilator",
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 100,
    ),
)

try_.compilator_builder(
    name = "mac13-arm64-rel-compilator",
    branch_selector = branches.selector.MAC_BRANCHES,
    os = os.MAC_DEFAULT,
    # TODO (crbug.com/1245171): Revert when root issue is fixed
    grace_period = 4 * time.minute,
    main_list_view = "try",
)

try_.builder(
    name = "mac12-arm64-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac12-arm64-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac12-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac12-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac13-arm64-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac13-arm64-wpt-content-shell-fyi-rel",
    ],
)

try_.builder(
    name = "mac13-wpt-content-shell-fyi-rel",
    mirrors = [
        "ci/mac13-wpt-content-shell-fyi-rel",
    ],
)

# NOTE: the following trybots aren't sensitive to Mac version on which
# they are built, hence no additional dimension is specified.
# The 10.xx version translates to which bots will run isolated tests.
try_.builder(
    name = "mac_chromium_10.15_rel_ng",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac10.15 Tests",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_11.0_rel_ng",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac11 Tests",
    ],
    builderless = False,
)

try_.builder(
    name = "mac12-tests",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac12 Tests",
    ],
    os = os.MAC_DEFAULT,
)

try_.builder(
    name = "mac13-tests",
    mirrors = [
        "ci/Mac Builder",
        "ci/Mac13 Tests",
    ],
)

try_.builder(
    name = "mac_chromium_asan_rel_ng",
    mirrors = [
        "ci/Mac ASan 64 Builder",
        "ci/Mac ASan 64 Tests (1)",
    ],
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_compile_dbg_ng",
    branch_selector = branches.selector.MAC_BRANCHES,
    mirrors = [
        "ci/Mac Builder (dbg)",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    os = os.MAC_DEFAULT,
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(),
)

try_.builder(
    name = "mac_chromium_compile_rel_ng",
    mirrors = [
        "ci/Mac Builder",
    ],
    try_settings = builder_config.try_settings(
        include_all_triggered_testers = True,
        is_compile_only = True,
    ),
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_chromium_dbg_ng",
    mirrors = [
        "ci/Mac Builder (dbg)",
        "ci/Mac13 Tests (dbg)",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.builder(
    name = "mac_upload_clang",
    executable = "recipe:chromium_toolchain/package_clang",
    builderless = False,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "mac_upload_clang_arm",
    executable = "recipe:chromium_toolchain/package_clang",
    builderless = False,
    execution_timeout = 8 * time.hour,
)

try_.builder(
    name = "mac_upload_rust",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = False,
    execution_timeout = 8 * time.hour,
)

try_.builder(
    name = "mac_upload_rust_arm",
    executable = "recipe:chromium_toolchain/package_rust",
    builderless = False,
    execution_timeout = 6 * time.hour,
)

try_.builder(
    name = "mac-code-coverage",
    mirrors = ["ci/mac-code-coverage"],
    execution_timeout = 20 * time.hour,
)

ios_builder(
    name = "ios-asan",
    mirrors = [
        "ci/ios-asan",
    ],
)

ios_builder(
    name = "ios-blink-dbg-fyi",
    mirrors = [
        "ci/ios-blink-dbg-fyi",
    ],
    builderless = True,
    cpu = cpu.ARM64,
    execution_timeout = 4 * time.hour,
)

ios_builder(
    name = "ios-catalyst",
    mirrors = [
        "ci/ios-catalyst",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios-device",
    mirrors = [
        "ci/ios-device",
    ],
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios-fieldtrial-rel",
    mirrors = ["ci/ios-fieldtrial-rel"],
    builderless = True,
)

ios_builder(
    name = "ios-m1-simulator",
    mirrors = ["ci/ios-m1-simulator"],
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios-m1-simulator-cronet",
    mirrors = ["ci/ios-m1-simulator-cronet"],
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

try_.orchestrator_builder(
    name = "ios-simulator",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator",
    ],
    # TODO (crbug.com/1372179): Move back to orchestrator bots once they can be
    # properly rate limited
    # use_orchestrator_pool = True,
    cores = 2,
    os = os.LINUX_DEFAULT,
    compilator = "ios-simulator-compilator",
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(),
    use_clang_coverage = True,
    xcode = xcode.x15main,
)

try_.compilator_builder(
    name = "ios-simulator-compilator",
    branch_selector = branches.selector.IOS_BRANCHES,
    # Set builderless to False so that branch builders use builderful bots
    builderless = False,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    ssd = None,
    main_list_view = "try",
)

try_.orchestrator_builder(
    name = "ios-simulator-siso",
    description_html = """\
This builder shadows ios-simulator builder to compare between Siso builds and Ninja builds.<br/>
This builder should be removed after migrating ios-simulator from Ninja to Siso. b/277863839
""",
    mirrors = builder_config.copy_from("try/ios-simulator"),
    try_settings = builder_config.try_settings(
        is_compile_only = True,
    ),
    os = os.LINUX_DEFAULT,
    compilator = "ios-simulator-siso-compilator",
    contact_team_email = "chrome-build-team@google.com",
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    experiments = {
        # go/nplus1shardsproposal
        "chromium.add_one_test_shard": 10,
    },
    main_list_view = "try",
    tryjob = try_.job(
        experiment_percentage = 10,
    ),
    use_clang_coverage = True,
)

try_.compilator_builder(
    name = "ios-simulator-siso-compilator",
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    contact_team_email = "chrome-build-team@google.com",
    main_list_view = "try",
    siso_enabled = True,
    xcode = xcode.x15main,
)

ios_builder(
    name = "ios-simulator-cronet",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator-cronet",
    ],
    main_list_view = "try",
    reclient_jobs = reclient.jobs.HIGH_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "components/cronet/.+",
            "components/grpc_support/.+",
            "ios/.+",
            cq.location_filter(exclude = True, path_regexp = "components/cronet/android/.+"),
        ],
    ),
)

ios_builder(
    name = "ios-simulator-full-configs",
    branch_selector = branches.selector.IOS_BRANCHES,
    mirrors = [
        "ci/ios-simulator-full-configs",
    ],
    cpu = cpu.ARM64,
    coverage_exclude_sources = "ios_test_files_and_test_utils",
    coverage_test_types = ["overall", "unit"],
    main_list_view = "try",
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "ios/.+",
        ],
    ),
    use_clang_coverage = True,
)

ios_builder(
    name = "ios-simulator-inverse-fieldtrials-fyi",
    mirrors = builder_config.copy_from("try/ios-simulator"),
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios-simulator-multi-window",
    mirrors = ["ci/ios-simulator-multi-window"],
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios-simulator-noncq",
    mirrors = [
        "ci/ios-simulator-noncq",
    ],
    cpu = cpu.ARM64,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
    tryjob = try_.job(
        location_filters = [
            "third_party/crashpad/crashpad/.+",
        ],
    ),
)

ios_builder(
    name = "ios-wpt-fyi-rel",
    mirrors = [
        "ci/ios-wpt-fyi-rel",
    ],
)

ios_builder(
    name = "ios16-beta-simulator",
    mirrors = [
        "ci/ios16-beta-simulator",
    ],
    os = os.MAC_13,
    cpu = cpu.ARM64,
    reclient_jobs = reclient.jobs.LOW_JOBS_FOR_CQ,
)

ios_builder(
    name = "ios16-sdk-simulator",
    mirrors = [
        "ci/ios16-sdk-simulator",
    ],
    os = os.MAC_14,
    cpu = cpu.ARM64,
    xcode = xcode.x15betabots,
)

ios_builder(
    name = "ios17-beta-simulator",
    mirrors = ["ci/ios17-beta-simulator"],
    os = os.MAC_13,
    cpu = cpu.ARM64,
)

ios_builder(
    name = "ios17-sdk-simulator",
    mirrors = ["ci/ios17-sdk-simulator"],
    os = os.MAC_13,
    cpu = cpu.ARM64,
    xcode = xcode.x15betabots,
)

ios_builder(
    name = "ios-simulator-code-coverage",
    mirrors = ["ci/ios-simulator-code-coverage"],
    builderless = True,
    execution_timeout = 20 * time.hour,
)

try_.gpu.optional_tests_builder(
    name = "mac_optional_gpu_tests_rel",
    branch_selector = branches.selector.IOS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "angle_internal",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
        ),
        build_gs_bucket = "chromium-gpu-fyi-archive",
    ),
    ssd = None,
    main_list_view = "try",
    tryjob = try_.job(
        location_filters = [
            # Inclusion filters.
            cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
            cq.location_filter(path_regexp = "content/browser/xr/.+"),
            cq.location_filter(path_regexp = "content/test/gpu/.+"),
            cq.location_filter(path_regexp = "gpu/.+"),
            cq.location_filter(path_regexp = "media/audio/.+"),
            cq.location_filter(path_regexp = "media/base/.+"),
            cq.location_filter(path_regexp = "media/capture/.+"),
            cq.location_filter(path_regexp = "media/filters/.+"),
            cq.location_filter(path_regexp = "media/gpu/.+"),
            cq.location_filter(path_regexp = "media/mojo/.+"),
            cq.location_filter(path_regexp = "media/renderers/.+"),
            cq.location_filter(path_regexp = "media/video/.+"),
            cq.location_filter(path_regexp = "services/shape_detection/.+"),
            cq.location_filter(path_regexp = "testing/buildbot/tryserver.chromium.mac.json"),
            cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
            cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
            cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
            cq.location_filter(path_regexp = "tools/mb/mb_config_expectations/tryserver.chromium.mac.json"),
            cq.location_filter(path_regexp = "ui/gl/.+"),

            # Exclusion filters.
            cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
        ],
    ),
)
