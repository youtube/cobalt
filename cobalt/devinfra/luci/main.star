#!/usr/bin/env lucicfg

RECIPE_CIPD_PACKAGE = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build"
RECIPE_NAME = "ytdevinfra/android_apk"
PROJECT_REPO = "https://github.com/youtube/cobalt"

lucicfg.check_version("1.43.14", "Please update depot_tools")

lucicfg.config(
    config_dir = "generated",
    tracked_files = ["*.cfg"],
    fail_on_warnings = True,
    lint_checks = ["default", "-module-docstring"],
)

luci.project(
    name = "ytdevinfra",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    tricium = "tricium-prod.appspot.com",
    bindings = [
        # Allow owners to submit any task in any pool.
        luci.binding(
            roles = [
                "role/swarming.poolOwner",
                "role/swarming.poolUser",
                "role/swarming.taskTriggerer",
            ],
            groups = [
                "ytdevinfra",
                "mdb/tv-engprod-team",
            ],
        ),

        # Allow any googler to see all bots and tasks there.
        luci.binding(
            roles = "role/swarming.poolViewer",
            groups = "googlers",
        ),

        # Allow any googler to read/validate/reimport the project configs.
        luci.binding(
            roles = "role/configs.developer",
            groups = "googlers",
        ),
    ],
)

# Per-service tweaks.
luci.logdog(gs_bucket = "yt-devinfra-luci")

# Realms with ACLs for corresponding Swarming pools. They are referenced in
# Swarming bot configs as "yt-devinfra-luci:pools/<name>".
luci.realm(name = "pools/ci")
luci.realm(name = "pools/try")
luci.realm(name = "pools/prod")

# Global recipe defaults
luci.recipe.defaults.cipd_version.set("refs/heads/main")
luci.recipe.defaults.use_python3.set(True)

# The try bucket will include builders which work on pre-commit or pre-review
# code.
luci.bucket(name = "try")

# The ci bucket will include builders which work on post-commit code.
luci.bucket(name = "ci")

# The prod bucket will include builders which work on post-commit code and
# generate executable artifacts used by other users or machines.
luci.bucket(name = "prod")

luci.gitiles_poller(
    name = "nightly-trigger",
    bucket = "ci",
    repo = PROJECT_REPO,
    refs = ["refs/heads/main"],
    schedule = "0 2,12,16 * * *",  # 2 am, 12 pm, 4 pm every day (multiple times while testing)
)

luci.recipe(
    name = RECIPE_NAME,
    cipd_package = RECIPE_CIPD_PACKAGE,
    cipd_version = "refs/heads/main",
    use_bbagent = True,
    use_python3 = True,
)

luci.builder(
    name = "cobalt-ci-builder",
    bucket = "ci",
    executable = RECIPE_NAME,
    service_account = "luci-vms@devexprod-reliability.iam.gserviceaccount.com",
    execution_timeout = 1 * time.hour,
    dimensions = {"pool": "luci.ytdevinfra.ci"},
    triggered_by = ["nightly-trigger"],
    build_numbers = True,
)
