// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_SCOPED_HANDLE_H_
#define BASE_WIN_SCOPED_HANDLE_H_
#pragma once

#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {
namespace win {

// Generic wrapper for raw handles that takes care of closing handles
// automatically. The class interface follows the style of
// the ScopedStdioHandle class with a few additions:
//   - IsValid() method can tolerate multiple invalid handle values such as NULL
//     and INVALID_HANDLE_VALUE (-1) for Win32 handles.
//   - Receive() method allows to receive a handle value from a function that
//     takes a raw handle pointer only.
template <class Traits>
class GenericScopedHandle {
 public:
  typedef typename Traits::Handle Handle;

  GenericScopedHandle() : handle_(Traits::NullHandle()) {}

  explicit GenericScopedHandle(Handle handle) : handle_(Traits::NullHandle()) {
    Set(handle);
  }

  ~GenericScopedHandle() {
    Close();
  }

  bool IsValid() const {
    return Traits::IsHandleValid(handle_);
  }

  void Set(Handle handle) {
    if (handle_ != handle) {
      Close();

      if (Traits::IsHandleValid(handle)) {
        handle_ = handle;
      }
    }
  }

  Handle Get() const {
    return handle_;
  }

  operator Handle() const {
    return handle_;
  }

  Handle* Receive() {
    DCHECK(!Traits::IsHandleValid(handle_)) << "Handle must be NULL";
    return &handle_;
  }

  // Transfers ownership away from this object.
  Handle Take() {
    Handle temp = handle_;
    handle_ = Traits::NullHandle();
    return temp;
  }

  // Explicitly closes the owned handle.
  void Close() {
    if (Traits::IsHandleValid(handle_)) {
      if (!Traits::CloseHandle(handle_)) {
        NOTREACHED();
      }
      handle_ = Traits::NullHandle();
    }
  }

 private:
  Handle handle_;

  DISALLOW_COPY_AND_ASSIGN(GenericScopedHandle);
};

// The traits class for Win32 handles that can be closed via CloseHandle() API.
class HandleTraits {
 public:
  typedef HANDLE Handle;

  // Closes the handle.
  static bool CloseHandle(HANDLE handle) {
    return ::CloseHandle(handle) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(HANDLE handle) {
    return handle != NULL && handle != INVALID_HANDLE_VALUE;
  }

  // Returns NULL handle value.
  static HANDLE NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HandleTraits);
};

typedef GenericScopedHandle<HandleTraits> ScopedHandle;

}  // namespace win
}  // namespace base

#endif  // BASE_SCOPED_HANDLE_WIN_H_
