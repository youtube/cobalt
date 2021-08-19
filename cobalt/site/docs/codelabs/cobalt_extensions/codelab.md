---
layout: doc
title: "Cobalt Extensions codelab"
---

The Cobalt Extension framework provides a way to add optional, platform-specific
features to the Cobalt application. A Cobalt Extension is an optional interface
that porters can implement for their platforms if, and as, they wish.

This tutorial uses coding exercises to guide you through the process of creating
a simple example of a Cobalt Extension. By the end you should have a firm
understanding of what Cobalt Extensions are, when to use them instead of
alternatives, how to write them, and how to work with the Cobalt team to
contribute them to the repository.

## Prerequisites

Because it's helpful to build and run Cobalt during the exercises, you'll first
want to set up your environment and make sure you can build Cobalt. You can
follow <a href="/development/setup-linux.html">Set up your environment -
Linux</a> to do this if you're a Linux user. Please note that the exercise
solutions assume you're using Linux but should be comparable to implementations
for other platforms.

Also note that while this codelab doesn't require it, you'll need to
<a href="/starboard/porting.html">Port Cobalt to your platform</a> before you
can actually use a Cobalt Extension to customize it for your platform.

Finally, the exercises assume the ability to program in C and C++.

### Exercise 0: Run Cobalt and inspect logs

Assuming you've already built Cobalt, please now run Cobalt and pay special
attention to a message it logs when it starts up. This message will be the focus
of subsequent exercises.

```
$ out/linux-x64x11_debug/cobalt 2>/dev/null | grep "Starting application"
```

## Background

Situated below Cobalt is Starboard. Starboard, which is a porting layer and OS
abstraction, contains a minimal set of APIs to encapsulate the platform-specific
functionalities that Cobalt uses. Each Starboard module (memory, socket, thread,
etc.) defines functions that must be implemented on a porter's platform,
imposing an implementation and maintenance cost on all porters. With this cost
in mind the Cobalt team tries to keep the Starboard APIs stable and only adds a
new API **when some functionality is required by Cobalt but the implementation
depends on the platform**.

A Starboard API can be made optional, though, by the introduction of an
accompanying API that asks platforms whether they support the underlying feature
and enables Cobalt to check for the answer at runtime. For example,
`SbWindowOnScreenKeyboardIsSupported` is used so that only platforms that
support an on screen keyboard need implement the related functions in
`starboard/window.h`. To spare porters uninterested in the functionality, the
Cobalt team chooses to make a Starboard API optional **when some Cobalt
functionality is optional and the implementation is platform-dependent**.

Finally, a nonobvious point explains why even an optional Starboard API may
sometimes be too cumbersome: other applications beyond Cobalt are able to be run
on top of Starboard. If a feature is needed by Cobalt but not by all Starboard-
based applications or by Starboard itself, adding a Starboard API for it may add
unnecessary size and complexity to the porting layer. **And here we arrive at
the sweet spot for Cobalt Extensions: when the desired functionality is
Cobalt-specific, optional in Cobalt, and has platform-dependent
implementation.** Also, because Cobalt Extensions are lightweight and, as you'll
see below, added without any changes to the Starboard layer, they're the
preferred way for porters to add new, custom features to Cobalt.

To summarize:

<table>
  <tr>
    <th colspan="1">Tool</th>
    <th colspan="1">Use case</th>
    <th colspan="1">Ecosystem cost</th>
  </tr>
  <tr>
    <td>Starboard API</td>
    <td>Feature is <strong>required</strong> but implementation is
    platform-dependent</td>
    <td>High</td>
  </tr>
  <tr>
    <td>Optional Starboard API</td>
    <td>Feature is <strong>optional</strong> and implementation is
    platform-dependent</td>
    <td>Medium</td>
  </tr>
  <tr>
    <td>Cobalt Extension</td>
    <td>Feature is <strong>optional and specific to Cobalt</strong> and
    implementation is platform-dependent</td>
    <td>Low</td>
  </tr>
