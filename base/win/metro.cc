// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/metro.h"

#include "base/string_util.h"

namespace base {
namespace win {

const char kActivateApplication[] = "ActivateApplication";

HMODULE GetMetroModule() {
  const HMODULE kUninitialized = reinterpret_cast<HMODULE>(1);
  static HMODULE metro_module = kUninitialized;

  if (metro_module == kUninitialized) {
    // Initialize the cache, note that the initialization is idempotent
    // under the assumption that metro_driver is never unloaded, so the
    // race to this assignment is safe.
    metro_module = GetModuleHandleA("metro_driver.dll");
    if (metro_module != NULL) {
      // This must be a metro process if the metro_driver is loaded.
      DCHECK(IsMetroProcess());
    }
  }

  DCHECK(metro_module != kUninitialized);
  return metro_module;
}

bool IsMetroProcess() {
  enum ImmersiveState {
    kImmersiveUnknown,
    kImmersiveTrue,
    kImmersiveFalse
  };
  typedef BOOL (WINAPI* IsImmersiveProcessFunc)(HANDLE process);

  // The immersive state of a process can never change.
  // Look it up once and cache it here.
  static ImmersiveState state = kImmersiveUnknown;

  if (state == kImmersiveUnknown) {
    // The lookup hasn't been done yet. Note that the code below here is
    // idempotent, so it doesn't matter if it races to assignment on multiple
    // threads.
    HMODULE user32 = ::GetModuleHandleA("user32.dll");
    DCHECK(user32 != NULL);

    IsImmersiveProcessFunc is_immersive_process =
        reinterpret_cast<IsImmersiveProcessFunc>(
            ::GetProcAddress(user32, "IsImmersiveProcess"));

    if (is_immersive_process != NULL) {
      if (is_immersive_process(::GetCurrentProcess())) {
        state = kImmersiveTrue;
      } else {
        state = kImmersiveFalse;
      }
    } else {
      // No "IsImmersiveProcess" export on user32.dll, so this is pre-Windows8
      // and therefore not immersive.
      state = kImmersiveFalse;
    }
  }
  DCHECK_NE(kImmersiveUnknown, state);

  return state == kImmersiveTrue;
}

wchar_t* LocalAllocAndCopyString(const string16& src) {
  size_t dest_size = (src.length() + 1) * sizeof(wchar_t);
  wchar_t* dest = reinterpret_cast<wchar_t*>(LocalAlloc(LPTR, dest_size));
  base::wcslcpy(dest, src.c_str(), dest_size);
  return dest;
}

}  // namespace win
}  // namespace base
