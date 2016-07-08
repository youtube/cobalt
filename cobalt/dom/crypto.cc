/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/crypto.h"

#include "cobalt/dom/dom_exception.h"
#include "crypto/random.h"

namespace cobalt {
namespace dom {

// https://www.w3.org/TR/WebCryptoAPI/#dfn-Crypto-method-getRandomValues
// static
scoped_refptr<ArrayBufferView> Crypto::GetRandomValues(
    const scoped_refptr<ArrayBufferView>& array,
    script::ExceptionState* exception_state) {
  // If the length of the array in bytes exceeds the following constant,
  // getRandomValues() should raise QuotaExceedErr instead of filling the array
  // with random values.
  uint32 kMaxArrayLengthInBytes = 65536;
  if (!array) {
    // TODO: Also throw exception if element type of the array is
    // not one of the integer types.
    DOMException::Raise(DOMException::kTypeMismatchErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }
  if (array->byte_length() > kMaxArrayLengthInBytes) {
    DOMException::Raise(DOMException::kQuotaExceededErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }
  ::crypto::RandBytes(array->base_address(), array->byte_length());
  return array;
}

}  // namespace dom
}  // namespace cobalt
