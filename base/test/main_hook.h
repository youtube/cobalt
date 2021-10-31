// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "build/build_config.h"

// Note: this is a legacy file from Cobalt's old copy of Chromium libs.

// Provides a way of running code before gtest-based tests with access to
// argv and argc.
class MainHook {
 public:
  typedef int (*MainType)(int, char* []);
  MainHook(MainType main_func, int argc, char* argv[]);
  ~MainHook();

 private:
  DISALLOW_COPY_AND_ASSIGN(MainHook);
};
