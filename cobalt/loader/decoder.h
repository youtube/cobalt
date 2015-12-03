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

#ifndef LOADER_DECODER_H_
#define LOADER_DECODER_H_

#include "net/http/http_response_headers.h"

namespace cobalt {
namespace loader {

class Fetcher;

class Decoder {
 public:
  Decoder() {}
  virtual ~Decoder() {}

  // A loader may want to signal the beginning of a decode and can send
  // the HTTP headers, if this was a network request.
  virtual void OnResponseStarted(
      Fetcher* /*fetcher*/,
      const scoped_refptr<net::HttpResponseHeaders>& /*headers*/) {}

  // This is the interface that chunks of bytes can be sent in to be decoded.
  virtual void DecodeChunk(const char* data, size_t size) = 0;

  // This is called when all data are sent in and decoding should be finalized.
  virtual void Finish() = 0;
};

}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_DECODER_H_
