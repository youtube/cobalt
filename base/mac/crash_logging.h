// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_CRASH_LOGGING_H_
#define BASE_MAC_CRASH_LOGGING_H_

#if __OBJC__
#import "base/memory/scoped_nsobject.h"

@class NSString;
#else
class NSString;
#endif

namespace base {
namespace mac {

typedef void (*SetCrashKeyValueFuncPtr)(NSString*, NSString*);
typedef void (*ClearCrashKeyValueFuncPtr)(NSString*);

// Set the low level functions used to supply crash keys to Breakpad.
void SetCrashKeyFunctions(SetCrashKeyValueFuncPtr set_key_func,
                          ClearCrashKeyValueFuncPtr clear_key_func);

// Set and clear meta information for Minidump.
// IMPORTANT: On OS X, the key/value pairs are sent to the crash server
// out of bounds and not recorded on disk in the minidump, this means
// that if you look at the minidump file locally you won't see them!
void SetCrashKeyValue(NSString* key, NSString* val);
void ClearCrashKey(NSString* key);

#if __OBJC__

class ScopedCrashKey {
 public:
  ScopedCrashKey(NSString* key, NSString* value);
  ~ScopedCrashKey();
 private:
  scoped_nsobject<NSString> crash_key_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCrashKey);
};

#endif  // __OBJC__

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_CRASH_LOGGING_H_
