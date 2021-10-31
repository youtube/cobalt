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

#include "cobalt/loader/image/threaded_image_decoder_proxy.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "cobalt/loader/image/image_decoder.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

// Helper function that is run on the WebModule thread to run a Callback if
// and only if |threaded_image_decoder_proxy| is still alive.
template <typename Callback, typename Arg>
void MaybeRun(
    base::WeakPtr<ThreadedImageDecoderProxy> threaded_image_decoder_proxy,
    const Callback& callback, const Arg& arg) {
  if (threaded_image_decoder_proxy) {
    callback.Run(arg);
  }
}

// Helper function to post a Callback back to the WebModule thread, that
// checks whether the ThreadedImageDecoderProxy it came from is alive or not
// before it runs.
template <typename Callback, typename Arg>
void PostToMessageLoopChecked(
    base::WeakPtr<ThreadedImageDecoderProxy> threaded_image_decoder_proxy,
    const Callback& callback, base::MessageLoop* message_loop, const Arg& arg) {
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MaybeRun<Callback, Arg>,
                            threaded_image_decoder_proxy, callback, arg));
}

}  // namespace

ThreadedImageDecoderProxy::ThreadedImageDecoderProxy(
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks* debugger_hooks,
    const ImageAvailableCallback& image_available_callback,
    base::MessageLoop* load_message_loop,
    const loader::Decoder::OnCompleteFunction& load_complete_callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      load_message_loop_(load_message_loop),
      result_message_loop_(base::MessageLoop::current()),
      image_decoder_(new ImageDecoder(
          resource_provider, *debugger_hooks,
          base::Bind(&PostToMessageLoopChecked<ImageAvailableCallback,
                                               scoped_refptr<Image>>,
                     weak_this_, image_available_callback,
                     result_message_loop_),
          base::Bind(
              &PostToMessageLoopChecked<loader::Decoder::OnCompleteFunction,
                                        base::Optional<std::string>>,
              weak_this_, load_complete_callback, result_message_loop_))) {
  DCHECK(load_message_loop_);
  DCHECK(result_message_loop_);
  DCHECK(image_decoder_);
}

// We're dying, but |image_decoder_| might still be doing work on the load
// thread.  Because of this, we transfer ownership of it into the load message
// loop, where it will be deleted after any pending tasks involving it are
// done.
ThreadedImageDecoderProxy::~ThreadedImageDecoderProxy() {
  // Notify the ImageDecoder that there's a pending deletion to ensure that no
  // additional work is done decoding the image.
  image_decoder_->SetDeletionPending();
  load_message_loop_->task_runner()->DeleteSoon(FROM_HERE,
                                                image_decoder_.release());
}

LoadResponseType ThreadedImageDecoderProxy::OnResponseStarted(
    Fetcher* fetcher, const scoped_refptr<net::HttpResponseHeaders>& headers) {
  return image_decoder_->OnResponseStarted(fetcher, headers);
}

void ThreadedImageDecoderProxy::DecodeChunk(const char* data, size_t size) {
  std::unique_ptr<std::string> scoped_data(new std::string(data, size));
  load_message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&Decoder::DecodeChunkPassed,
                            base::Unretained(image_decoder_.get()),
                            base::Passed(&scoped_data)));
}

void ThreadedImageDecoderProxy::DecodeChunkPassed(
    std::unique_ptr<std::string> data) {
  load_message_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Decoder::DecodeChunkPassed,
                 base::Unretained(image_decoder_.get()), base::Passed(&data)));
}

void ThreadedImageDecoderProxy::Finish() {
  load_message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ImageDecoder::Finish,
                            base::Unretained(image_decoder_.get())));
}

bool ThreadedImageDecoderProxy::Suspend() {
  load_message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(base::IgnoreResult(&ImageDecoder::Suspend),
                            base::Unretained(image_decoder_.get())));
  return true;
}

void ThreadedImageDecoderProxy::Resume(
    render_tree::ResourceProvider* resource_provider) {
  load_message_loop_->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&ImageDecoder::Resume, base::Unretained(image_decoder_.get()),
                 resource_provider));
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
