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

// Initializes COM in the constructor (STA), and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  ScopedCOMInitializer() : hr_(CoInitialize(NULL)) {
  }

  ScopedCOMInitializer::~ScopedCOMInitializer() {
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

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
  ScopedCOMInitializer() {}
  ~ScopedCOMInitializer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

}  // namespace win
}  // namespace base

#endif

#endif  // BASE_WIN_SCOPED_COM_INITIALIZER_H_
