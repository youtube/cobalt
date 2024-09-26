#!/usr/bin/env lucicfg
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
# for information on starlark/lucicfg

load("//lib/branches.star", "branches")
load("//project.star", "settings")

lucicfg.check_version(
    min = "1.40.0",
    message = "Update depot_tools",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

# Tell lucicfg what files it is allowed to touch
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "builders/*/*/*",
        "cq-builders.md",
        "cq-usage/default.cfg",
        "cq-usage/full.cfg",
        "health-specs/health-specs.json",
        "luci/commit-queue.cfg",
        "luci/cr-buildbucket.cfg",
        "luci/luci-analysis.cfg",
        "luci/luci-logdog.cfg",
        "luci/luci-milo.cfg",
        "luci/luci-notify.cfg",
        "luci/luci-notify/email-templates/*.template",
        "luci/luci-scheduler.cfg",
        "luci/project.cfg",
        "luci/realms.cfg",
        "luci/tricium-prod.cfg",
        "outages.pyl",
        "sheriff-rotations/*.txt",
        "project.pyl",
        "testing/*.pyl",
    ],
    fail_on_warnings = True,
    lint_checks = [
        "default",
        "-confusing-name",
        "-function-docstring",
        "-function-docstring-args",
        "-function-docstring-return",
        "-function-docstring-header",
        "-module-docstring",
    ],
)

# Just copy tricium-prod.cfg to the generated outputs
lucicfg.emit(
    dest = "luci/tricium-prod.cfg",
    data = io.read_file("tricium-prod.cfg"),
)

# Just copy LUCI Analysis config to generated outputs.
lucicfg.emit(
    dest = "luci/luci-analysis.cfg",
    data = io.read_file("luci-analysis.cfg"),
)

luci.project(
    name = settings.project,
    config_dir = "luci",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    milo = "luci-milo.appspot.com",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-writers",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-chromium-admins",
        ),
    ],
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            groups = [
                "project-chromium-try-task-accounts",
                "project-chromium-ci-task-accounts",
            ],
        ),
        # Roles for LUCI Analysis.
        luci.binding(
            roles = "role/analysis.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/analysis.queryUser",
            groups = "authenticated-users",
        ),
        luci.binding(
            roles = "role/analysis.editor",
            groups = ["project-chromium-committers", "googlers"],
        ),
    ],
)

luci.cq(
    submit_max_burst = 2,
    submit_burst_delay = time.minute,
    status_host = "chromium-cq-status.appspot.com",
)

luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)

luci.milo(
    logo = "https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg",
)

luci.notify(
    tree_closing_enabled = True,
)

# An all-purpose public realm.
luci.realm(
    name = "public",
    bindings = [
        luci.binding(
            roles = "role/buildbucket.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-tryjob-access",
        ),
        # Other roles are inherited from @root which grants them to group:all.
    ],
)

luci.realm(
    name = "ci",
    bindings = [
        # Allow CI builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-ci-task-accounts",
        ),
    ],
)

luci.realm(
    name = "try",
    bindings = [
        # Allow try builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = [
                "project-chromium-try-task-accounts",
                # In order to be able to reproduce test tasks that have
                # ResultDB enabled (at this point that should be all
                # tests), a realm must be provided. The ability to
                # trigger machines in the test pool is associated with
                # the try realm, so allow those who can trigger swarming
                # tasks in that pool tasks to create invocations.
                "chromium-led-users",
                "project-chromium-tryjob-access",
            ],
        ),
    ],
)

luci.realm(
    name = "webrtc",
    bindings = [
        # Allow WebRTC builders to create invocations in their own builds.
        luci.binding(
            roles = "role/resultdb.invocationCreator",
            groups = "project-chromium-ci-task-accounts",
        ),
    ],
)

luci.builder.defaults.test_presentation.set(resultdb.test_presentation(grouping_keys = ["status", "v.test_suite"]))

exec("//swarming.star")

exec("//recipes.star")
exec("//targets/mixins.star")
exec("//targets/targets.star")
exec("//targets/variants.star")

exec("//notifiers.star")

exec("//subprojects/chromium/subproject.star")
exec("//subprojects/chrome/subproject.star")
exec("//subprojects/crossbench/subproject.star")
branches.exec("//subprojects/codesearch/subproject.star")
branches.exec("//subprojects/findit/subproject.star")
branches.exec("//subprojects/flakiness/subproject.star")
branches.exec("//subprojects/goma/subproject.star")
branches.exec("//subprojects/reclient/subproject.star")
branches.exec("//subprojects/reviver/subproject.star")
branches.exec("//subprojects/webrtc/subproject.star")

exec("//generators/cq-usage.star")
branches.exec("//generators/cq-builders-md.star")

exec("//generators/sort-consoles.star")

# Execute validators after eveything except the outage file so that we're
# validating the final non-outages configuration
exec("//validators/builder-group-triggers.star")
exec("//validators/builders-in-consoles.star")

# Execute this file last so that any configuration changes needed for handling
# outages gets final say
exec("//outages/outages.star")
