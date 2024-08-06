// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/services/mojo_cdm_promise.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "third_party/chromium/media/base/content_decryption_module.h"
#include "third_party/chromium/media/base/decryptor.h"
#include "third_party/chromium/media/mojo/mojom/content_decryption_module.mojom.h"

namespace media_m96 {

static mojom::CdmPromiseResultPtr GetRejectResult(
    CdmPromise::Exception exception,
    uint32_t system_code,
    const std::string& error_message) {
  mojom::CdmPromiseResultPtr cdm_promise_result(mojom::CdmPromiseResult::New());
  cdm_promise_result->success = false;
  cdm_promise_result->exception = exception;
  cdm_promise_result->system_code = system_code;
  cdm_promise_result->error_message = error_message;
  return cdm_promise_result;
}

template <typename F, typename... T>
MojoCdmPromise<F, T...>::MojoCdmPromise(CallbackType callback)
    : callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

template <typename F, typename... T>
MojoCdmPromise<F, T...>::~MojoCdmPromise() {
  if (IsPromiseSettled())
    return;

  DCHECK(!callback_.is_null());
  RejectPromiseOnDestruction();
}

template <typename F, typename... T>
void MojoCdmPromise<F, T...>::resolve(const T&... result) {
  MarkPromiseSettled();
  mojom::CdmPromiseResultPtr cdm_promise_result(mojom::CdmPromiseResult::New());
  cdm_promise_result->success = true;
  std::move(callback_).Run(std::move(cdm_promise_result), result...);
}

template <typename F, typename... T>
void MojoCdmPromise<F, T...>::reject(CdmPromise::Exception exception,
                                     uint32_t system_code,
                                     const std::string& error_message) {
  MarkPromiseSettled();
  std::move(callback_).Run(
      GetRejectResult(exception, system_code, error_message), T()...);
}

template class MojoCdmPromise<void(mojom::CdmPromiseResultPtr)>;
template class MojoCdmPromise<void(mojom::CdmPromiseResultPtr,
                                   CdmKeyInformation::KeyStatus),
                              CdmKeyInformation::KeyStatus>;
template class MojoCdmPromise<void(mojom::CdmPromiseResultPtr,
                                   const std::string&),
                              std::string>;

}  // namespace media_m96
