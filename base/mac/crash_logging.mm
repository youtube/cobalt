// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/crash_logging.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"

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

void SetCrashKeyFromAddresses(NSString* key,
                              const void* const* addresses,
                              size_t count) {
  NSString* value = @"<null>";
  if (addresses && count) {
    const size_t kBreakpadValueMax = 255;

    NSMutableArray* hexBacktrace = [NSMutableArray arrayWithCapacity:count];
    size_t length = 0;
    for (size_t i = 0; i < count; ++i) {
      NSString* s = [NSString stringWithFormat:@"%p", addresses[i]];
      length += 1 + [s length];
      if (length > kBreakpadValueMax)
        break;
      [hexBacktrace addObject:s];
    }
    value = [hexBacktrace componentsJoinedByString:@" "];

    // Warn someone if this exceeds the breakpad limits.
    DCHECK_LE(strlen([value UTF8String]), kBreakpadValueMax);
  }
  base::mac::SetCrashKeyValue(key, value);
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
