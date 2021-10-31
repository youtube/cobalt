// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/memory/ptr_util.h"
#include "cobalt/loader/font/typeface_decoder.h"

namespace cobalt {
namespace loader {
namespace font {

TypefaceDecoder::TypefaceDecoder(
    render_tree::ResourceProvider* resource_provider,
    const TypefaceAvailableCallback& typeface_available_callback,
    const loader::Decoder::OnCompleteFunction& load_complete_callback)
    : resource_provider_(resource_provider),
      typeface_available_callback_(typeface_available_callback),
      load_complete_callback_(load_complete_callback),
      is_raw_data_too_large_(false),
      is_suspended_(!resource_provider_) {
  DCHECK(!typeface_available_callback_.is_null());
  DCHECK(!load_complete_callback.is_null());
}

void TypefaceDecoder::DecodeChunk(const char* data, size_t size) {
  if (is_suspended_) {
    DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
    return;
  }

  // If the data was already too large, then there's no need to process this
  // chunk. Just early out.
  if (is_raw_data_too_large_) {
    return;
  }

  if (!raw_data_) {
    raw_data_ = base::WrapUnique(
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
  if (is_suspended_) {
    DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
    return;
  }

  if (is_raw_data_too_large_) {
    load_complete_callback_.Run(
        std::string("Raw typeface data size too large"));
    return;
  }

  std::string error_string;
  scoped_refptr<render_tree::Typeface> decoded_typeface =
      resource_provider_->CreateTypefaceFromRawData(std::move(raw_data_),
                                                    &error_string);

  if (decoded_typeface) {
    typeface_available_callback_.Run(decoded_typeface);
    load_complete_callback_.Run(base::nullopt);
  } else {
    load_complete_callback_.Run(std::string(error_string));
  }
}

void TypefaceDecoder::ReleaseRawData() { raw_data_.reset(); }

bool TypefaceDecoder::Suspend() {
  DCHECK(!is_suspended_);
  DCHECK(resource_provider_);

  is_suspended_ = true;
  resource_provider_ = NULL;

  is_raw_data_too_large_ = false;
  ReleaseRawData();

  return true;
}

void TypefaceDecoder::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK(is_suspended_);
  DCHECK(!resource_provider_);
  DCHECK(resource_provider);

  is_suspended_ = false;
  resource_provider_ = resource_provider;
}

}  // namespace font
}  // namespace loader
}  // namespace cobalt
