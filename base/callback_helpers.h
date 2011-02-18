// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions and classes that help the
// implementation, and management of the Callback objects.

#ifndef BASE_CALLBACK_HELPERS_H_
#define BASE_CALLBACK_HELPERS_H_
#pragma once

#include "base/ref_counted.h"

namespace base {
namespace internal {

// InvokerStorageBase is used to provide an opaque handle that the Callback
// class can use to represent a function object with bound arguments.  It
// behaves as an existential type that is used by a corresponding
// DoInvoke function to perform the function execution.  This allows
// us to shield the Callback class from the types of the bound argument via
// "type erasure."
class InvokerStorageBase : public RefCountedThreadSafe<InvokerStorageBase> {
 protected:
  friend class RefCountedThreadSafe<InvokerStorageBase>;
  virtual ~InvokerStorageBase() {}
};

// This structure exists purely to pass the returned |invoker_storage_| from
// Bind() to Callback while avoiding an extra AddRef/Release() pair.
//
// To do this, the constructor of Callback<> must take a const-ref.  The
// reference must be to a const object otherwise the compiler will emit a
// warning about taking a reference to a temporary.
//
// Unfortunately, this means that the internal |invoker_storage_| field must
// be made mutable.
template <typename T>
struct InvokerStorageHolder {
  explicit InvokerStorageHolder(T* invoker_storage)
      : invoker_storage_(invoker_storage) {
  }

  mutable scoped_refptr<InvokerStorageBase> invoker_storage_;
};

template <typename T>
InvokerStorageHolder<T> MakeInvokerStorageHolder(T* o) {
  return InvokerStorageHolder<T>(o);
}

}  // namespace internal
}  // namespace base

#endif  // BASE_CALLBACK_HELPERS_H_
