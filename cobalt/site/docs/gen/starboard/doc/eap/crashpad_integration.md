Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Migrating a platform's Crashpad integration

Partners are required to complete some steps to integrate Crashpad into Cobalt.
This doc describes any changes to these integration steps from Cobalt 25 to
Cobalt 26.

## Building Crashpad components

Partners are still required to build the Crashpad handler executable using a
"native_target" toolchain defined by the platform; see
[cobalt_evergreen_overview.md](../evergreen/cobalt_evergreen_overview.md) for
more information.

A change is only needed if your platform's toolchain uses C++17 and you
experience C++17-related errors when building the
`native_target/crashpad_handler` target. In this case you should set
`build_base_with_cpp17 = true` in the toolchain definition. The Crashpad client,
which is linked into Starboard's `loader_app`, also depends on `//base`. So if
you need to assign `build_base_with_cpp17` for your platform's "native_target"
toolchain then you'll likely need to do the same for its "starboard" toolchain.

## Deploying the Crashpad handler

Partners are still required to deploy the Crashpad handler alongside the
`loader_app`. *However*, the Crashpad handler executable should now be placed
inside a "native_target" parent directory. So you should have:

```
├── Directory containing kSbSystemPathExecutableFile
    ├── loader_app <--(kSbSystemPathExecutableFile)
    ├── native_target
        ├── crashpad_handler
```

## CrashHandler Starboard extension

Partners are still required to wire up the shared implementation of the
`CrashHandler` Starboard extension, as described in
[crash_handlers.md](../crash_handlers.md). There is no change or action required
here, but please do note that the shared implementation of the Starboard
extension has been upgraded to version 3.

## Installing Crashpad

In Cobalt 25, platforms were required to explicitly call
`crashpad::InstallCrashpadHandler()` to install Crashpad's signal handlers.
Platform application code should no longer do this, as the
`crashpad::InstallCrashpadHandler()` call has been moved into shared Evergreen
code (see `SbEventHandle` in `starboard/loader_app/loader_app.cc`).
