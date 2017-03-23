// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_CDM_CALLBACK_PROMISE_H_
#define COBALT_MEDIA_BASE_CDM_CALLBACK_PROMISE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "cobalt/media/base/cdm_promise.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_keys.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

typedef base::Callback<void(
    MediaKeys::Exception exception_code, uint32_t system_code,
    const std::string& error_message)> PromiseRejectedCB;

template <typename... T>
class MEDIA_EXPORT CdmCallbackPromise : public CdmPromiseTemplate<T...> {
 public:
  CdmCallbackPromise(const base::Callback<void(const T&...)>& resolve_cb,
                     const PromiseRejectedCB& reject_cb);
  virtual ~CdmCallbackPromise();

  // CdmPromiseTemplate<T> implementation.
  virtual void resolve(const T&... result) OVERRIDE;
  virtual void reject(MediaKeys::Exception exception_code, uint32_t system_code,
                      const std::string& error_message) OVERRIDE;

 private:
  using CdmPromiseTemplate<T...>::IsPromiseSettled;
  using CdmPromiseTemplate<T...>::MarkPromiseSettled;
  using CdmPromiseTemplate<T...>::RejectPromiseOnDestruction;

  base::Callback<void(const T&...)> resolve_cb_;
  PromiseRejectedCB reject_cb_;

  DISALLOW_COPY_AND_ASSIGN(CdmCallbackPromise);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_CDM_CALLBACK_PROMISE_H_
