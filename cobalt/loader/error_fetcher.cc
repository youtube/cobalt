// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/error_fetcher.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace cobalt {
namespace loader {

ErrorFetcher::ErrorFetcher(Handler* handler, const std::string& error_message)
    : Fetcher(handler),
      error_message_(error_message),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ErrorFetcher::Fetch, weak_ptr_factory_.GetWeakPtr()));
}

void ErrorFetcher::Fetch() { handler()->OnError(this, error_message_); }

}  // namespace loader
}  // namespace cobalt
