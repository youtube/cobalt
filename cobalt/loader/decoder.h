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

#ifndef COBALT_LOADER_DECODER_H_
#define COBALT_LOADER_DECODER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/loader_types.h"
#include "cobalt/render_tree/resource_provider.h"
#include "googleurl/src/gurl.h"
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
  virtual LoadResponseType OnResponseStarted(
      Fetcher* /*fetcher*/,
      const scoped_refptr<net::HttpResponseHeaders>& /*headers*/)
      WARN_UNUSED_RESULT {
    return kLoadResponseContinue;
  }

  // This is the interface that chunks of bytes can be sent in to be decoded.
  virtual void DecodeChunk(const char* data, size_t size) = 0;

  // A decoder can choose to implement |DecodeChunkPassed| in order to take
  // ownership of the chunk data.  Taking ownership over the chunk data can
  // allow data copies to be avoided, such as when passing the data off to be
  // decoded asynchronously.  Not all fetchers are guaranteed to support this
  // though, in which case they simply forward the scoped pointer data to
  // DecodeChunk.
  virtual void DecodeChunkPassed(scoped_ptr<std::string> data) {
    DecodeChunk(data->data(), data->size());
  }

  // This is called when all data are sent in and decoding should be finalized.
  virtual void Finish() = 0;

  // Suspends the decode of this resource, resetting internal state. Returns
  // whether the decoder was reset correctly. If not, the load will have to be
  // aborted.
  virtual bool Suspend() = 0;

  // Resumes the decode of this resource, starting over from the zero state.
  virtual void Resume(render_tree::ResourceProvider* resource_provider) = 0;

  // Provides textdecoder with last url to prevent security leak if resource is
  // cross-origin.
  virtual void SetLastURLOrigin(const Origin&) {}
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_DECODER_H_
