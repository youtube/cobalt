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

#include "third_party/icu/source/common/unicode/uchriter.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace cobalt {
namespace encoding {

// static
const std::size_t TextDecoder::kConversionBufferSize = 16384;  // 2^14
const TextDecoderOptions TextDecoder::kDefaultDecoderOptions;
const char TextDecoder::kDefaultEncoding[] = "utf-8";
const UChar32 TextDecoder::kReplacementCharacter = 0xFFFD;
const char TextDecoder::kReplacementEncoding[] = "REPLACEMENT";


namespace {
std::string ToLower(std::string str) {
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
                        script::ExceptionState* exception_state,
                        const TextDecoderOptions& options) {
  UErrorCode error_code = U_ZERO_ERROR;
  converter_ = ucnv_open(label.c_str(), &error_code);
  if (U_FAILURE(error_code)) {
    LOG(ERROR) << "Unable to open converter. " << u_errorName(error_code);
    exception_state->SetSimpleException(script::kRangeError,
                                        "label %s is invalid", label.c_str());
    return Cleanup();
  }

  error_code = U_ZERO_ERROR;
  encoding_ = ucnv_getName(converter_, &error_code);
  if (U_FAILURE(error_code) || encoding_ == kReplacementEncoding) {
    LOG(ERROR) << "Unable to get encoding name. " << u_errorName(error_code);
    exception_state->SetSimpleException(
        script::kRangeError, "encoding %s is invalid", encoding_.c_str());
    return Cleanup();
  }

  // Internal default setup;
  encoding_ = ToLower(encoding_);
  error_mode_ = options.fatal() ? "fatal" : "replacement";
  ignore_bom_ = options.ignore_bom();
  bom_seen_ = false;
  do_not_flush_ = false;
}

TextDecoder::TextDecoder(script::ExceptionState* exception_state)
    : converter_(nullptr) {
  Setup(kDefaultEncoding, exception_state, kDefaultDecoderOptions);
}

TextDecoder::TextDecoder(const std::string& label,
                         script::ExceptionState* exception_state)
    : converter_(nullptr) {
  Setup(label, exception_state, kDefaultDecoderOptions);
}

TextDecoder::TextDecoder(const TextDecoderOptions& options,
                         script::ExceptionState* exception_state) {
  Setup(kDefaultEncoding, exception_state, options);
}

TextDecoder::TextDecoder(const std::string& label,
                         const TextDecoderOptions& options,
                         script::ExceptionState* exception_state) {
  Setup(label, exception_state, options);
}

TextDecoder::~TextDecoder() { Cleanup(); }

std::string TextDecoder::Decode(script::ExceptionState* exception_state) {
  const TextDecodeOptions default_options;
  return Decode(default_options, exception_state);
}

std::string TextDecoder::Decode(const dom::BufferSource& input,
                                script::ExceptionState* exception_state) {
  const TextDecodeOptions default_options;
  return Decode(input, default_options, exception_state);
}

std::string TextDecoder::Decode(const TextDecodeOptions& options,
                                script::ExceptionState* exception_state) {
  std::string result;
  Decode(nullptr, 0, options, exception_state, &result);
  return result;
}

std::string TextDecoder::Decode(const dom::BufferSource& input,
                                const TextDecodeOptions& options,
                                script::ExceptionState* exception_state) {
  int32_t size;
  const uint8* buffer;
  std::string result;
  dom::GetBufferAndSize(input, &buffer, &size);
  Decode(reinterpret_cast<const char*>(buffer), size, options, exception_state,
         &result);
  return result;
}

// General Decode method, all public methods end here.
void TextDecoder::Decode(const char* start, int32_t length,
                         const TextDecodeOptions& options,
                         script::ExceptionState* exception_state,
                         std::string* output) {
  if (!converter_) {
    return;
  }
  if (!do_not_flush_) {
    bom_seen_ = false;
  }
  if (!ignore_bom_ && !bom_seen_) {
    bom_seen_ = true;
    if (!RemoveBOM(start, length, exception_state)) {
      return;
    }
  }
  do_not_flush_ = options.stream();
  bool saw_error = false;
  DecodeImpl(start, length, !do_not_flush_, error_mode_ == "fatal", saw_error,
             output);

  if (error_mode_ == "fatal" && saw_error) {
    if (!do_not_flush_) {
      // If flushing, the error should not persist.
      ucnv_reset(converter_);
    }
    exception_state->SetSimpleException(script::kTypeError,
                                        "Error processing the encoded data");
  }
  return;
}

bool TextDecoder::RemoveBOM(const char*& buffer, int& size,
                            script::ExceptionState* exception_state) {
  UErrorCode error_code = U_ZERO_ERROR;
  int32_t signature_length = 0;
  const char* detected_encoding =
      ucnv_detectUnicodeSignature(buffer, size, &signature_length, &error_code);
  if (U_FAILURE(error_code)) {
    LOG(ERROR) << "Error detecting signature " << u_errorName(error_code);
    exception_state->SetSimpleException(script::kRangeError,
                                        "Error detecting signature");
    return false;
  }

  // Only remove BOM when both encodings match.
  if (detected_encoding && (ToLower(detected_encoding) == encoding_)) {
    buffer += signature_length;
    size -= signature_length;
    LOG(INFO) << "Buffer adjusted (+" << signature_length << ") bytes.";
  }
  return true;
}

class ErrorCallbackSetter final {
 public:
  ErrorCallbackSetter(UConverter* converter, bool stop_on_error)
      : converter_(converter), stop_on_error_(stop_on_error) {
    if (stop_on_error_) {
      UErrorCode error = U_ZERO_ERROR;
      ucnv_setToUCallBack(converter_, UCNV_TO_U_CALLBACK_STOP, nullptr,
                          &saved_action_, &saved_context_, &error);
      DCHECK_EQ(error, U_ZERO_ERROR);
    }
  }
  ~ErrorCallbackSetter() {
    if (stop_on_error_) {
      UErrorCode error = U_ZERO_ERROR;
      const void* old_context;
      UConverterToUCallback old_action;
      ucnv_setToUCallBack(converter_, saved_action_, saved_context_,
                          &old_action, &old_context, &error);
      DCHECK_EQ(old_action, UCNV_TO_U_CALLBACK_STOP);
      DCHECK(!old_context);
      DCHECK_EQ(error, U_ZERO_ERROR);
    }
  }

