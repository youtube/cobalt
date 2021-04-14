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

#include "cobalt/encoding/text_encoder.h"

#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"

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
  return script::Uint8Array::New(dom_settings->global_environment(),
                                 input.data(), input.size());
}

TextEncoderEncodeIntoResult TextEncoder::EncodeInto(
    script::EnvironmentSettings* settings, const std::string& input,
    const script::Handle<script::Uint8Array>& destination) {
  // TODO: Figure out how to append bytes to an existing buffer.
  NOTIMPLEMENTED();
  return TextEncoderEncodeIntoResult();
}


}  // namespace encoding
}  // namespace cobalt
