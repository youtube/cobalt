#!/usr/bin/env lucicfg

RECIPE_CIPD_PACKAGE = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build"
PROJECT_REPO = "https://lbshell-internal.googlesource.com/cobalt_src"

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
    swarming = "chrome-swarming.appspot.com",
    acls = [
        # Publicly readable.
        acl.entry(
            roles = [
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = "all",
        ),
    ],
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
    # TODO(b/735809862): Replace this line to a single run every night once implementation is done
    schedule = "0 */1 * * *",  # Run once every hour
)

luci.recipe(
    name = "chrobalt-nightly",
    recipe = "ytdevinfra/android_apk",
    cipd_package = RECIPE_CIPD_PACKAGE,
    cipd_version = "refs/heads/main",
    use_bbagent = True,
    use_python3 = True,
)

luci.builder(
    name = "cobalt-ci-builder",
    bucket = "ci",
    executable = "chrobalt-nightly",
    service_account = "luci-vms@devexprod-reliability.iam.gserviceaccount.com",
    execution_timeout = 45 * time.minute,
    dimensions = {"pool": "luci.ytdevinfra.ci"},
    triggered_by = ["nightly-trigger"],
    build_numbers = True,
)
