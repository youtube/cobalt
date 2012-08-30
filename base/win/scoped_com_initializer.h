// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_COM_INITIALIZER_H_
#define BASE_WIN_SCOPED_COM_INITIALIZER_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)

#include <objbase.h>

namespace base {
namespace win {

// Initializes COM in the constructor (STA or MTA), and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  // Enum value provided to initialize the thread as an MTA instead of STA.
  enum SelectMTA { kMTA };

  // Constructor for STA initialization.
  ScopedCOMInitializer() {
    Initialize(COINIT_APARTMENTTHREADED);
  }

  // Constructor for MTA initialization.
  explicit ScopedCOMInitializer(SelectMTA mta) {
    Initialize(COINIT_MULTITHREADED);
  }

  ~ScopedCOMInitializer() {
#ifndef NDEBUG
    // Using the windows API directly to avoid dependency on platform_thread.
    DCHECK_EQ(GetCurrentThreadId(), thread_id_);
#endif
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

  bool succeeded() const { return SUCCEEDED(hr_); }

 private:
  void Initialize(COINIT init) {
#ifndef NDEBUG
    thread_id_ = GetCurrentThreadId();
#endif
    hr_ = CoInitializeEx(NULL, init);
#ifndef NDEBUG
    switch (hr_) {
      case S_FALSE:
        LOG(ERROR) << "Multiple CoInitialize() called for thread "
                   << thread_id_;
        break;
      case RPC_E_CHANGED_MODE:
        DCHECK(false) << "Invalid COM thread model change";
        break;
      default:
        break;
    }
#endif
  }

  HRESULT hr_;
#ifndef NDEBUG
  // In debug builds we use this variable to catch a potential bug where a
  // ScopedCOMInitializer instance is deleted on a different thread than it
  // was initially created on.  If that ever happens it can have bad
  // consequences and the cause can be tricky to track down.
  DWORD thread_id_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

}  // namespace win
}  // namespace base

#else

namespace base {
namespace win {

// Do-nothing class for other platforms.
class ScopedCOMInitializer {
 public:
  enum SelectMTA { kMTA };
  ScopedCOMInitializer() {}
  explicit ScopedCOMInitializer(SelectMTA mta) {}
  ~ScopedCOMInitializer() {}

  bool succeeded() const { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

}  // namespace win
}  // namespace base

#endif

#endif  // BASE_WIN_SCOPED_COM_INITIALIZER_H_
