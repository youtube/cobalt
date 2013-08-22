// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"
#include "base/test/test_suite.h"

#if defined(__LB_SHELL__)

#include "lb_shell/lb_shell_constants.h"

struct Args {
  int argc;
  char** argv;
  int ret_val;
};

static void* RunTests(void* params) {
  Args* args = static_cast<Args*>(params);

  MainHook hook(NULL, args->argc, args->argv);
  args->ret_val = base::TestSuite(args->argc, args->argv).Run();

  return NULL;
}

int main(int argc, char **argv) {
  // Run the actual tests in a helper method run from another thread
  // so that we can control that thread's stack size.
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, kWebKitThreadStackSize);

  pthread_t helper_thread;
  Args args;
  args.argc = argc;
  args.argv = argv;
  args.ret_val = -1;
  pthread_create(&helper_thread, &attributes, RunTests, &args);
  pthread_join(helper_thread, NULL);
  return args.ret_val;
}

#else

int main(int argc, char** argv) {
  MainHook hook(main, argc, argv);
  return base::TestSuite(argc, argv).Run();
}

#endif