</table>

As a caveat, please note that for all three of these abstractions the interface
is in Cobalt's open-source repository and therefore visible to other porters.
The implementation, on the other hand, is written and built by porters and so
may be kept private.

Finally, in addition to the alternatives mentioned, porters have in some cases
made local changes to Cobalt, above the Starboard layer, to achieve some
customization or optimization. This has been discouraged by the Cobalt team
because it makes rebasing to future versions of Cobalt more difficult but has
been possible because porters have historically built **both** Cobalt and
Starboard. However, Cobalt is moving toward Evergreen
(<a href="https://cobalt.googlesource.com/cobalt/+/refs/heads/master/src/starboard/doc/evergreen/cobalt_evergreen_overview.md">overview</a>),
an architecture that enables automatic Cobalt updates on devices by separating a
Google-built, Cobalt core shared library from the partner-built Starboard layer
and Cobalt loader app. Because Cobalt core code is built by Google, custom
changes to it are no longer possible for partners using Evergreen.

## Anatomy of a Cobalt Extension

### Extension structures

Cobalt uses a structure to describe the interface for an extension and organizes
the structures in headers under `cobalt/extension/`. The header for a "foo"
extension should be named `foo.h` and the first version of it should contain the
following content, as well as any additional members that provide the "foo"
functionality.

```
#ifndef COBALT_EXTENSION_FOO_H_
#define COBALT_EXTENSION_FOO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionFooName "dev.cobalt.extension.Foo"

typedef struct CobaltExtensionFooApi {
  // Name should be the string |kCobaltExtensionFooName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

} CobaltExtensionFooApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_FOO_H_
```

Please note a few important points about this structure:

*   The first two members must be, in order:
    *   A `const char* |name|`, storing the extension's name.
    *   A `uint32_t |version|`, storing the version number of the extension.
        Extension versioning is discussed later on in this codelab.
*   The following members can be any C types (including custom structures) that
    are useful. Often, these are function pointers.

### Extension access in Cobalt

The `SbSystemGetExtension` function from Starboard's `system` module allows
Cobalt to query for an extension by name. It returns a pointer to the constant,
global instance of the structure implementing the extension with the given name
if it exists, otherwise `NULL`. This function is the only mechanism Cobalt has
to get an extension; the Starboard interface intentionally doesn't have any
functions related to the specific extensions.

The caller in Cobalt must static cast the `const void*` returned by
`SbSystemGetExtension` to a `const CobaltExtensionFooApi*`, or pointer of
whatever type the extension structure type happens to be, before using it. Since
the caller can't be sure whether a platform implements the extension or, if it
does, implements it correctly, it's good defensive programming to check that the
resulting pointer is not `NULL` and that the `name` member in the pointed-to
structure has the same value as `kCobaltExtensionFooName`.

### Extension implementation

Because Cobalt Extensions are platform-dependent, the implementations of an
extension belong in Starboard ports. A Starboard port implements an extension by
defining a constant, global instance of the structure and implementing the
`SbSystemGetExtension` function to return a pointer to it. For our "foo"
extension, an implementation for `custom_platform`'s Starboard port could look
as follows.

`starboard/custom_platform/foo.h` declares a `GetFooApi` accessor for the
structure instance.

```
#ifndef STARBOARD_CUSTOM_PLATFORM_FOO_H_
#define STARBOARD_CUSTOM_PLATFORM_FOO_H_

namespace starboard {
namespace custom_platform {

const void* GetFooApi();

}  // namespace custom_platform
}  // namespace starboard

#endif  // STARBOARD_CUSTOM_PLATFORM_FOO_H_
```

`starboard/custom_platform/foo.cc`, then, defines `GetFooApi`.

```
#include "starboard/custom_platform/foo.h"

#include "cobalt/extension/foo.h"

namespace starboard {
namespace custom_platform {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

const CobaltExtensionFooApi kFooApi = {
    kCobaltExtensionFooName,
    1,  // API version that's implemented.
    // Any additional members are initialized here.
};

}  // namespace

const void* GetFooApi() {
  return &kFooApi;
}

}  // namespace custom_platform
}  // namespace starboard
```

