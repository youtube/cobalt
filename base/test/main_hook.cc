// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/main_hook.h"

#if !defined(OS_IOS)
MainHook::MainHook(MainType main_func, int argc, char* argv[]) {}
#endif
