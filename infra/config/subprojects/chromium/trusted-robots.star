# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//builder_health_indicators.star", "health_spec")
load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//consoles.star", "consoles")
load("@chromium-luci//swarming.star", "swarming")

# Realm for pool with bots that run as trusted robots.
swarming.pool_realm(
    name = "pools/trusted-robots",
)

luci.bucket(
    name = "trusted.robots",
    constraints = luci.bucket_constraints(
        pools = ["luci.chromium.trusted-robots"],
    ),
    acls = [
        acl.entry(
            roles = [
                acl.BUILDBUCKET_READER,
                acl.SCHEDULER_READER,
            ],
            groups = [
                # TODO(dlf): make this more visible.
                "googlers",
            ],
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = [
                "mdb/chrome-troopers",
            ],
        ),
        acl.entry(
            roles = acl.BUILDBUCKET_TRIGGERER,
            groups = [
                "mdb/chrome-troopers",
            ],
        ),
    ],
)

consoles.console_view(
    name = "chromium.trusted-robots",
)

ci.defaults.set(
    bucket = "trusted.robots",
    builder_group = "chromium.trusted-robots",
    pool = "luci.chromium.trusted-robots",
    builderless = True,
    cores = 8,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    execution_timeout = 1 * time.hour,
    health_spec = health_spec.default(),
)

exec("./trusted-robots/toolchains.star")