Finally, `starboard/custom_platform/system_get_extension.cc` defines
`SbSystemGetExtension` to wire up the extensions for the platform.

```
#include "starboard/system.h"

#include "cobalt/extension/foo.h"
#include "starboard/common/string.h"
#include "starboard/custom_platform/foo.h"

const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionFooName) == 0) {
    return starboard::custom_platform::GetFooApi();
  }
  // Other conditions here should handle other implemented extensions.

  return NULL;
}
```

Please feel free to browse existing extension implementations in the repository.
For example, the reference Raspberry Pi port implements the `Graphics` extension
across the following files.

*   `starboard/raspi/shared/graphics.h`
*   `starboard/raspi/shared/graphics.cc`
*   `starboard/raspi/shared/system_get_extensions.cc`

### Exercise 1: Write and use your first extension

Now that you've seen the anatomy of a Cobalt Extension it's your turn to write
one of your own. In Exercise 0 we saw that Cobalt logs "Starting application"
when it's started. Please write a `Pleasantry` Cobalt Extension that has a
member of type `const char*` and name `greeting` and make any necessary changes
in `cobalt/browser/main.cc` so that the extension can be used to log a custom
greeting directly after the plain "Starting application." Implement the
extension for Linux, or whichever other platform you'd like, and confirm that
the greeting is logged.

#### Solution to Exercise 1

Click the items below to expand parts of a solution. The `git diff`s are between
the solution and the `master` branch.

<details>
    <summary style="display:list-item">Contents of new
    `cobalt/extension/pleasantry.h` file.</summary>

```
#ifndef COBALT_EXTENSION_PLEASANTRY_H_
#define COBALT_EXTENSION_PLEASANTRY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionPleasantryName "dev.cobalt.extension.Pleasantry"

typedef struct CobaltExtensionPleasantryApi {
  // Name should be the string |kCobaltExtensionPleasantryName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.
  const char* greeting;

} CobaltExtensionPleasantryApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_PLEASANTRY_H_
```

</details>

<details>
    <summary style="display:list-item">Contents of new
    `starboard/linux/shared/pleasantry.h` file.</summary>

```
#ifndef STARBOARD_LINUX_SHARED_PLEASANTRY_H_
#define STARBOARD_LINUX_SHARED_PLEASANTRY_H_

namespace starboard {
namespace shared {

const void* GetPleasantryApi();

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_PLEASANTRY_H_
```

</details>

<details>
    <summary style="display:list-item">Contents of new
    `starboard/linux/shared/pleasantry.cc` file.</summary>

```
#include "starboard/linux/shared/pleasantry.h"

#include "cobalt/extension/pleasantry.h"

namespace starboard {
namespace shared {

namespace {

const char *kGreeting = "Happy debugging!";

const CobaltExtensionPleasantryApi kPleasantryApi = {
    kCobaltExtensionPleasantryName,
    1,
    kGreeting,
};

}  // namespace

const void* GetPleasantryApi() {
  return &kPleasantryApi;
}

}  // namespace shared
}  // namespace starboard
```

</details>

<details>
    <summary style="display:list-item">`git diff
    starboard/linux/shared/starboard_platform.gypi`</summary>

```
@@ -38,6 +38,8 @@
       '<(DEPTH)/starboard/linux/shared/netlink.cc',
       '<(DEPTH)/starboard/linux/shared/netlink.h',
       '<(DEPTH)/starboard/linux/shared/player_components_factory.cc',
+      '<(DEPTH)/starboard/linux/shared/pleasantry.cc',
+      '<(DEPTH)/starboard/linux/shared/pleasantry.h',
       '<(DEPTH)/starboard/linux/shared/routes.cc',
       '<(DEPTH)/starboard/linux/shared/routes.h',
       '<(DEPTH)/starboard/linux/shared/system_get_connection_type.cc',
```

