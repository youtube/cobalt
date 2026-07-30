---
name: experimental-code-coverage-build-invoker
description: >-
  Triggers LUCI try jobs to generate code coverage data. Supports Mode 1
  (standard git cl try for source CLs) and Mode 2 (led for custom builds).
---

# Code Coverage Build Invoker

This skill schedules LUCI try jobs to generate code coverage data. It provides
two distinct operational modes based on what is being tested.

## When to Use This Skill

Activate this skill when:

- Triggering verification try jobs on a pending Gerrit CL (`git cl try`).
- Comparing baseline coverage against experimental patchsets.
- Testing local uncommitted recipe modifications or custom build properties
  (`led`).

## Inputs

- `cl_url`: The full URL to the Gerrit CL (e.g.,
  `https://chromium-review.googlesource.com/c/chromium/src/+/7916168`).
- `builders_of_concern`: A list of builder names to trigger (e.g.,
  `["android-x86-rel", "linux-rel"]`).

## Workflow Modes

### Mode 1: Standard CL Verification (`git cl try`)

Use this mode for submitted or WIP Gerrit CLs modifying application source code
(`.cc`, `.java`) or Starlark builder configurations (`.star`).

Execute the command from the repository root:

```bash
git cl try -B luci.chromium.try -b builder1 -b builder2
```

### Mode 2: Low-Level Build Dispatch (`led`)

Use this mode exclusively for debugging unlanded recipe changes, custom build
properties, or reproducing historical Swarming runs.

For each target builder, fetch the builder definition, apply unmerged CL
modifications (or local recipe bundle), explicitly set the Swarming priority to match standard CQ tryjob traffic (`priority = 30`), and dispatch:

```bash
# Example for patching a CL and setting CQ priority via LED
led get-builder luci.chromium.try:android-x86-rel \
  | led edit-gerrit-cl <cl_url> \
  | jq '.buildbucket.bbagent_args.build.infra.backend.config.priority = 30' \
  | led launch
```

#### Example: Testing Local Recipe Modifications
To test unmerged local recipe edits at standard CQ tryjob priority:

```bash
led get-builder luci.chromium.try:android-x86-rel \
  | led edit-recipe-bundle \
  | jq '.buildbucket.bbagent_args.build.infra.backend.config.priority = 30' \
  | led launch
```

## Required Output & State Update

1. Report the Buildbucket build links/URLs for the scheduled jobs mapped to builder name
   (e.g., `{"linux-rel": "https://ci.chromium.org/b/8679163306673603153"}`).
2. Write/update the build links in `scratch/triage_state.json` mapped to builder name under `control_builds` (if `cl_url` matches `control_cl`) or `fix_builds` (if `cl_url` matches `fix_cl`).

## Important Considerations

- **Authentication**: Ensure `gcert` / `luci-auth` tokens are valid.
- **Syntactic Bucket**: Note warnings suggesting `chromium/try`, but
  `-B luci.chromium.try` is syntactically required by `git cl try`.

## References

- [Using LED](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/infra/using_led.md)
- [How to LED](https://g3doc.corp.google.com/company/teams/chrome/ops/luci/how_to_led.md?cl=head)
- [Buildbucket CLI](https://g3doc.corp.google.com/company/teams/chrome/ops/luci/buildbucket/cli.md?cl=head)
