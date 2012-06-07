// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    if (hwnd_) {
      DCHECK(IsWindow(hwnd_));
      DCHECK(hdc_);
    } else {
      // If GetDC(NULL) returns NULL, something really bad has happened, like
      // GDI handle exhaustion.  In this case Chrome is going to behave badly no
      // matter what, so we may as well just force a crash now.
      CHECK(hdc_);
    }
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
// CreateCompatibleDC, CreateDC and CreateIC.
class ScopedCreateDC {
 public:
  ScopedCreateDC() : hdc_(NULL) { }
  explicit ScopedCreateDC(HDC h) : hdc_(h) { }

  ~ScopedCreateDC() {
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

  DISALLOW_COPY_AND_ASSIGN(ScopedCreateDC);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_HDC_H_
