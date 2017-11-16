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

#ifndef COBALT_LOADER_LOADER_H_
#define COBALT_LOADER_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// Loader class consists of a Fetcher and a Decoder, that loads and decodes a
// resource respectively.
class Loader {
 public:
  typedef base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> FetcherCreator;
  typedef base::Callback<void(Loader*)> OnDestructionFunction;
  typedef base::Callback<void(const std::string&)> OnErrorFunction;

  // The construction of Loader initiates the loading. It takes the ownership
  // of a Decoder and creates and manages a Fetcher using the given creation
  // method.
  // The fetcher creator, decoder and error callback shouldn't be NULL.
  // It is allowed to destroy the loader in the error callback.
  Loader(const FetcherCreator& fetcher_creator, scoped_ptr<Decoder> decoder,
         const OnErrorFunction& on_error,
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

 private:
  class FetcherToDecoderAdapter;

  // Starts the fetch-and-decode.
  void Start();

  const FetcherCreator fetcher_creator_;

  scoped_ptr<Decoder> decoder_;
  scoped_ptr<FetcherToDecoderAdapter> fetcher_to_decoder_adaptor_;
  scoped_ptr<Fetcher> fetcher_;

  base::CancelableClosure fetcher_creator_error_closure_;
  base::ThreadChecker thread_checker_;

  const OnErrorFunction on_error_;
  const OnDestructionFunction on_destruction_;

  bool is_suspended_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_LOADER_H_
