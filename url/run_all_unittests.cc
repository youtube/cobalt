// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#include "mojo/core/embedder/embedder.h"  // nogncheck
#endif

#if BUILDFLAG(IS_STARBOARD)
#include "starboard/client_porting/wrap_main/wrap_main.h"

namespace {
int LaunchUnitTests(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

#if !BUILDFLAG(IS_IOS)
  mojo::core::Init();
#endif

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
}

// For the Starboard OS define SbEventHandle as the entry point
SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(LaunchUnitTests);

#else

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

#if !BUILDFLAG(IS_IOS)
  mojo::core::Init();
#endif

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
#endif
