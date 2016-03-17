/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/font/typeface_decoder.h"

namespace cobalt {
namespace loader {
namespace font {

TypefaceDecoder::TypefaceDecoder(
    render_tree::ResourceProvider* resource_provider,
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      error_callback_(error_callback),
      is_raw_data_too_large_(false) {
  DCHECK(resource_provider_);
  DCHECK(!success_callback_.is_null());
  DCHECK(!error_callback_.is_null());
}

void TypefaceDecoder::DecodeChunk(const char* data, size_t size) {
  // If the data was already too large, then there's no need to process this
  // chunk. Just early out.
  if (is_raw_data_too_large_) {
    return;
  }

  if (!raw_data_) {
    raw_data_ = make_scoped_ptr(
        new render_tree::ResourceProvider::RawTypefaceDataVector());
  }

  size_t start_size = raw_data_->size();

  // Verify that adding this chunk to the raw data won't cause it to become
  // larger than the max allowed size. If it will, then set the flag indicating
  // that the data was too large and release the data.
  if (start_size + size > render_tree::ResourceProvider::kMaxTypefaceDataSize) {
    is_raw_data_too_large_ = true;
    ReleaseRawData();
    return;
  }

  raw_data_->resize(start_size + size);
  memcpy(&((*raw_data_)[start_size]), data, size);
}

void TypefaceDecoder::Finish() {
  if (is_raw_data_too_large_) {
    error_callback_.Run("Raw typeface data size too large");
    return;
  }

  std::string error_string;
  scoped_refptr<render_tree::Typeface> decoded_typeface =
      resource_provider_->CreateTypefaceFromRawData(raw_data_.Pass(),
                                                    &error_string);

  if (decoded_typeface) {
    success_callback_.Run(decoded_typeface);
  } else {
    error_callback_.Run(error_string);
  }
}

void TypefaceDecoder::ReleaseRawData() { raw_data_.reset(); }

}  // namespace font
}  // namespace loader
}  // namespace cobalt
