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

#include "cobalt/loader/cache_fetcher.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace cobalt {
namespace loader {

namespace {
bool CacheURLToKey(const GURL& url, std::string* key) {
  DCHECK(url.is_valid() && url.SchemeIs(kCacheScheme));
  *key = url.path();
  DCHECK_EQ('/', (*key)[0]);
  DCHECK_EQ('/', (*key)[1]);
  (*key).erase(0, 2);
  return !key->empty();
}
}  // namespace

const char kCacheScheme[] = "h5vcc-cache";

CacheFetcher::CacheFetcher(
    const GURL& url, const csp::SecurityCallback& security_callback,
    Handler* handler,
    const base::Callback<int(const std::string&, std::unique_ptr<char[]>*)>&
        read_cache_callback)
    : Fetcher(handler),
      url_(url),
      security_callback_(security_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      read_cache_callback_(read_cache_callback) {
  TRACE_EVENT0("cobalt::loader", "CacheFetcher::CacheFetcher");
  // Cache assets can be loaded synchronously.
  Fetch();
}

CacheFetcher::~CacheFetcher() {}

void CacheFetcher::Fetch() {
  if (!IsAllowedByCsp()) {
    std::string msg(base::StringPrintf("URL %s rejected by security policy.",
                                       url_.spec().c_str()));
    handler()->OnError(this, msg);
    return;
  }

  std::string key;
  if (!CacheURLToKey(url_, &key)) {
    std::string msg(
        base::StringPrintf("Invalid cache URL: %s.", url_.spec().c_str()));
    handler()->OnError(this, msg);
    return;
  }

  GetCacheData(key);
}

void CacheFetcher::GetCacheData(const std::string& key) {
  TRACE_EVENT0("cobalt::loader", "CacheFetcher::GetCacheData");
  const char kFailedToReadCache[] = "Failed to read cache.";

  DCHECK(!read_cache_callback_.is_null());
  std::unique_ptr<char[]> buffer;
  int buffer_size = read_cache_callback_.Run(key, &buffer);
  if (!buffer.get()) {
    handler()->OnError(this, kFailedToReadCache);
    return;
  }

  handler()->OnReceived(this, buffer.get(), buffer_size);
  handler()->OnDone(this);
}

bool CacheFetcher::IsAllowedByCsp() {
  bool did_redirect = false;
  if (security_callback_.is_null() ||
      security_callback_.Run(url_, did_redirect)) {
    return true;
  } else {
    return false;
  }
}

}  // namespace loader
}  // namespace cobalt
