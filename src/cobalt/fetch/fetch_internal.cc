// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/strings/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace fetch {

// static
bool FetchInternal::IsUrlValid(script::EnvironmentSettings* settings,
                               const std::string& url, bool allow_credentials) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  GURL gurl = dom_settings->base_url().Resolve(url);
  return gurl.is_valid() &&
         (allow_credentials || (!gurl.has_username() && !gurl.has_password()));
}

// static
script::Handle<script::Uint8Array> FetchInternal::EncodeToUTF8(
    script::EnvironmentSettings* settings, const std::string& text,
    script::ExceptionState* exception_state) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  // The conversion helper already translated the JSValue into a UTF-8 encoded
  // string. So just wrap the result in a Uint8Array.
  return script::Uint8Array::New(dom_settings->global_environment(),
                                 text.data(), text.size());
}

// static
std::string FetchInternal::DecodeFromUTF8(
    const script::Handle<script::Uint8Array>& data,
    script::ExceptionState* exception_state) {
  // The conversion helper expects the return value to be a UTF-8 encoded
  // string.
  base::StringPiece input(reinterpret_cast<const char*>(data->Data()),
                          data->Length());

  if (IsStringUTF8(input)) {
    // Input is already UTF-8. Just strip the byte order mark if it's present.
    const base::StringPiece kUtf8ByteOrderMarkStringPiece(
        base::kUtf8ByteOrderMark);
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

// static
script::Handle<script::ArrayBuffer> FetchInternal::BlobToArrayBuffer(
    script::EnvironmentSettings* settings,
    const scoped_refptr<dom::Blob>& blob) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  // Create a copy of the data so that the caller cannot modify the Blob.
  return script::ArrayBuffer::New(dom_settings->global_environment(),
                                  blob->data(), blob->size());
}

}  // namespace fetch
}  // namespace cobalt
