// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "build/build_config.h"

// TODO[johnx]: Deprecate this legacy file.
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
