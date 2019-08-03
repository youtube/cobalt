// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"

#if defined(__LB_SHELL__)
#include "base/at_exit.h"
#include "base/command_line.h"
#include "lb_shell_platform_delegate.h"
#endif

#if defined(__LB_SHELL__FOR_RELEASE__)
#error You cannot build unit tests in gold builds.
#endif

#if defined(__LB_SHELL__)
base::AtExitManager* platform_at_exit_manager_;

MainHook::MainHook(MainType main_func, int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  platform_at_exit_manager_ = new base::AtExitManager();
  LBShellPlatformDelegate::Init();
}
MainHook::~MainHook() {
  LBShellPlatformDelegate::Teardown();
  delete platform_at_exit_manager_;
}
#elif !defined(OS_IOS)
MainHook::MainHook(MainType main_func, int argc, char* argv[]) {}
MainHook::~MainHook() {}
#endif
