# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package declaration for the chromium project."""

pkg.declare(
    name = "@chromium-project",
    lucicfg = "1.44.1",
)

pkg.options.lint_checks([
    "default",
    "-confusing-name",
    "-function-docstring",
    "-function-docstring-args",
    "-function-docstring-return",
    "-function-docstring-header",
    "-module-docstring",
])

pkg.entrypoint("main.star")
pkg.entrypoint("dev.star")

pkg.depend(
    name = "@chromium-luci",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "infra/chromium",
        ref = "refs/heads/main",
        path = "starlark-libs/chromium-luci",
        revision = "0286cb7e8d0f8f6b2eb3a9ffcba6421f7364d343",
    ),
)

pkg.resources([
    "dev/chromium-header.textpb",
    "lib/linux-default.json",
    "luci-analysis-dev.cfg",
    "luci-analysis.cfg",
    "luci-bisection-dev.cfg",
    "luci-bisection.cfg",
    "milestones.json",
    "settings.json",
    "targets/autoshard_exceptions.json",
    "templates/build_with_step_summary.template",
    "templates/tree_closure_email.template",
    "testhaus-staging.cfg",
    "testhaus.cfg",
])
