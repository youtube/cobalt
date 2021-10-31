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

#ifndef COBALT_LOADER_IMAGE_THREADED_IMAGE_DECODER_PROXY_H_
#define COBALT_LOADER_IMAGE_THREADED_IMAGE_DECODER_PROXY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
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
  typedef base::Callback<void(const scoped_refptr<Image>&)>
      ImageAvailableCallback;

  ~ThreadedImageDecoderProxy();

  // This function is used for binding a callback to create a
  // ThreadedImageDecoderProxy.
  static std::unique_ptr<Decoder> Create(
      render_tree::ResourceProvider* resource_provider,
      const base::DebuggerHooks* debugger_hooks,
      const ImageAvailableCallback& image_available_callback,
      base::MessageLoop* load_message_loop_,
      const loader::Decoder::OnCompleteFunction& load_complete_callback) {
    return std::unique_ptr<Decoder>(new ThreadedImageDecoderProxy(
        resource_provider, debugger_hooks, image_available_callback,
        load_message_loop_, load_complete_callback));
  }

  // From the Decoder interface.
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override;
  void DecodeChunk(const char* data, size_t size) override;
  void DecodeChunkPassed(std::unique_ptr<std::string> data) override;
  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

 private:
  ThreadedImageDecoderProxy(
      render_tree::ResourceProvider* resource_provider,
      const base::DebuggerHooks* debugger_hooks,
      const ImageAvailableCallback& image_available_callback,
      base::MessageLoop* load_message_loop_,
      const loader::Decoder::OnCompleteFunction& load_complete_callback);

  base::WeakPtrFactory<ThreadedImageDecoderProxy> weak_ptr_factory_;
  base::WeakPtr<ThreadedImageDecoderProxy> weak_this_;

  // The message loop that |image_decoder_| should run on.
  base::MessageLoop* load_message_loop_;

  // The message loop that the original callbacks passed into us should run
  // on.
  base::MessageLoop* result_message_loop_;

  // The actual image decoder.
  std::unique_ptr<ImageDecoder> image_decoder_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_THREADED_IMAGE_DECODER_PROXY_H_
