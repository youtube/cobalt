/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_TEXT_DECODER_H_
#define COBALT_LOADER_TEXT_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/decoder.h"

namespace cobalt {
namespace loader {

// TextDecoder converts chunks of data into std::string and concatenate the
// results.
class TextDecoder : public Decoder {
 public:
  explicit TextDecoder(base::Callback<void(const std::string&)> done_callback)
      : done_callback_(done_callback) {}
  ~TextDecoder() OVERRIDE {}

  // This function is used for binding callback for creating TextDecoder.
  static scoped_ptr<Decoder> Create(
      base::Callback<void(const std::string&)> done_callback) {
    return scoped_ptr<Decoder>(new TextDecoder(done_callback));
  }

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    text_.append(data, size);
  }
  void Finish() OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    done_callback_.Run(text_);
  }

 private:
  base::ThreadChecker thread_checker_;
  std::string text_;
  base::Callback<void(const std::string&)> done_callback_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_TEXT_DECODER_H_