</details>

<details>
    <summary style="display:list-item">`git diff
    starboard/linux/shared/system_get_extensions.cc`</summary>

```
@@ -16,12 +16,14 @@

 #include "cobalt/extension/configuration.h"
 #include "cobalt/extension/crash_handler.h"
+#include "cobalt/extension/pleasantry.h"
 #include "starboard/common/string.h"
 #include "starboard/shared/starboard/crash_handler.h"
 #if SB_IS(EVERGREEN_COMPATIBLE)
 #include "starboard/elf_loader/evergreen_config.h"
 #endif
 #include "starboard/linux/shared/configuration.h"
+#include "starboard/linux/shared/pleasantry.h"

 const void* SbSystemGetExtension(const char* name) {
 #if SB_IS(EVERGREEN_COMPATIBLE)
@@ -41,5 +43,8 @@ const void* SbSystemGetExtension(const char* name) {
   if (strcmp(name, kCobaltExtensionCrashHandlerName) == 0) {
     return starboard::common::GetCrashHandlerApi();
   }
+  if (strcmp(name, kCobaltExtensionPleasantryName) == 0) {
+    return starboard::shared::GetPleasantryApi();
+  }
   return NULL;
 }
```

</details>

<details>
    <summary style="display:list-item">`git diff cobalt/browser/main.cc`
    </summary>

```
@@ -18,7 +18,9 @@
 #include "cobalt/base/wrap_main.h"
 #include "cobalt/browser/application.h"
 #include "cobalt/browser/switches.h"
+#include "cobalt/extension/pleasantry.h"
 #include "cobalt/version.h"
+#include "starboard/system.h"

 namespace {

@@ -77,6 +79,14 @@ void StartApplication(int argc, char** argv, const char* link,
     return;
   }
   LOG(INFO) << "Starting application.";
+  const CobaltExtensionPleasantryApi* pleasantry_extension =
+      static_cast<const CobaltExtensionPleasantryApi*>(
+          SbSystemGetExtension(kCobaltExtensionPleasantryName));
+  if (pleasantry_extension &&
+      strcmp(pleasantry_extension->name, kCobaltExtensionPleasantryName) == 0 &&
+      pleasantry_extension->version >= 1) {
+    LOG(INFO) << pleasantry_extension->greeting;
+  }
 #if SB_API_VERSION >= 13
   DCHECK(!g_application);
   g_application = new cobalt::browser::Application(quit_closure,
```

</details>

## Extension versioning

Cobalt Extensions are themselves extensible, but care must be taken to ensure
that the extension interface in Cobalt and implementation in a platform's port,
which may be built separately, are consistent.

The `version` member, which is always the second member in an extension
structure, indicates which version of the interface the structure describes. In
other words, a `version` of the extension structure corresponds to a specific,
invariant list of members. By convention, the first version of a Cobalt
Extension is version `1` (i.e., one-based indexing, not zero-based).

A new version of the extension can be introduced in the structure declaration by
adding additional members to the end of the declaration and adding a comment to
delineate, e.g., "The fields below this point were added in version 2 or later."
To maintain compatibility and enable Cobalt to correctly index into instances of
the structure, it's important that members are always added at the end of the
structure declaration and that members are never removed. If a member is
deprecated in a later version, this fact should simply be noted with a comment
in the structure declaration.

To implement a new version of the extension, a platform's port should then set
the `version` member to the appropriate value when creating the instance of the
structure, and also initialize all members required for the version.

Finally, any code in Cobalt that uses the extension should guard references to
members with version checks.

### Exercise 2: Version your extension

Add a second version of the `Pleasantry` extension that enables porters to also
log a polite farewell message when the Cobalt application is stopped. To allow
platforms more flexibility, add the new `farewell` member as a pointer to a
function that takes no parameters and returns a `const char*`. Update
`cobalt/browser/main.cc` so that Cobalt, if the platform implements version 2 of
this extension, replaces the "Stopping application." message with a polite
farewell provided by the platform.

