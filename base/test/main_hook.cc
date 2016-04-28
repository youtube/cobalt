// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"

#if defined(__LB_SHELL__) || defined(COBALT)
#include "base/at_exit.h"
#include "base/command_line.h"
#include "cobalt/deprecated/platform_delegate.h"
#endif

#if defined(__LB_SHELL__FOR_RELEASE__)
#error You cannot build unit tests in gold builds.
#endif

#if defined(__LB_SHELL__) || defined(COBALT)
base::AtExitManager* platform_at_exit_manager_;

MainHook::MainHook(MainType main_func, int argc, char* argv[]) {
  // TODO(uzhilinsky): MainHooks should no longer be used and in fact this
  // class was removed in the recent version of Chromium. Any required
  // initialization logic in tests should be done in TestSuite::Initialize().
  CommandLine::Init(argc, argv);
  platform_at_exit_manager_ = new base::AtExitManager();
  cobalt::deprecated::PlatformDelegate::Init();
}
MainHook::~MainHook() {
  cobalt::deprecated::PlatformDelegate::Teardown();
  delete platform_at_exit_manager_;
}
#elif !defined(OS_IOS)
MainHook::MainHook(MainType main_func, int argc, char* argv[]) {}
MainHook::~MainHook() {}
#endif
