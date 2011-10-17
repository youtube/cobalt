// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_COM_INITIALIZER_H_
#define BASE_WIN_SCOPED_COM_INITIALIZER_H_
#pragma once

#include "base/basictypes.h"
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
  ScopedCOMInitializer() : hr_(CoInitialize(NULL)) {
  }

  // Constructor for MTA initialization.
  explicit ScopedCOMInitializer(SelectMTA mta)
    : hr_(CoInitializeEx(NULL, COINIT_MULTITHREADED)) {
  }

  ScopedCOMInitializer::~ScopedCOMInitializer() {
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

  bool succeeded() const { return SUCCEEDED(hr_); }

 private:
  HRESULT hr_;

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
