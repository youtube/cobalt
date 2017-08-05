# GYP files and their corresponding GN files

Generally, `foo/bar/bar.gyp` corresponds to `foo/bar/BUILD.gn`, while
`foo/bar/baz.gyp` corresponds to either `foo/bar/baz/BUILD.gn` or
`foo/bar/BUILD.gn` (depending on whether it made sense to combine `baz.gyp` with
`bar.gyp`). `foo/bar/quux.gypi` typically corresponds to
`foo/bar/quux.gni`. Here is a table of irregular correspondences:

GYP File                                            | GN File                                                                   | Notes
--------------------------------------------------- | ------------------------------------------------------------------------- |------
build/common.gypi                                   | cobalt/build/config/base.gni, starboard/build/config/base.gni (variables) | A few variables have been omitted, moved to `BUILDCONFIG.gn` instead, or refactored into configs. See the GYP -> GN cookbook for more info.
build/common.gypi                                   | cobalt/build/config/BUILD.gn (target defaults)
cobalt/build/all.gyp                                | BUILD.gn (in root directory)                                              | GN requires this location to be used
cobalt/build/config/base.gypi                       | cobalt/build/config/base.gni, starboard/build/config/base.gni (variables) | See comments for `build/common.gypi`
starboard/linux/shared/compiler_flags.gypi          | starboard/linux/shared/BUILD.gn                                           | "Compiler Defaults" section
starboard/linux/shared/starboard_base_symbolize.gyp | starboard/linux/shared/BUILD.gn                                           | "starboard_platform Target" section
starboard/linux/shared/starboard_platform.gypi      | starboard/linux/shared/BUILD.gn                                           | "starboard_platform Target" section
starboard/linux/x64x11/libraries.gypi               | starboard/linux/x64x11/BUILD.gn                                           | `libs` variable of the `compiler_defaults` config
starboard/starboard_base_target.gypi                | starboard/build/config/BUILD.gn                                           | "Compiler Defaults" section
