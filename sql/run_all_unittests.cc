// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"
#include "base/test/test_suite.h"
#include "sql/test_vfs.h"

int main(int argc, char** argv) {
  MainHook hook(main, argc, argv);
  sql::RegisterTestVfs();
  int result = base::TestSuite(argc, argv).Run();
  sql::UnregisterTestVfs();
  return result;
}
