// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_DOM_EME_EME_HELPERS_H_
#define COBALT_DOM_EME_EME_HELPERS_H_

#include <string>
#include <vector>

#include "cobalt/script/script_value_factory.h"
#include "starboard/drm.h"

namespace cobalt {
namespace dom {
namespace eme {

template <typename PromiseValueReferenceType>
void RejectPromise(PromiseValueReferenceType* promise_reference,
                   SbDrmStatus status, const std::string& error_message) {
  switch (status) {
    case kSbDrmStatusSuccess:
      NOTREACHED() << "'kSbDrmStatusSuccess' is not an error.";
      break;
    case kSbDrmStatusTypeError:
      // TODO: Pass |error_message| once we support message on simple errors.
      promise_reference->value().Reject(script::kTypeError);
      break;
    case kSbDrmStatusNotSupportedError:
      promise_reference->value().Reject(
          new DOMException(DOMException::kNotSupportedErr, error_message));
      break;
    case kSbDrmStatusInvalidStateError:
      promise_reference->value().Reject(
          new DOMException(DOMException::kInvalidStateErr, error_message));
      break;
    case kSbDrmStatusQuotaExceededError:
      promise_reference->value().Reject(
          new DOMException(DOMException::kQuotaExceededErr, error_message));
      break;
    case kSbDrmStatusUnknownError:
      promise_reference->value().Reject(
          new DOMException(DOMException::kNone, error_message));
      break;
  }
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_EME_HELPERS_H_
