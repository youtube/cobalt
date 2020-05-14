// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_TEXT_DECODER_H_
#define COBALT_LOADER_TEXT_DECODER_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/decoder.h"

namespace cobalt {
namespace loader {

// TextDecoder converts chunks of data into std::string and concatenate the
// results.
class TextDecoder : public Decoder {
 public:
  typedef base::Callback<void(const loader::Origin&,
                              std::unique_ptr<std::string>)>
      TextAvailableCallback;

  ~TextDecoder() override {}

  // This function is used for binding callback for creating TextDecoder.
  static std::unique_ptr<Decoder> Create(
      const TextAvailableCallback& text_available_callback,
      const loader::Decoder::OnCompleteFunction& load_complete_callback =
          loader::Decoder::OnCompleteFunction()) {
    return std::unique_ptr<Decoder>(
        new TextDecoder(text_available_callback, load_complete_callback));
  }

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (suspended_) {
      return;
    }
    if (!text_) {
      text_.reset(new std::string);
    }
    text_->append(data, size);
  }

  void DecodeChunkPassed(std::unique_ptr<std::string> data) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(data);
    if (suspended_) {
      return;
    }

    if (!text_) {
      text_ = std::move(data);
    } else {
      text_->append(*data);
    }
  }

  void Finish() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (suspended_) return;

    if (!text_) text_.reset(new std::string);

    text_available_callback_.Run(last_url_origin_, std::move(text_));
    if (!load_complete_callback_.is_null()) {
      load_complete_callback_.Run(base::nullopt);
    }
  }

  bool Suspend() override {
    suspended_ = true;
    text_.reset();
    return true;
  }

  void Resume(render_tree::ResourceProvider* resource_provider) override {
    suspended_ = false;
  }

  void SetLastURLOrigin(const loader::Origin& last_url_origin) override {
    last_url_origin_ = last_url_origin;
  }

 private:
  explicit TextDecoder(
      const TextAvailableCallback& text_available_callback,
      const loader::Decoder::OnCompleteFunction& load_complete_callback)
      : text_available_callback_(text_available_callback),
        load_complete_callback_(load_complete_callback),
        suspended_(false) {}

  THREAD_CHECKER(thread_checker_);
  TextAvailableCallback text_available_callback_;
  loader::Decoder::OnCompleteFunction load_complete_callback_;
  loader::Origin last_url_origin_;
  std::unique_ptr<std::string> text_;
  bool suspended_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_TEXT_DECODER_H_
