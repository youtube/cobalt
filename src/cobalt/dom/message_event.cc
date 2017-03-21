/* Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/message_event.h"

#include <string>

#include "base/basictypes.h"
#include "starboard/string.h"

namespace {
const char* const kResponseTypes[] = {"text", "blob", "arraybuffer"};

COMPILE_ASSERT(arraysize(kResponseTypes) ==
                   cobalt::dom::MessageEvent::kResponseTypeCodeMax,
               enum_and_array_size_mismatch);

}  // namespace

namespace cobalt {
namespace dom {

std::string MessageEvent::GetResponseTypeAsString(
    const MessageEvent::ResponseTypeCode code) {
  DCHECK_LT(code, kResponseTypeCodeMax);
  if ((code >= kText) && (code < kResponseTypeCodeMax)) {
    return kResponseTypes[code];
  }
  return std::string();
}

MessageEvent::ResponseTypeCode MessageEvent::GetResponseTypeCode(
    base::StringPiece to_match) {
  for (std::size_t i = 0; i != arraysize(kResponseTypes); ++i) {
    if (SbStringCompare(kResponseTypes[i], to_match.data(), to_match.size()) ==
        0) {
      return MessageEvent::ResponseTypeCode(i);
    }
  }
  return kResponseTypeCodeMax;
}

MessageEvent::ResponseType MessageEvent::data() const {
  scoped_refptr<dom::ArrayBuffer> response_buffer;

  const char* data_pointer = NULL;
  int data_length = 0;
  if (data_) {
    data_pointer = data_->data();
    data_length = data_->size();
  }

  if (response_type_ != kText) {
    response_buffer = new dom::ArrayBuffer(
        settings_, reinterpret_cast<const uint8*>(data_pointer), data_length);
  }

  switch (response_type_) {
    case kText: {
      // TODO: Find a way to remove two copies of data here.
      std::string string_response(data_pointer, data_length);
      return ResponseType(string_response);
    }
    case kBlob: {
      scoped_refptr<dom::Blob> blob = new dom::Blob(settings_, response_buffer);
      return ResponseType(blob);
    }
    case kArrayBuffer: {
      return ResponseType(response_buffer);
    }
    case kResponseTypeCodeMax:
      NOTREACHED() << "Invalid response type.";
      return ResponseType();
  }

  return ResponseType();
}

}  // namespace dom
}  // namespace cobalt