To keep things interesting, have the platform's implementation pseudo-randomly
return one of several messages. And, once you've made the changes, build Cobalt
and run it a few times to confirm that the feature behaves as expected.

#### Solution to Exercise 2

Click the items below to expand parts of a solution. The `git diff` is between
the solution and the `master` branch.

<details>
    <summary style="display:list-item">Updated contents of
    `cobalt/extension/pleasantry.h`.</summary>

```
#ifndef COBALT_EXTENSION_PLEASANTRY_H_
#define COBALT_EXTENSION_PLEASANTRY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionPleasantryName "dev.cobalt.extension.Pleasantry"

typedef struct CobaltExtensionPleasantryApi {
  // Name should be the string |kCobaltExtensionPleasantryName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.
  const char* greeting;

  // The fields below this point were added in version 2 or later.
  const char* (*GetFarewell)();

} CobaltExtensionPleasantryApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_PLEASANTRY_H_
```

</details>

<details>
    <summary style="display:list-item">Updated contents of
    `starboard/linux/shared/pleasantry.cc`.</summary>

```
#include "starboard/linux/shared/pleasantry.h"

#include <stdlib.h>

#include "cobalt/extension/pleasantry.h"
#include "starboard/system.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {

namespace {

const char* kGreeting = "Happy debugging!";

const char* kFarewells[] = {
  "Farewell",
  "Take care",
  "Thanks for running Cobalt",
};

const char* GetFarewell() {
  srand (SbTimeGetNow());
  int pseudo_random_index = rand() % SB_ARRAY_SIZE_INT(kFarewells);
  return kFarewells[pseudo_random_index];
}

const CobaltExtensionPleasantryApi kPleasantryApi = {
  kCobaltExtensionPleasantryName,
  2,
  kGreeting,
  &GetFarewell,
};

}  // namespace

const void* GetPleasantryApi() {
  return &kPleasantryApi;
}

}  // namespace shared
}  // namespace starboard
```

</details>

<details>
    <summary style="display:list-item">`git diff cobalt/browser/main.cc`
    </summary>

```
@@ -18,7 +18,9 @@
 #include "cobalt/base/wrap_main.h"
 #include "cobalt/browser/application.h"
 #include "cobalt/browser/switches.h"
+#include "cobalt/extension/pleasantry.h"
 #include "cobalt/version.h"
+#include "starboard/system.h"

 namespace {

@@ -54,6 +56,14 @@ bool CheckForAndExecuteStartupSwitches() {
   return g_is_startup_switch_set;
 }

+// Get the Pleasantry extension if it's implemented.
+const CobaltExtensionPleasantryApi* GetPleasantryApi() {
+  static const CobaltExtensionPleasantryApi* pleasantry_extension =
+      static_cast<const CobaltExtensionPleasantryApi*>(
+          SbSystemGetExtension(kCobaltExtensionPleasantryName));
+  return pleasantry_extension;
+}
+
 void PreloadApplication(int argc, char** argv, const char* link,
                         const base::Closure& quit_closure,
                         SbTimeMonotonic timestamp) {
@@ -77,6 +87,12 @@ void StartApplication(int argc, char** argv, const char* link,
     return;
   }
   LOG(INFO) << "Starting application.";
+  const CobaltExtensionPleasantryApi* pleasantry_extension = GetPleasantryApi();
+  if (pleasantry_extension &&
+      strcmp(pleasantry_extension->name, kCobaltExtensionPleasantryName) == 0 &&
+      pleasantry_extension->version >= 1) {
+    LOG(INFO) << pleasantry_extension->greeting;
+  }
 #if SB_API_VERSION >= 13
   DCHECK(!g_application);
   g_application = new cobalt::browser::Application(quit_closure,
@@ -96,7 +112,14 @@ void StartApplication(int argc, char** argv, const char* link,
 }

 void StopApplication() {
-  LOG(INFO) << "Stopping application.";
+  const CobaltExtensionPleasantryApi* pleasantry_extension = GetPleasantryApi();
+  if (pleasantry_extension &&
+      strcmp(pleasantry_extension->name, kCobaltExtensionPleasantryName) == 0 &&
+      pleasantry_extension->version >= 2) {
+    LOG(INFO) << pleasantry_extension->GetFarewell();
+  } else {
+    LOG(INFO) << "Stopping application.";
+  }
   delete g_application;
   g_application = NULL;
 }
```

