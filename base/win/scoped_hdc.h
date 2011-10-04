// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HDC_H_
#define BASE_WIN_SCOPED_HDC_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {
namespace win {

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// GetDC.
class ScopedGetDC {
 public:
  explicit ScopedGetDC(HWND hwnd)
      : hwnd_(hwnd),
        hdc_(GetDC(hwnd)) {
    DCHECK(!hwnd_ || IsWindow(hwnd_));
    DCHECK(hdc_);
  }

  ~ScopedGetDC() {
    if (hdc_)
      ReleaseDC(hwnd_, hdc_);
  }

  operator HDC() { return hdc_; }

 private:
  HWND hwnd_;
  HDC hdc_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGetDC);
};

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC.
// TODO(yosin) To eliminate confusion with ScopedGetDC, we should rename
// ScopedHDC to ScopedCreateDC.
class ScopedHDC {
 public:
  ScopedHDC() : hdc_(NULL) { }
  explicit ScopedHDC(HDC h) : hdc_(h) { }

  ~ScopedHDC() {
    Close();
  }

  HDC Get() {
    return hdc_;
  }

  void Set(HDC h) {
    Close();
    hdc_ = h;
  }

  operator HDC() { return hdc_; }

 private:
  void Close() {
#ifdef NOGDI
    assert(false);
#else
    if (hdc_)
      DeleteDC(hdc_);
#endif  // NOGDI
  }

  HDC hdc_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHDC);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_HDC_H_
