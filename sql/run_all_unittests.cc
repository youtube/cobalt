// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"
#include "base/test/test_suite.h"

#if defined(__LB_SHELL__)
#include "lb_shell_platform_delegate.h"
#include "lb_stack.h"
#endif

#if defined(__LB_SHELL__FOR_RELEASE__)
#error You cannot build unit tests in gold builds.
#endif

int main(int argc, char** argv) {
#if defined(__LB_SHELL__)
  LBShellPlatformDelegate::PlatformInit();
  LB::SetStackSize();
#endif

  MainHook hook(main, argc, argv);
  return base::TestSuite(argc, argv).Run();
}