 private:
  UConverter* converter_;
  bool stop_on_error_;
  const void* saved_context_;
  UConverterToUCallback saved_action_;
};

namespace {
int DecodeTo(UConverter* converter_, UChar* target, UChar* target_limit,
             const char*& source, const char* source_limit, int32_t* offsets,
             bool flush, UErrorCode& error) {
  UChar* target_start = target;
  error = U_ZERO_ERROR;
  ucnv_toUnicode(converter_, &target, target_limit, &source, source_limit,
                 offsets, flush, &error);
  return static_cast<int>(target - target_start);
}
}  // namespace

void TextDecoder::DecodeImpl(const char* bytes, int length, bool flush,
                             bool stop_on_error, bool& saw_error,
                             std::string* output) {
  if (!converter_) {
    LOG(WARNING) << "no converter available.";
    return;
  }

  ErrorCallbackSetter callback_setter(converter_, stop_on_error);

  icu::UnicodeString result;

  UChar buffer[kConversionBufferSize];
  UChar* buffer_limit = buffer + kConversionBufferSize;
  const char* source = reinterpret_cast<const char*>(bytes);
  const char* source_limit = source + length;
  int32_t* offsets = nullptr;
  UErrorCode error = U_ZERO_ERROR;

  do {
    int uchars_decoded = DecodeTo(converter_, buffer, buffer_limit, source,
                                  source_limit, offsets, flush, error);
    result.append(buffer, uchars_decoded);
  } while (error == U_BUFFER_OVERFLOW_ERROR);

  // Flush the converter so it can be reused.
  if (U_FAILURE(error)) {
    do {
      DecodeTo(converter_, buffer, buffer_limit, source, source_limit, offsets,
               true, error);
    } while (source < source_limit);
    saw_error = true;
  }
  result.toUTF8String(*output);
  return;
}

}  // namespace encoding
}  // namespace cobalt
