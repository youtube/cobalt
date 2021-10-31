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

#ifndef COBALT_ENCODING_TEXT_ENCODER_H_
#define COBALT_ENCODING_TEXT_ENCODER_H_

#include <string>

#include "cobalt/encoding/text_encoder_encode_into_result.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/typed_arrays.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace encoding {

class TextEncoder : public script::Wrappable {
 public:
  TextEncoder();
  ~TextEncoder();

  std::string encoding() { return KEncoding; }

  script::Handle<script::Uint8Array> Encode(script::EnvironmentSettings *);
  script::Handle<script::Uint8Array> Encode(script::EnvironmentSettings *,
                                            const std::string &);
  TextEncoderEncodeIntoResult EncodeInto(
      script::EnvironmentSettings *, const std::string &,
      const script::Handle<script::Uint8Array> &);

  DEFINE_WRAPPABLE_TYPE(TextEncoder);

 private:
  static const char KEncoding[];

  DISALLOW_COPY_AND_ASSIGN(TextEncoder);
};

}  // namespace encoding
}  // namespace cobalt
#endif  // COBALT_ENCODING_TEXT_ENCODER_H_
