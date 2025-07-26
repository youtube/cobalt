# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.android builder group."""

load("//lib/builders.star", "os")
load("//lib/branches.star", "branches")
load("//lib/try.star", "try_")
load("//lib/consoles.star", "consoles")
load("//project.star", "PLATFORMS", "platform")
load("../fallback-cq.star", "fallback_cq")

try_.defaults.set(
    pool = try_.DEFAULT_POOL,
    cores = 8,
    os = os.LINUX_DEFAULT,
    list_view = "presubmit",

    # These builders don't run recipes that use the flakiness module, so prevent
    # the property for the flakiness module from being generated
    check_for_flakiness = False,
    check_for_flakiness_with_resultdb = False,
    execution_timeout = 15 * time.minute,
    main_list_view = "try",

    # Default priority for buildbucket is 30, see
    # https://chromium.googlesource.com/infra/infra/+/bb68e62b4380ede486f65cd32d9ff3f1bbe288e4/appengine/cr-buildbucket/creation.py#42
    # This will improve our turnaround time for landing infra/config changes
    # when addressing outages
    priority = 25,
    service_account = try_.DEFAULT_SERVICE_ACCOUNT,
)

consoles.list_view(
    name = "presubmit",
    branch_selector = branches.selector.ALL_BRANCHES,
    title = "presubmit builders",
)

# Errors that this builder would catch would go unnoticed until a project is set
# up on a branch day or even worse when a branch was turned into an LTS branch,
# long after the change has been made, so make it a presubmit builder to ensure
# it's checked with current code. The builder runs in a few minutes and only for
# infra/config changes, so it won't impose a heavy burden on our capacity.
def branch_configs():
    """Get the branch configs to be tested.

    Returns:
      A list of objects that can be used as the value of the "branch_configs"
      property for the branch_configuration/tester recipe. See
      https://chromium.googlesource.com/chromium/tools/build/+/refs/heads/main/recipes/recipes/branch_configuration/tester.proto
      The returned configs will cover the common branch configurations and each
      platform individually.
    """
    return [{
        "name": "standard branch",
        "initialize": {},
    }, {
        "name": "desktop extended stable branch",
        "platform_set": {
            "platforms": [platform.MAC, platform.WINDOWS],
            "gardener_rotation": "chrome_browser_release",
        },
    }] + [{
        "name": p,
        "platform_set": {
            "platforms": [p],
        },
    } for p in PLATFORMS]

try_.presubmit_builder(
    name = "branch-config-verifier",
    executable = "recipe:branch_configuration/tester",
    properties = {
        "branch_script": "infra/config/scripts/branch.py",
        "branch_configs": branch_configs(),
        "starlark_entry_points": ["infra/config/main.star", "infra/config/dev.star"],
    },
    tryjob = try_.job(
        location_filters = ["infra/config/.+"],
    ),
)

try_.presubmit_builder(
    name = "reclient-config-deployment-verifier",
    executable = "recipe:reclient_config_deploy_check/tester",
    properties = {
        "fetch_script": "buildtools/reclient_cfgs/fetch_reclient_cfgs.py",
        "rbe_project": [
            {
                "name": "rbe-chromium-trusted",
                "cfg_file": [
                    "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_linux.cfg",
                    "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_windows.cfg",
                    "buildtools/reclient_cfgs/nacl/rewrapper_linux.cfg",
                ],
            },
        ],
    },
    tryjob = try_.job(
        location_filters = [
            "buildtools/reclient_cfgs/.+",
            "tools/clang/scripts/update.py",
            "DEPS",
        ],
    ),
)

try_.presubmit_builder(
    name = "builder-config-verifier",
    description_html = "checks that builder configs in properties files match the recipe-side configs",
    executable = "recipe:chromium/builder_config_verifier",
    properties = {
        "builder_config_directory": "infra/config/generated/builders",
    },
    tryjob = try_.job(
        location_filters = ["infra/config/generated/builders[^/]+/[^/]+/properties\\.json"],
    ),
)

try_.presubmit_builder(
    name = "targets-config-verifier",
    description_html = "checks that target configs specified in starlark match those specified in //testing/buildbot",
    executable = "recipe:chromium/targets_config_verifier",
    properties = {
        "builder_config_directory": "infra/config/generated/builders",
        "precommit_buckets": ["try"],
    },
    tryjob = try_.job(
        location_filters = ["infra/config/generated/builders/[^/]+/[^/]+/targets/.+\\.json"],
    ),
)

try_.presubmit_builder(
    name = "targets-config-verifier-dev",
    description_html = "checks that target configs specified in starlark for dev builders match those specified in //testing/buildbot",
    executable = "recipe:chromium/targets_config_verifier",
    contact_team_email = "chrome-dev-infra@google.com",
    properties = {
        "builder_config_directory": "infra/config/generated/builders-dev",
        "precommit_buckets": ["try"],
    },
    tryjob = try_.job(
        location_filters = ["infra/config/generated/builders-dev/[^/]+/[^/]+/targets/.+\\.json"],
    ),
)

try_.presubmit_builder(
    name = "gn-args-verifier",
    description_html = "checks that GN args generated by starlark definition match those originally specified in //tools/mb/mb_config.pyl",
    executable = "recipe:chromium/gn_args_verifier",
    contact_team_email = "chrome-browser-infra-team@google.com",
    properties = {
        "gclient_config": "chromium",
        "builder_config_directory": "infra/config/generated/builders",
        "mb_config_paths": ["src/tools/mb/mb_config.pyl"],
    },
    tryjob = try_.job(
        location_filters = ["infra/config/generated/builders/[^/]+/[^/]+/gn-args\\.json"],
    ),
)

try_.presubmit_builder(
    name = "chromium_presubmit",
    branch_selector = branches.selector.ALL_BRANCHES,
    executable = "recipe:presubmit",
    execution_timeout = 40 * time.minute,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480,
        },
        "repo_name": "chromium",
    },
    tryjob = try_.job(),
)

try_.presubmit_builder(
    name = "win-presubmit",
    executable = "recipe:presubmit",
    builderless = True,
    os = os.WINDOWS_DEFAULT,
    ssd = True,
    execution_timeout = 40 * time.minute,
    properties = {
        "$depot_tools/presubmit": {
            "runhooks": True,
            "timeout_s": 480,
        },
        "repo_name": "chromium",
    },
    tryjob = try_.job(),
)

try_.presubmit_builder(
    name = "requires-testing-checker",
    description_html = "prevents CLs that requires testing from landing on branches with no CQ",
    executable = "recipe:requires_testing_checker",
    cq_group = fallback_cq.GROUP,
    tryjob = try_.job(),
)
