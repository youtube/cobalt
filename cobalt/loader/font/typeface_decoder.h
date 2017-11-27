// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_FONT_TYPEFACE_DECODER_H_
#define COBALT_LOADER_FONT_TYPEFACE_DECODER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/typeface.h"

namespace cobalt {
namespace loader {
namespace font {

// |TypefaceDecoder| handles collecting typeface data, which it provides to its
// |ResourceProvider|, which is responsible for interpreting the data and
// generating the typeface.
class TypefaceDecoder : public Decoder {
 public:
  typedef base::Callback<void(const scoped_refptr<render_tree::Typeface>&)>
      SuccessCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  TypefaceDecoder(render_tree::ResourceProvider* resource_provider,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback);

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) override;
  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

 private:
  void ReleaseRawData();

  render_tree::ResourceProvider* resource_provider_;
  const SuccessCallback success_callback_;
  const ErrorCallback error_callback_;

  scoped_ptr<render_tree::ResourceProvider::RawTypefaceDataVector> raw_data_;
  bool is_raw_data_too_large_;

  bool is_suspended_;
};

}  // namespace font
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FONT_TYPEFACE_DECODER_H_
