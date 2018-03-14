// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines helpful methods for dealing with Callbacks.  Because Callbacks
// are implemented using templates, with a class per callback signature, adding
// methods to Callback<> itself is unattractive (lots of extra code gets
// generated).  Instead, consider adding methods here.
//
// ResetAndReturn(&cb) is like cb.Reset() but allows executing a callback (via a
// copy) after the original callback is Reset().  This can be handy if Run()
// reads/writes the variable holding the Callback.

#ifndef BASE_CALLBACK_HELPERS_H_
#define BASE_CALLBACK_HELPERS_H_

#include "base/callback.h"

namespace base {

template <typename Sig>
base::Callback<Sig> ResetAndReturn(base::Callback<Sig>* cb) {
  base::Callback<Sig> ret(*cb);
  cb->Reset();
  return ret;
}

inline bool ResetAndRunIfNotNull(base::Closure* cb) {
  if (cb->is_null()) {
    return false;
  }
  base::Closure ret(*cb);
  cb->Reset();
  ret.Run();
  return true;
}

template <typename Sig, typename... ParamTypes>
bool ResetAndRunIfNotNull(base::Callback<Sig>* cb,
                          const ParamTypes&... params) {
  if (cb->is_null()) {
    return false;
  }
  base::Callback<Sig> ret(*cb);
  cb->Reset();
  ret.Run(params...);
  return true;
}

}  // namespace base

#endif  // BASE_CALLBACK_HELPERS_H_
