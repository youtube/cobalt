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

#ifndef COBALT_SPEECH_SANDBOX_AUDIO_LOADER_H_
#define COBALT_SPEECH_SANDBOX_AUDIO_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace speech {
namespace sandbox {

class AudioLoader {
 public:
  typedef base::Callback<void(const uint8*, int)> DoneCallback;
  AudioLoader(const GURL& url, network::NetworkModule* network_module,
              const DoneCallback& callback);
  ~AudioLoader();

 private:
  void OnLoadingDone(const uint8* data, int size);
  void OnLoadingError(const base::Optional<std::string>& error);

  const DoneCallback done_callback_;
  network::NetworkModule* network_module_;
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  std::unique_ptr<loader::Loader> loader_;
};

}  // namespace sandbox
}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SANDBOX_AUDIO_LOADER_H_
