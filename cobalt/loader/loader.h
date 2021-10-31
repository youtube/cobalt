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

#ifndef COBALT_LOADER_LOADER_H_
#define COBALT_LOADER_LOADER_H_

#include <memory>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// Loader class consists of a Fetcher and a Decoder, that loads and decodes a
// resource respectively.
class Loader {
 public:
  typedef base::Callback<std::unique_ptr<Fetcher>(Fetcher::Handler*)>
      FetcherCreator;
  typedef base::Callback<void(Loader*)> OnDestructionFunction;
  typedef base::Callback<void(const base::Optional<std::string>&)>
      OnCompleteFunction;
  typedef base::Callback<std::unique_ptr<Decoder>(const OnCompleteFunction&)>
      DecoderCreator;

  // The construction of Loader initiates the loading. It takes the ownership
  // of a Decoder and creates and manages a Fetcher using the given creation
  // method.
  // The fetcher creator, decoder and error callback shouldn't be NULL.
  // It is allowed to destroy the loader in the error callback.
  Loader(const FetcherCreator& fetcher_creator,
         const DecoderCreator& decoder_creator,
         const OnCompleteFunction& on_load_complete,
         const OnDestructionFunction& on_destruction = OnDestructionFunction(),
         bool is_suspended = false);

  ~Loader();

  // Suspends the load of this resource, expecting it to be resumed or destroyed
  // later.
  void Suspend();

  // Resumes the load of this resource. Suspend must have been previously
  // called.
  void Resume(render_tree::ResourceProvider* resource_provider);

  bool DidFailFromTransientError() const;

  void LoadComplete(const base::Optional<std::string>& status);

  net::LoadTimingInfo get_load_timing_info();
  void set_load_timing_info(const net::LoadTimingInfo& timing_info);

 private:
  class FetcherHandlerToDecoderAdapter;

  // Starts the fetch-and-decode.
  void Start();

  const FetcherCreator fetcher_creator_;
  const DecoderCreator decoder_creator_;

  std::unique_ptr<Decoder> decoder_;
  std::unique_ptr<FetcherHandlerToDecoderAdapter>
      fetcher_handler_to_decoder_adaptor_;
  std::unique_ptr<Fetcher> fetcher_;

  base::CancelableClosure fetcher_creator_error_closure_;
  THREAD_CHECKER(thread_checker_);

  const OnCompleteFunction on_load_complete_;
  const OnDestructionFunction on_destruction_;

  bool is_suspended_;
  bool is_load_complete_ = false;

  net::LoadTimingInfo load_timing_info_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_LOADER_H_
