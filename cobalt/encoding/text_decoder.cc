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
#include <cctype>

#include "cobalt/encoding/text_decoder.h"

#include "third_party/icu/source/common/unicode/ucnv.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace cobalt {
namespace encoding {

// static
const char TextDecoder::kDefaultEncoding[] = "utf-8";
const char TextDecoder::kReplacementEncoding[] = "REPLACEMENT";

namespace {
std::string to_lower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}
}  // namespace

void TextDecoder::Cleanup() {
  encoding_.clear();
  if (converter_) {
    ucnv_close(converter_);
  }
  converter_ = nullptr;
}

void TextDecoder::Setup(std::string label,
                        script::ExceptionState* exception_state) {
  UErrorCode error_code = U_ZERO_ERROR;
  converter_ = ucnv_open(label.c_str(), &error_code);
  if (U_FAILURE(error_code)) {
    LOG(ERROR) << "Unable to open icu converter. " << u_errorName(error_code);
    exception_state->SetSimpleException(script::kRangeError,
                                        "label %s is invalid", label.c_str());
    return Cleanup();
  }

  error_code = U_ZERO_ERROR;
  encoding_ = ucnv_getName(converter_, &error_code);
  if (U_FAILURE(error_code) || encoding_ == kReplacementEncoding) {
    LOG(ERROR) << "Unable to get encoding name. " << u_errorName(error_code);
    exception_state->SetSimpleException(
        script::kRangeError, "label %s is invalid", encoding_.c_str());
    return Cleanup();
  }

  // Encoding’s name, lowercased.
  encoding_ = to_lower(encoding_);
}

TextDecoder::TextDecoder(script::ExceptionState* exception_state)
    : converter_(nullptr) {
  Setup(kDefaultEncoding, exception_state);
}

TextDecoder::TextDecoder(const std::string& label,
                         script::ExceptionState* exception_state)
    : converter_(nullptr) {
  Setup(label, exception_state);
}

TextDecoder::TextDecoder(const TextDecoderOptions& options,
                         script::ExceptionState* exception_state)
    : fatal_(options.fatal()), ignore_bom_(options.ignore_bom()) {
  Setup(kDefaultEncoding, exception_state);
}

TextDecoder::TextDecoder(const std::string& label,
                         const TextDecoderOptions& options,
                         script::ExceptionState* exception_state)
    : fatal_(options.fatal()), ignore_bom_(options.ignore_bom()) {
  Setup(label, exception_state);
}

TextDecoder::~TextDecoder() { Cleanup(); }

std::string TextDecoder::Decode(script::ExceptionState* exception_state) {
  // TODO: Figure out how to remove this use case or implement its support.
  NOTIMPLEMENTED();
  return "";
}
std::string TextDecoder::Decode(const dom::BufferSource& input,
                                script::ExceptionState* exception_state) {
  // Use the default options here.
  const TextDecodeOptions default_options;
  return Decode(input, default_options, exception_state);
}
std::string TextDecoder::Decode(const TextDecodeOptions& options,
                                script::ExceptionState* exception_state) {
  // TODO: Figure out how to remove this use case or implement its support.
  // Note: This is a valid case, for example when using the stream option.
  NOTIMPLEMENTED();
  return "";
}
std::string TextDecoder::Decode(const dom::BufferSource& input,
                                const TextDecodeOptions& options,
                                script::ExceptionState* exception_state) {
  int32_t size;
  const uint8* buffer;
  std::string result;
  UErrorCode error_code = U_ZERO_ERROR;
  dom::GetBufferAndSize(input, &buffer, &size);

  if (converter_) {
    icu::UnicodeString unicode_input(reinterpret_cast<const char*>(buffer),
                                     size, converter_, error_code);
    if (U_FAILURE(error_code)) {
      LOG(ERROR) << "Error decoding " << u_errorName(error_code);
      exception_state->SetSimpleException(script::kRangeError,
                                          "Error processing the data");
      return result;
    }
    unicode_input.toUTF8String(result);
  } else {
    LOG(ERROR) << "No converter available";
    exception_state->SetSimpleException(script::kRangeError,
                                        "No converter available");
  }
  return result;
}

}  // namespace encoding
}  // namespace cobalt
