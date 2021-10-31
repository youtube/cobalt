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

#ifndef COBALT_ENCODING_TEXT_DECODER_H_
#define COBALT_ENCODING_TEXT_DECODER_H_

#include <string>

#include "cobalt/dom/buffer_source.h"
#include "cobalt/encoding/text_decode_options.h"
#include "cobalt/encoding/text_decoder_options.h"
#include "cobalt/script/wrappable.h"

typedef struct UConverter UConverter;

namespace cobalt {
namespace encoding {

class TextDecoder : public script::Wrappable {
 public:
  explicit TextDecoder(script::ExceptionState*);
  explicit TextDecoder(const std::string&, script::ExceptionState*);
  explicit TextDecoder(const TextDecoderOptions&, script::ExceptionState*);
  TextDecoder(const std::string&, const TextDecoderOptions&,
              script::ExceptionState*);

  ~TextDecoder() override;

  std::string encoding() const { return encoding_; }
  bool fatal() const { return error_mode_ == "fatal"; }
  bool ignore_bom() const { return ignore_bom_; }

  std::string Decode(script::ExceptionState*);
  std::string Decode(const dom::BufferSource&, script::ExceptionState*);
  std::string Decode(const TextDecodeOptions&, script::ExceptionState*);
  std::string Decode(const dom::BufferSource&, const TextDecodeOptions&,
                     script::ExceptionState*);

  DEFINE_WRAPPABLE_TYPE(TextDecoder);

 private:
  typedef int32_t UChar32;
  // Web API standard.
  std::string encoding_;
  std::string error_mode_;
  bool bom_seen_;
  bool do_not_flush_;
  bool ignore_bom_;

  UConverter* converter_;

  static const std::size_t kConversionBufferSize;
  static const TextDecoderOptions kDefaultDecoderOptions;
  static const char kDefaultEncoding[];
  static const UChar32 kReplacementCharacter;
  static const char kReplacementEncoding[];


  // Common code.
  void Cleanup();
  void Setup(std::string, script::ExceptionState*, const TextDecoderOptions&);

  // Function helpers.
  bool RemoveBOM(const char*&, int&, script::ExceptionState*);
  void Decode(const char*, int32_t, const TextDecodeOptions&,
              script::ExceptionState*, std::string*);
  void DecodeImpl(const char* bytes, int length, bool flush, bool stop_on_error,
                  bool& saw_error, std::string*);

  DISALLOW_COPY_AND_ASSIGN(TextDecoder);
};

}  // namespace encoding
}  // namespace cobalt
#endif  // COBALT_ENCODING_TEXT_DECODER_H_
