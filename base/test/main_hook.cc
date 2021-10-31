// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"

#if defined(__LB_SHELL__) || defined(COBALT)
#include "base/at_exit.h"
#include "base/command_line.h"
#endif

#if defined(__LB_SHELL__) || defined(COBALT)
base::AtExitManager* platform_at_exit_manager_;

MainHook::MainHook(MainType main_func, int argc, char* argv[]) {
  // TODO: MainHooks should no longer be used and in fact this
  // class was removed in the recent version of Chromium. Any required
  // initialization logic in tests should be done in TestSuite::Initialize().
  base::CommandLine::Init(argc, argv);
  // platform_at_exit_manager_ = new base::AtExitManager();
}
MainHook::~MainHook() {
  delete platform_at_exit_manager_;
}
#elif !defined(OS_IOS)
MainHook::MainHook(MainType main_func, int argc, char* argv[]) {}
MainHook::~MainHook() {}
#endif
