// Copyright 2015 Google Inc. All Rights Reserved.
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
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/decoder.h"

namespace cobalt {
namespace loader {

// TextDecoder converts chunks of data into std::string and concatenate the
// results.
class TextDecoder : public Decoder {
 public:
  explicit TextDecoder(
      base::Callback<void(const std::string&, const loader::Origin&)>
          done_callback)
      : done_callback_(done_callback), suspended_(false) {}
  ~TextDecoder() override {}

  // This function is used for binding callback for creating TextDecoder.
  static scoped_ptr<Decoder> Create(
      base::Callback<void(const std::string&, const loader::Origin&)>
          done_callback) {
    return scoped_ptr<Decoder>(new TextDecoder(done_callback));
  }

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (suspended_) {
      return;
    }
    text_.append(data, size);
  }

  void DecodeChunkPassed(scoped_ptr<std::string> data) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(data);
    if (suspended_) {
      return;
    }

    if (text_.empty()) {
      std::swap(*data, text_);
    } else {
      text_.append(*data);
    }
  }

  void Finish() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (suspended_) {
      return;
    }
    done_callback_.Run(text_, last_url_origin_);
  }
  bool Suspend() override {
    suspended_ = true;
    text_.clear();
    return true;
  }
  void Resume(render_tree::ResourceProvider* /*resource_provider*/) override {
    suspended_ = false;
  }
  void SetLastURLOrigin(const loader::Origin& last_url_origin) override {
    last_url_origin_ = last_url_origin;
  }

 private:
  base::ThreadChecker thread_checker_;
  std::string text_;
  base::Callback<void(const std::string&, const loader::Origin&)>
      done_callback_;
  bool suspended_;
  loader::Origin last_url_origin_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_TEXT_DECODER_H_
