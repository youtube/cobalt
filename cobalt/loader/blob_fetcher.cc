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

#include "cobalt/loader/blob_fetcher.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace cobalt {
namespace loader {

BlobFetcher::BlobFetcher(const GURL& url, Handler* handler,
                         const ResolverCallback& resolver_callback)
    : Fetcher(handler),
      url_(url),
      resolver_callback_(resolver_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(!resolver_callback_.is_null());

  Fetch();
}

void BlobFetcher::Fetch() {
  size_t buffer_size = 0;
  const char* buffer_data = NULL;

  if (resolver_callback_.Run(url_, &buffer_data, &buffer_size)) {
    if (buffer_size > 0) {
      handler()->OnReceived(this, buffer_data, buffer_size);
    }
    handler()->OnDone(this);
  } else {
    handler()->OnError(this, "Blob URL not found in object store.");
  }
}

BlobFetcher::~BlobFetcher() {}

}  // namespace loader
}  // namespace cobalt
