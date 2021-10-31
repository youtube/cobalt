// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
#define COBALT_MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

// This is a helper utility for base::Bind()ing callbacks to the current
// base::MessageLoop. The typical use is when |a| (of class |A|) wants to hand a
// callback such as base::Bind(&A::AMethod, a) to |b|, but needs to ensure that
// when |b| executes the callback, it does so on |a|'s current
// base::MessageLoop.
//
// Typical usage: request to be called back on the current thread:
// other->StartAsyncProcessAndCallMeBack(
//    media::BindToCurrentLoop(base::Bind(&MyClass::MyMethod, this)));
//
// Note that like base::Bind(), BindToCurrentLoop() can't bind non-constant
// references, and that *unlike* base::Bind(), BindToCurrentLoop() makes copies
// of its arguments, and thus can't be used with arrays.

namespace cobalt {
namespace media {

// Mimic base::internal::CallbackForward, replacing std::move(p) with
// base::Passed(&p) to account for the extra layer of indirection.
namespace internal {
template <typename T>
T& TrampolineForward(T& t) {
  return t;
}

template <typename T, typename R>
base::internal::PassedWrapper<std::unique_ptr<T>> TrampolineForward(
    std::unique_ptr<T>& p) {
  return base::Passed(&p);
}

template <typename T>
base::internal::PassedWrapper<std::vector<std::unique_ptr<T>>>
TrampolineForward(std::vector<std::unique_ptr<T>>& p) {
  return base::Passed(&p);
}

// First, tell the compiler TrampolineHelper is a struct template with one
// type parameter.  Then define specializations where the type is a function
// returning void and taking zero or more arguments.
template <typename Sig>
struct TrampolineHelper;

template <>
struct TrampolineHelper<void()> {
  static void Run(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::Closure& cb) {
    task_runner->PostTask(FROM_HERE, cb);
  }
};

template <typename A1>
struct TrampolineHelper<void(A1)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1)>& cb, A1 a1) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1)));
  }
};

template <typename A1, typename A2>
struct TrampolineHelper<void(A1, A2)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1, A2)>& cb, A1 a1, A2 a2) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1),
                                         internal::TrampolineForward(a2)));
  }
};

template <typename A1, typename A2, typename A3>
struct TrampolineHelper<void(A1, A2, A3)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1, A2, A3)>& cb, A1 a1, A2 a2,
                  A3 a3) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1),
                                         internal::TrampolineForward(a2),
                                         internal::TrampolineForward(a3)));
  }
};

template <typename A1, typename A2, typename A3, typename A4>
struct TrampolineHelper<void(A1, A2, A3, A4)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1, A2, A3, A4)>& cb, A1 a1, A2 a2,
                  A3 a3, A4 a4) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1),
                                         internal::TrampolineForward(a2),
                                         internal::TrampolineForward(a3),
                                         internal::TrampolineForward(a4)));
  }
};

template <typename A1, typename A2, typename A3, typename A4, typename A5>
struct TrampolineHelper<void(A1, A2, A3, A4, A5)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1, A2, A3, A4, A5)>& cb, A1 a1,
                  A2 a2, A3 a3, A4 a4, A5 a5) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1),
                                         internal::TrampolineForward(a2),
                                         internal::TrampolineForward(a3),
                                         internal::TrampolineForward(a4),
                                         internal::TrampolineForward(a5)));
  }
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
          typename A6>
struct TrampolineHelper<void(A1, A2, A3, A4, A5, A6)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1, A2, A3, A4, A5, A6)>& cb, A1 a1,
                  A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1),
                                         internal::TrampolineForward(a2),
                                         internal::TrampolineForward(a3),
                                         internal::TrampolineForward(a4),
                                         internal::TrampolineForward(a5),
                                         internal::TrampolineForward(a6)));
  }
};

template <typename A1, typename A2, typename A3, typename A4, typename A5,
          typename A6, typename A7>
struct TrampolineHelper<void(A1, A2, A3, A4, A5, A6, A7)> {
  static void Run(const scoped_refptr<base::SingleThreadTaskRunner>& loop,
                  const base::Callback<void(A1, A2, A3, A4, A5, A6, A7)>& cb,
                  A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) {
    loop->PostTask(FROM_HERE, base::Bind(cb, internal::TrampolineForward(a1),
                                         internal::TrampolineForward(a2),
                                         internal::TrampolineForward(a3),
                                         internal::TrampolineForward(a4),
                                         internal::TrampolineForward(a5),
                                         internal::TrampolineForward(a6),
                                         internal::TrampolineForward(a7)));
  }
};

}  // namespace internal

template <typename T>
static base::Callback<T> BindToCurrentLoop(const base::Callback<T>& cb) {
  return base::Bind(&internal::TrampolineHelper<T>::Run,
                    base::ThreadTaskRunnerHandle::Get(), cb);
}

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_BIND_TO_CURRENT_LOOP_H_
