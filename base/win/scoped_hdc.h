// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HDC_H_
#define BASE_WIN_SCOPED_HDC_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace base {
namespace win {

// The ScopedGetDC and ScopedCreateDC classes manage the default GDI objects
// that are initially selected into a DC. They help you avoid the following
// common mistake:
//
// HDC hdc = GetDC(NULL);
// SelectObject(hdc, new_bitmap);
// .. drawing code here ..
// ReleaseDC(hdc);   <--- error: the DC has a custom object still selected!
//
// This code should be:
//
// HDC hdc = GetDC(NULL);
// HGDIOBJ old_obj = SelectObject(hdc, new_bitmap);
// .. drawing code here ..
// SelectObject(hdc, old_obj);
// ReleaseDC(hdc);    <--- ok to release now.
//
// But why work so hard? Use our handy classes:
//
// ScopedGetDC dc(NULL);
// dc.SelectBitmap(hdc, new_bitmap);
// .. drawing here
// .. when dc goes out of scope it will select the original object before
// .. being released.
//
class ScopedDC {
 public:
  virtual ~ScopedDC();

  virtual void DisposeDC(HDC hdc) = 0;

  HDC get() { return hdc_; }

  void SelectBitmap(HBITMAP bitmap);
  void SelectFont(HFONT font);
  void SelectBrush(HBRUSH brush);
  void SelectPen(HPEN pen);
  void SelectRegion(HRGN region);

 protected:
  ScopedDC(HDC hdc);
  void Close();
  void Reset(HDC hdc);

 private:
  void ResetObjects();
  void Select(HGDIOBJ object, HGDIOBJ* holder);

  HDC hdc_;
  HGDIOBJ bitmap_;
  HGDIOBJ font_;
  HGDIOBJ brush_;
  HGDIOBJ pen_;
  HGDIOBJ region_;
};

// Creates and manages an HDC obtained by GetDC.
class ScopedGetDC : public ScopedDC  {
 public:
  explicit ScopedGetDC(HWND hwnd);
  virtual ~ScopedGetDC();

 private:
  virtual void DisposeDC(HDC hdc) OVERRIDE;

  HWND hwnd_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGetDC);
};

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC, CreateDC and CreateIC.
class ScopedCreateDC : public ScopedDC {
 public:
  ScopedCreateDC();
  explicit ScopedCreateDC(HDC hdc);
  virtual ~ScopedCreateDC();
  void Set(HDC hdc);

 private:
  virtual void DisposeDC(HDC hdc) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ScopedCreateDC);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_SCOPED_HDC_H_