</details>

`starboard/linux/shared/pleasantry.h`,
`starboard/linux/shared/starboard_platform.gypi`, and
`starboard/linux/shared/system_get_extensions.cc` should be unchanged from the
Exercise 1 solution.

## Extension testing

Each Cobalt Extension has a test in `cobalt/extension/extension_test.cc` that
tests whether the extension is wired up correctly for the platform Cobalt
happens to be built for.

Since some platforms may not implement a particular extension, these tests begin
by checking whether `SbSystemGetExtension` simply returns `NULL` for the
extension's name. For our `foo` extension, the first few lines may contain the
following.

```
TEST(ExtensionTest, Foo) {
  typedef CobaltExtensionFooApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionFooName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  // Verifications about the global structure instance, if implemented.
}
```

If `SbSystemGetExtension` does not return `NULL`, meaning the platform does
implement the extension, the tests generally verify a few details about the
structure:

*   It has the expected name.
*   Its version is in the range of possible versions for the extension.
*   For whichever version is implemented, any members required for that version
    are present.
*   It's a singleton.

### Exercise 3: Test your extension

You guessed it! Add a test for your new extension to
`cobalt/extension/extension_test.cc`.

Once you've written your test you can execute it to confirm that it passes.
`cobalt/extension/extension.gyp` configures an `extension_test` target to be
built from our `extension_test.cc` source file. We can build that target for our
platform and then run the executable to run the tests.

```
$ cobalt/build/gyp_cobalt linux-x64x11
```

```
$ ninja -C out/linux-x64x11_devel all
```

```
$ out/linux-x64x11_devel/extension_test
```

Tip: because the `extension_test` has type `<(gtest_target_type)`, we can use
`--gtest_filter` to filter the tests that are run. For example, you can run just
your newly added test with `--gtest_filter=ExtensionTest.Pleasantry`.

#### Solution to Exercise 3

<details>
    <summary style="display:list-item">Click here to see a solution for the new
    test.</summary>

```
TEST(ExtensionTest, Pleasantry) {
  typedef CobaltExtensionPleasantryApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionPleasantryName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_GE(extension_api->version, 1u);
  EXPECT_LE(extension_api->version, 2u);

  EXPECT_NE(extension_api->greeting, nullptr);

  if (extension_api->version >= 2) {
    EXPECT_NE(extension_api->GetFarewell, nullptr);
  }

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}
```

</details>

You'll also want to include the header for the extension, i.e., `#include
"cobalt/extension/pleasantry.h"`.

## Contributing a Cobalt Extension

Thanks for taking the time to complete the codelab!

**If you'd like to contribute an actual Cobalt Extension to Cobalt in order to
add some useful functionality for your platform, we encourage you to start a
discussion with the Cobalt team before you begin coding.** To do so, please
[file a feature request](https://issuetracker.google.com/issues/new?component=181120)
for the extension and include the following information:

*   The name of the Cobalt Extension.
*   A description of the extension.
*   Why a Cobalt Extension is the right tool, instead of some alternative.
*   The fact that you'd like to contribute the extension (i.e., write the code)
    rather than rely on the Cobalt team to prioritize, plan, and implement it.

Please file this feature request with the appropriate priority and the Cobalt
team will review the proposal accordingly. If the Cobalt team approves of the
use case and design then a member of the team will assign the feature request
back to you for implementation. At this point, please follow the
<a href="/contributors/index.html">Contributing to Cobalt</a> guide to ensure
your code is compliant and can be reviewed and submitted.
