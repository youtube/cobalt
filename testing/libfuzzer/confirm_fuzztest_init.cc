// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "testing/libfuzzer/confirm_fuzztest_init_buildflags.h"
#include "testing/libfuzzer/fuzztest_init_helper.h"
#include "third_party/fuzztest/src/fuzztest/init_fuzztest.h"

// We can't depend on //base, hence not using compiler_specific.h - copied
// from there
#if defined(__clang__)
// clang-format off
#define UNSAFE_BUFFERS(...)                  \
  _Pragma("clang unsafe_buffer_usage begin") \
  __VA_ARGS__                                \
  _Pragma("clang unsafe_buffer_usage end")
// clang-format on
#else
#define UNSAFE_BUFFERS(...) __VA_ARGS__
#endif

namespace {

static void RealInitFunction(int argc, char** argv) {
  static std::vector<std::string> fuzztest_argv_strings;
  static std::vector<char*> fuzztest_argv_data;
  static int fuzztest_argc;
  static char** fuzztest_argv;
  // Fuzztest might refer to the command line later, by which time Chromium
  // may have altered it. Keep our own copy so that the data
  // we pass into fuzztest remains valid, no matter what Chromium code
  // does to the original.
  fuzztest_argv_strings.reserve(argc);
  fuzztest_argv_data.reserve(argc);
  for (int i = 0; i < argc; i++) {
    // SAFETY: this function relies upon paired argc and argv
    // per command-line norms. Spanification of this family
    // of functions has been attempted, but because the
    // provider and consumer of the data both work in terms of
    // old-fashioned argc/argv pairs, it seemed to introduce
    // more complexity than it eliminated.
    UNSAFE_BUFFERS(fuzztest_argv_strings.push_back(argv[i]));
    fuzztest_argv_data.push_back(fuzztest_argv_strings.back().data());
  }
  fuzztest_argc = argc;
  fuzztest_argv = fuzztest_argv_data.data();
  fuzztest::ParseAbslFlags(fuzztest_argc, fuzztest_argv);
#if BUILDFLAG(REGISTER_FUZZTESTS_IN_TEST_SUITES)
  fuzztest::InitFuzzTest(&fuzztest_argc, &fuzztest_argv);
#endif
}

// This code is called from the generic initialization code of any
// unit test suite. Such code is used in test suites containing fuzztests
// and those without. In those without, we want to avoid depending
// on fuzztest's complex dependencies, but on those with fuzztests
// we need to call InitFuzzTest. So, use a static initializer to fill
// in a function pointer in those cases.
class FuzztestInitializer {
 public:
  FuzztestInitializer() {
    fuzztest_init_helper::initialization_function = RealInitFunction;
  }
};

FuzztestInitializer static_initializer;  // NOLINT

}  // namespace
