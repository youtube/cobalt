---
layout: doc
title: "GYP to GN Migration Changes"
---
# GYP to GN Migration Changes

This file tracks changes to configuration meta build configuration variables in
the GYP to GN migration. Reference the table below to find the correct GN
equivalent to a changed variable, deprecated GYP variables not in GN, and added
variables.

## Variable Changes

*GYP*                                     | *GN*                                                 | *GN import*
:---------------------------------------- | :--------------------------------------------------- | :----------
`OS` ("starboard"/other)                  | `is_starboard` (true/false)                          | (global)
`clang` (0/1)                             | `is_clang` (true/false)                              | (global)
`has_input_events_filter`                 | `is_internal_build` (true/false)                     | (global)
`has_drm_system_extension`                | `is_internal_build` (true/false)                     | (global)
`has_cdm`                                 | `is_internal_build` (true/false)                     | (global)
`has_private_system_properties`           | `is_internal_build` (true/false)                     | (global)
`sb_deploy_output_dir`                    | `sb_install_output_dir`                              | `//starboard/build/config/base_configuration.gni`
`sb_evergreen` (0/1)                      | `sb_is_evergreen` (true/false)                       | `//starboard/build/config/base_configuration.gni`
`sb_evergreen_compatible` (0/1)           | `sb_is_evergreen_compatible` (true/false)            | `//starboard/build/config/base_configuration.gni`
`sb_evergreen_compatible_libunwind` (0/1) | `sb_evergreen_compatible_use_libunwind` (true/false) | `//starboard/build/config/base_configuration.gni`
`sb_evergreen_compatible_lite` (0/1)      | `sb_evergreen_compatible_enable_lite` (true/false)   | `//starboard/build/config/base_configuration.gni`
`sb_disable_cpp14_audit`                  | (none)                                               |
`sb_disable_microphone_idl`               | (none)                                               |
`starboard_path`                          | (none)                                               |
`tizen_os`                                | (none)                                               |
`includes_starboard`                      | (none)                                               |
(none)                                    | `has_platform_tests` (true/false)                    | `//starboard/build/config/base_configuration.gni`
(none)                                    | `has_platform_targets` (true/false)                  | `//starboard/build/config/base_configuration.gni`
(none)                                    | `install_target_path` (true/false)                   | `//starboard/build/config/base_configuration.gni`

## Other Changes

*GYP*                           | *GN*                                                  | *Notes* (see below)
:------------------------------ | :---------------------------------------------------- | :------------------
`'STARBOARD_IMPLEMENTATION'`    | `"//starboard/build/config:starboard_implementation"` | Starboard Implementation
`optimize_target_for_speed` (0) | `"//starboard/build/config:size"`                     | Optimizations
`optimize_target_for_speed` (1) | `"//starboard/build/config:speed"`                    | Optimizations
`compiler_flags_*_speed`        | `speed_config_path`                                   | Optimizations
`compiler_flags_*_size`         | `size_config_path`                                    | Optimizations
`sb_pedantic_warnings`          | `pedantic_warnings_config_path`                       | Compiler Options
`sb_pedantic_warnings`          | `no_pedantic_warnings_config_path`                    | Compiler Options

Notes:

*   *Starboard Implementation:* If your platform defined
    `STARBOARD_IMPLENTATION` in its implementation, you would now add the above
    config with `configs +=
    ["//starboard/build/config:starboard_implementation"]`.

*   *Optimizations:* Cobalt defaults to building targets to optimize for size.
    If you need to optimize a target for speed, remove the size config and add
    the speed config with `configs -= [ "//starboard/build/config:size" ]` and
    `configs += [ "//starboard/build/config:speed" ]`. You can define these
    configurations for your platform by creating `config`s and pointing to the
    correct ones for `speed_config_path` and `size_config_path` in your
    platform's `platform_configuration/configuration.gni` file.

*   *Compiler Options:* Cobalt compiles some targets with stricter settings
    than others, depending on the platform. Before these targets would opt into
    the stricter settings by settings `sb_pedantic_warnings: 1` in their
    `variables` section. Now they will add the appropriate config like so:
    `configs += [ "//starboard/build/config:pedantic_warnings" ]` and remove
    the default: `configs -= [ "//starboard/build/config:no_pedantic_warnings"
    ]`. The additional config that is used to compile these targets is
    specified with the `pedantic_warnings_config_path` and
    `no_pedantic_warnings_config_path` variables in your platform's
    `platform_configuration/configuration.gni` file.
