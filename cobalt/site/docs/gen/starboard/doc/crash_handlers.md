---
layout: doc
title: "Installing Crash Handlers in Cobalt"
---
# Installing Crash Handlers in Cobalt

Partners can install Crashpad's crash handlers to create crash reports in the
cache directory. This is done by:

1. Adding the following files to the `starboard_platform` target's sources:

```
'<(DEPTH)/starboard/shared/starboard/crash_handler.cc',
'<(DEPTH)/starboard/shared/starboard/crash_handler.h',
```

2. Handling `kCobaltExtensionCrashHandlerName` in the implementation of
`SbSystemGetExtension`:

```
#include "starboard/system.h"

#include "cobalt/extension/crash_handler.h"
#include "starboard/shared/starboard/crash_handler.h"

...

const void* SbSystemGetExtension(const char* name) {

  ...

  if (SbStringCompareAll(name, kCobaltExtensionCrashHandlerName) == 0) {
    return starboard::common::GetCrashHandlerApi();
  }
  return NULL;
}
```

3. Calling the `third_party::crashpad::wrapper::InstallCrashpadHandler()` hook
directly after installing system crash handlers. On linux, for example, this
could look like:

```
#include "third_party/crashpad/wrapper/wrapper.h"

int main(int argc, char** argv) {
  ...
  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();

  third_party::crashpad::wrapper::InstallCrashpadHandler();

  int result = application.Run(argc, argv);
  ...
}
```
