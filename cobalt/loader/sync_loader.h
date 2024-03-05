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

#ifndef COBALT_LOADER_SYNC_LOADER_H_
#define COBALT_LOADER_SYNC_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// Synchronously loads the resource. This function will block the main thread
// and use the given task runner to load the resource. The fetcher and decoder
// are responsible for setting up timeout for themselves.
void LoadSynchronously(
    base::SequencedTaskRunner* task_runner,
    base::WaitableEvent* interrupt_trigger,
    base::Callback<std::unique_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    base::Callback<std::unique_ptr<Decoder>()> decoder_creator,
    base::Callback<void(const base::Optional<std::string>&)>
        load_complete_callback);

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_SYNC_LOADER_H_
