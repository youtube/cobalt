## Cast Starboard API

The Cast Starboard API is a shared library which contains the portion of
Starboard required to run Cast.

### Customizations

As of Starboard 14, there are public methods required for Cast that are not
already exposed by `libstarboard_platform_group.so`. These methods are declared
in the header `cast_starboard_api.h`, and a sample implementation is provided
in `cast_starboard_api_impl.cc`.

Cast also requires additional behavior be implemented behind the existing
Starboard APIs. Reference the `Cast TV Integration Guide` for details.

### Reference Implementation

The `cast_starboard_api/samples/` directory contains the reference target
`cast_starboard_api` which can be built when `use_contrib_cast=true` is
specified. To generate the target:

```
gn gen out/linux-x64x11_devel --args="target_platform=\"linux-x64x11\" use_contrib_cast=true build_type=\"devel\""
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
./out/linux-x64x11_devel/cast_starboard_api_test
```

### Known Issues

- When `build_type=\"devel\"`, some systems may SB_DCHECK in `NetworkNotifier`.
- On some toolchains, `use_asan=false` may be required to build cleanly.