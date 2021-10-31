// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <string>

#include "cobalt/encoding/text_encoder.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "third_party/icu/source/common/unicode/utext.h"
#include "third_party/icu/source/common/unicode/utf8.h"

namespace cobalt {
namespace encoding {
// static
const char TextEncoder::KEncoding[] = "utf-8";


TextEncoder::TextEncoder() {}
TextEncoder::~TextEncoder() {}

script::Handle<script::Uint8Array> TextEncoder::Encode(
    script::EnvironmentSettings* settings) {
  return Encode(settings, std::string());
}

script::Handle<script::Uint8Array> TextEncoder::Encode(
    script::EnvironmentSettings* settings, const std::string& input) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);

  // The conversion helper already translated the JSValue into a valid UTF-8
  // encoded string.
  return script::Uint8Array::New(dom_settings->global_environment(),
                                 input.data(), input.size());
}

TextEncoderEncodeIntoResult TextEncoder::EncodeInto(
    script::EnvironmentSettings* settings, const std::string& input,
    const script::Handle<script::Uint8Array>& destination) {
  int32_t capacity = static_cast<int>(destination->ByteLength());
  uint8* buffer = static_cast<uint8*>(destination->Data());
  // The conversion helper already translated the JSValue into a valid UTF-8
  // encoded string. However, this extra process is in place to avoid:
  //  1. The caller allocated less space than needed.
  //  2. The caller allocated more space than needed.
  UErrorCode error_code = U_ZERO_ERROR;
  int32_t read = 0, written = 0;
  UText* unicode_text = nullptr;
  UBool err = 0;
  unicode_text =
      utext_openUTF8(unicode_text, input.c_str(), input.size(), &error_code);
  if (U_SUCCESS(error_code)) {
    for (UChar32 code_point = utext_next32From(unicode_text, 0);
         code_point >= 0 && !err && written < capacity;
         code_point = utext_next32(unicode_text)) {
      U8_APPEND(buffer, written, capacity, code_point, err);
      if (!err) {
        ++read;
      }
    }
    utext_close(unicode_text);
  } else {
    LOG(ERROR) << "Invalid string: " << u_errorName(error_code);
  }
  TextEncoderEncodeIntoResult result;
  result.set_read(read);
  result.set_written(written);
  return result;
}


}  // namespace encoding
}  // namespace cobalt
