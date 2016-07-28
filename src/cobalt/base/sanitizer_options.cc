/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(ADDRESS_SANITIZER)

// Functions returning default options are declared weak in the tools' runtime
// libraries. To make the linker pick the strong replacements for those
// functions from this module, we explicitly force its inclusion by passing
// -Wl,-u_sanitizer_options_link_helper
extern "C" void _sanitizer_options_link_helper() { }

// The callbacks we define here will be called from the sanitizer runtime, but
// aren't referenced from the executable. We must ensure that those
// callbacks are not sanitizer-instrumented, and that they aren't stripped by
// the linker.
#define SANITIZER_HOOK_ATTRIBUTE          \
  extern "C"                              \
  __attribute__((no_sanitize_address))    \
  __attribute__((no_sanitize_memory))     \
  __attribute__((no_sanitize_thread))     \
  __attribute__((visibility("default")))  \
  __attribute__((used))

extern "C" char kLSanDefaultSuppressions[];

char kLSanDefaultSuppressions[] =
    // 16-byte leak from a call to calloc() in this library.
    "leak:egl_gallium.so\n";

SANITIZER_HOOK_ATTRIBUTE const char *__lsan_default_suppressions() {
  return kLSanDefaultSuppressions;
}

#endif  // defined(ADDRESS_SANITIZER)
