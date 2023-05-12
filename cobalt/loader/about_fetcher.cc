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

#include "cobalt/loader/about_fetcher.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"

namespace cobalt {
namespace loader {

AboutFetcher::AboutFetcher(Handler* handler)
    : Fetcher(handler),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&AboutFetcher::Fetch, weak_ptr_factory_.GetWeakPtr()));
}

void AboutFetcher::Fetch() { handler()->OnDone(this); }

}  // namespace loader
}  // namespace cobalt
