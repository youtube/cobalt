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

#ifndef COBALT_LOADER_IMAGE_THREADED_IMAGE_DECODER_PROXY_H_
#define COBALT_LOADER_IMAGE_THREADED_IMAGE_DECODER_PROXY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace image {

// This class handles the layer of indirection between images that web module
// thread wants to decode, and the ResourceLoader thread that does the actual
// work.
class ThreadedImageDecoderProxy : public Decoder {
 public:
  typedef base::Callback<void(const scoped_refptr<Image>&)> SuccessCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  ThreadedImageDecoderProxy(render_tree::ResourceProvider* resource_provider,
                            const SuccessCallback& success_callback,
                            const ErrorCallback& error_callback,
                            MessageLoop* load_message_loop_);

  ~ThreadedImageDecoderProxy();

  // From the Decoder interface.
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override;
  void DecodeChunk(const char* data, size_t size) override;
  void DecodeChunkPassed(scoped_ptr<std::string> data) override;
  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

 private:
  base::WeakPtrFactory<ThreadedImageDecoderProxy> weak_ptr_factory_;
  base::WeakPtr<ThreadedImageDecoderProxy> weak_this_;

  // The message loop that |image_decoder_| should run on.
  MessageLoop* load_message_loop_;

  // The message loop that the original callbacks passed into us should run
  // on.
  MessageLoop* result_message_loop_;

  // The actual image decoder.
  scoped_ptr<ImageDecoder> image_decoder_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_THREADED_IMAGE_DECODER_PROXY_H_
