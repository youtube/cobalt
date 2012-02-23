// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/crash_logging.h"

#import <Foundation/Foundation.h>

namespace base {
namespace mac {

static SetCrashKeyValueFuncPtr g_set_key_func;
static ClearCrashKeyValueFuncPtr g_clear_key_func;

void SetCrashKeyFunctions(SetCrashKeyValueFuncPtr set_key_func,
                          ClearCrashKeyValueFuncPtr clear_key_func) {
  g_set_key_func = set_key_func;
  g_clear_key_func = clear_key_func;
}

void SetCrashKeyValue(NSString* key, NSString* val) {
  if (g_set_key_func)
    g_set_key_func(key, val);
}

void ClearCrashKey(NSString* key) {
  if (g_clear_key_func)
    g_clear_key_func(key);
}

ScopedCrashKey::ScopedCrashKey(NSString* key, NSString* value)
    : crash_key_([key retain]) {
  if (g_set_key_func)
    g_set_key_func(crash_key_, value);
}

ScopedCrashKey::~ScopedCrashKey() {
  if (g_clear_key_func)
    g_clear_key_func(crash_key_);
}

}  // namespace mac
}  // namespace base
