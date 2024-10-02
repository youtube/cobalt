// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

int wmain(int argc, wchar_t **argv) {
  if (argc >= 2) {
    if (0 == _wcsicmp(argv[1], L"-child"))
      return sandbox::DispatchCall(argc, argv);
  }

  // Force binary unduplication for crbug.com/959223.
  // If you're reading this, it should be safe to remove.
  DCHECK(argc >= 0);

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv, false,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
