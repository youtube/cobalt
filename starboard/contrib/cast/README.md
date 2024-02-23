## Cast Starboard API

The Cast Starboard API is a shared library which contains the portion of
Starboard required to run Cast.

### Customizations

As of Starboard 15, there are public methods required for Cast that are not
already exposed by `libstarboard.so`. As a result, the dedicated
header `cast_starboard_api.h` is omitted from this release, though it may
return in the future.

Cast still requires additional behavior be implemented behind the existing
Starboard APIs. Reference the `Cast TV Integration Guide` for details.

### Reference Implementation

The `cast_starboard_api/samples/` directory contains the reference target
`cast_starboard_api` which can be built when both
`build_with_separate_cobalt_toolchain=true` and `use_contrib_cast=true` are
specified. To generate the target:

```
gn gen out/linux-x64x11_devel --args="target_platform=\"linux-x64x11\" build_with_separate_cobalt_toolchain=true use_contrib_cast=true build_type=\"devel\""
```

To build the target:

```
ninja -C out/linux-x64x11_devel/ cast_starboard_api
```

### Test Suite

Tests for Cast-specific behaviors are not currently included in NPLB or YTS.

A limited test suite, `cast_starboard_api_test`, is provided to ensure the
standalone library can be initialized and a window surface can be created in the
format required by Cast. To build the test suite:

```
ninja -C out/linux-x64x11_devel/ cast_starboard_api_test
```

To run the test suite:

```
./out/linux-x64x11_devel/cast_starboard_api_test_loader
```

### Known Issues

- When `build_type=\"devel\"`, some systems may SB_DCHECK in `NetworkNotifier`.
