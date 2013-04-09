// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"

#if defined(__LB_SHELL__)
#include "lb_shell_platform_delegate.h"
#include "lb_stack.h"
#endif

#if defined(__LB_SHELL__FOR_RELEASE__)
#error You cannot build unit tests in gold builds.
#endif

#if defined(__LB_SHELL__)
MainHook::MainHook(MainType main_func, int argc, char* argv[]) {
  LB::SetStackSize();
  LBShellPlatformDelegate::Init();
}
MainHook::~MainHook() {
  LBShellPlatformDelegate::Teardown();
}
#elif !defined(OS_IOS)
MainHook::MainHook(MainType main_func, int argc, char* argv[]) {}
MainHook::~MainHook() {}
#endif
