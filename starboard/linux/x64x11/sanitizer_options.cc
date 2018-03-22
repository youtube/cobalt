// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Removes gallium leak warnings from x11 GL code, but defines it as a weak
// symbol, so other code can override it if they want to.

#if defined(ADDRESS_SANITIZER)

// Functions returning default options are declared weak in the tools' runtime
// libraries. To make the linker pick the strong replacements for those
// functions from this module, we explicitly force its inclusion by passing
// -Wl,-u_sanitizer_options_link_helper
extern "C" void _sanitizer_options_link_helper() { }

#define SANITIZER_HOOK_ATTRIBUTE          \
  extern "C"                              \
  __attribute__((no_sanitize_address))    \
  __attribute__((no_sanitize_memory))     \
  __attribute__((no_sanitize_thread))     \
  __attribute__((visibility("default")))  \
  __attribute__((weak))                   \
  __attribute__((used))

// Newline separated list of issues to suppress, see
// http://clang.llvm.org/docs/AddressSanitizer.html#issue-suppression
// http://llvm.org/svn/llvm-project/compiler-rt/trunk/lib/sanitizer_common/sanitizer_suppressions.cc
SANITIZER_HOOK_ATTRIBUTE const char* __lsan_default_suppressions() {
  return
      "leak:egl_gallium.so\n"
      "leak:nvidia\n"
      "leak:libspeechd.so\n";
}

#if defined(ASAN_SYMBOLIZER_PATH)
extern "C" const char *__asan_default_options() {
  return "external_symbolizer_path=" ASAN_SYMBOLIZER_PATH;
}
#endif

#endif  // defined(ADDRESS_SANITIZER)
