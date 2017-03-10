// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_CDM_RESULT_PROMISE_H_
#define COBALT_MEDIA_BLINK_CDM_RESULT_PROMISE_H_

#include <stdint.h>

#include <string>

#include "base/basictypes.h"
#include "cobalt/media/base/cdm_promise.h"
#include "cobalt/media/base/media_keys.h"
#include "cobalt/media/blink/cdm_result_promise_helper.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace cobalt {
namespace media {

// Used to convert a WebContentDecryptionModuleResult into a CdmPromiseTemplate
// so that it can be passed through Chromium. When resolve(T) is called, the
// appropriate complete...() method on WebContentDecryptionModuleResult will be
// invoked. If reject() is called instead,
// WebContentDecryptionModuleResult::completeWithError() is called.
// If constructed with a |uma_name|, CdmResultPromise will report the promise
// result (success or rejection code) to UMA.
template <typename... T>
class CdmResultPromise : public CdmPromiseTemplate<T...> {
 public:
  CdmResultPromise(const blink::WebContentDecryptionModuleResult& result,
                   const std::string& uma_name);
  ~CdmResultPromise() OVERRIDE;

  // CdmPromiseTemplate<T> implementation.
  void resolve(const T&... result) OVERRIDE;
  void reject(MediaKeys::Exception exception_code, uint32_t system_code,
              const std::string& error_message) OVERRIDE;

 private:
  using CdmPromiseTemplate<T...>::IsPromiseSettled;
  using CdmPromiseTemplate<T...>::MarkPromiseSettled;
  using CdmPromiseTemplate<T...>::RejectPromiseOnDestruction;

  blink::WebContentDecryptionModuleResult web_cdm_result_;

  // UMA name to report result to.
  std::string uma_name_;

  DISALLOW_COPY_AND_ASSIGN(CdmResultPromise);
};

template <typename... T>
CdmResultPromise<T...>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& uma_name)
    : web_cdm_result_(result), uma_name_(uma_name) {}

template <typename... T>
CdmResultPromise<T...>::~CdmResultPromise() {
  if (!IsPromiseSettled()) RejectPromiseOnDestruction();
}

// "inline" is needed to prevent multiple definition error.

template <>
inline void CdmResultPromise<>::resolve() {
  MarkPromiseSettled();
  ReportCdmResultUMA(uma_name_, SUCCESS);
  web_cdm_result_.complete();
}

template <typename... T>
void CdmResultPromise<T...>::reject(MediaKeys::Exception exception_code,
                                    uint32_t system_code,
                                    const std::string& error_message) {
  MarkPromiseSettled();
  ReportCdmResultUMA(uma_name_,
                     ConvertCdmExceptionToResultForUMA(exception_code));
  web_cdm_result_.completeWithError(ConvertCdmException(exception_code),
                                    system_code,
                                    blink::WebString::fromUTF8(error_message));
}

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BLINK_CDM_RESULT_PROMISE_H_
