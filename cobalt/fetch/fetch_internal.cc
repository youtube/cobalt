// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/fetch/fetch_internal.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace fetch {

// static
bool FetchInternal::IsUrlValid(const std::string& url) {
  GURL gurl(url);
  return gurl.is_valid() || gurl.is_empty();
}

// static
scoped_refptr<dom::Uint8Array> FetchInternal::EncodeToUTF8(
    script::EnvironmentSettings* settings, const std::string& text,
    script::ExceptionState* exception_state) {
  // The conversion helper already translated the JSValue into a UTF-8 encoded
  // string. So just wrap the result in a Uint8Array.
  return new dom::Uint8Array(settings,
      reinterpret_cast<const uint8*>(text.c_str()),
      static_cast<uint32>(text.size()),
      exception_state);
}

// static
std::string FetchInternal::DecodeFromUTF8(
    const scoped_refptr<dom::Uint8Array>& data,
    script::ExceptionState* exception_state) {
  // The conversion helper expects the return value to be a UTF-8 encoded
  // string.
  base::StringPiece input(reinterpret_cast<const char*>(data->data()),
                          data->length());

  if (IsStringUTF8(input)) {
    // Input is already UTF-8. Just strip the byte order mark if it's present.
    const base::StringPiece kUtf8ByteOrderMarkStringPiece(kUtf8ByteOrderMark);
    if (input.starts_with(kUtf8ByteOrderMarkStringPiece)) {
      input = input.substr(kUtf8ByteOrderMarkStringPiece.length());
    }
    return input.as_string();
  }

  // Only UTF-8 is supported.
  exception_state->SetSimpleException(script::kTypeError,
                                      "Not a valid UTF-8 string");
  return std::string();
}

}  // namespace fetch
}  // namespace cobalt
