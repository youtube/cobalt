# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is used to list trybots in Chromium's commit-queue.cfg that
# are defined in src-internal. Note that the recipe these builders run have
# certain ACL restrictions. For more info, see
# http://go/chromium-cq#internal-builders-on-the-cq.

load("//lib/branches.star", "branches")
load("//lib/try.star", "default_location_filters", "try_")
load("//project.star", "settings")

def chrome_internal_verifier(
        *,
        builder,
        tryjob = None,
        **kwargs):
    """Registers an internal Chrome trybot in Chromium's CQ config

    Args:
      builder: Name of builder in the internal chrome project.
      tryjob - A struct containing the details of the tryjob verifier for the
        builder, obtained by calling the `try.tryjob()` function.
    """
    if tryjob != None:
        location_filters = tryjob.location_filters
        if tryjob.add_default_filters:
            location_filters = (location_filters or []) + default_location_filters()

        branches.cq_tryjob_verifier(
            builder = "{}:try/{}".format(settings.chrome_project, builder),
            cancel_stale = tryjob.cancel_stale,
            cq_group = "cq",
            disable_reuse = tryjob.disable_reuse,
            experiment_percentage = tryjob.experiment_percentage,
            location_filters = location_filters,
            result_visibility = cq.COMMENT_LEVEL_RESTRICTED,
        )
    else:
        branches.cq_tryjob_verifier(
            builder = "{}:try/{}".format(settings.chrome_project, builder),
            cq_group = "cq",
            includable_only = True,
            owner_whitelist = [
                "googlers",
                "project-chromium-robot-committers",
            ],
            result_visibility = cq.COMMENT_LEVEL_RESTRICTED,
            **kwargs
        )

### Mandatory builders ###

chrome_internal_verifier(
    builder = "internal-cq-builder-verifier",
    tryjob = try_.job(
        add_default_filters = False,
        location_filters = ["infra/config/generated/cq-usage/full.cfg"],
    ),
)

chrome_internal_verifier(
    builder = "linux-chromeos-compile-chrome",
    tryjob = try_.job(),
)

chrome_internal_verifier(
    builder = "win-branded-compile-rel",
    tryjob = try_.job(),
)

### Optional builders ###

chrome_internal_verifier(
    builder = "android-internal-binary-size",
)

chrome_internal_verifier(
    builder = "android-internal-rel",
)

chrome_internal_verifier(
    builder = "chromeos-betty-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-betty-pi-arc-cfi-thin-lto-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-betty-pi-arc-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-eve-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-jacuzzi-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-jacuzzi-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-octopus-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-octopus-compile-chrome",
)

chrome_internal_verifier(
    builder = "chromeos-reven-chrome",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-astro",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-nelson",
)

chrome_internal_verifier(
    builder = "fuchsia-fyi-sherlock",
)

chrome_internal_verifier(
    builder = "fuchsia-internal-images-roller",
)

chrome_internal_verifier(
    builder = "fuchsia-smoke-nelson",
)

chrome_internal_verifier(
    builder = "fuchsia-smoke-sherlock",
)

chrome_internal_verifier(
    builder = "ipad-device",
)

chrome_internal_verifier(
    builder = "iphone-device",
)

chrome_internal_verifier(
    builder = "lacros-amd64-generic-chrome",
)

chrome_internal_verifier(
    branch_selector = branches.selector.CROS_BRANCHES,
    builder = "lacros-amd64-generic-chrome-skylab",
)

chrome_internal_verifier(
    builder = "lacros-arm-generic-chrome",
)

chrome_internal_verifier(
    builder = "lacros-arm-generic-chrome-skylab",
)

chrome_internal_verifier(
    builder = "lacros-arm64-generic-chrome-skylab",
)

chrome_internal_verifier(
    builder = "linux-autofill-captured-sites-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder = "linux-chrome",
)

chrome_internal_verifier(
    builder = "linux-chromeos-chrome",
)

chrome_internal_verifier(
    builder = "linux-nearby-chrome-fyi",
)

chrome_internal_verifier(
    builder = "linux-password-manager-captured-sites-rel",
)

chrome_internal_verifier(
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder = "linux-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-chrome",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-arm-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.MAC_BRANCHES,
    builder = "mac-pgo",
)

chrome_internal_verifier(
    builder = "test-o-emulator",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win-arm64-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win-chrome",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win32-pgo",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win64-chrome",
)

chrome_internal_verifier(
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder = "win64-pgo",
)
