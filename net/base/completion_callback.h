// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_CALLBACK_H__
#define NET_BASE_COMPLETION_CALLBACK_H__
#pragma once

#include "base/callback_old.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"

namespace net {

// A callback specialization that takes a single int parameter.  Usually this
// is used to report a byte count or network error code.
typedef Callback1<int>::Type OldCompletionCallback;
typedef base::Callback<void(int)> CompletionCallback;

// Used to implement a OldCompletionCallback.
template <class T>
class OldCompletionCallbackImpl :
    public CallbackImpl< T, void (T::*)(int), Tuple1<int> > {
 public:
  OldCompletionCallbackImpl(T* obj, void (T::* meth)(int))
    : CallbackImpl< T, void (T::*)(int),
                    Tuple1<int> >::CallbackImpl(obj, meth) {
  }
};

typedef base::CancelableCallback<void(int)> CancelableCompletionCallback;

// CancelableOldCompletionCallback is used for completion callbacks
// which may outlive the target for the method dispatch. In such a case, the
// provider of the callback calls Cancel() to mark the callback as
// "canceled". When the canceled callback is eventually run it does nothing
// other than to decrement the refcount to 0 and free the memory.
template <class T>
class CancelableOldCompletionCallback :
    public OldCompletionCallbackImpl<T>,
    public base::RefCounted<CancelableOldCompletionCallback<T> > {
 public:
  CancelableOldCompletionCallback(T* obj, void (T::* meth)(int))
    : OldCompletionCallbackImpl<T>(obj, meth), is_canceled_(false) {
  }

  void Cancel() {
    is_canceled_ = true;
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    if (is_canceled_) {
      base::RefCounted<CancelableOldCompletionCallback<T> >::Release();
    } else {
      OldCompletionCallbackImpl<T>::RunWithParams(params);
    }
  }

 private:
  bool is_canceled_;
};

}  // namespace net

#endif  // NET_BASE_COMPLETION_CALLBACK_H__
