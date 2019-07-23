// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/fetcher_cache.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/loader/loader_types.h"

namespace cobalt {
namespace loader {

class FetcherCache::CachedFetcherHandler : public Fetcher::Handler {
 public:
  typedef base::Callback<void(
      CachedFetcherHandler* handler, const std::string& url,
      const scoped_refptr<net::HttpResponseHeaders>& headers,
      const Origin& last_url_origin, std::string* data)>
      SuccessCallback;
  typedef base::Callback<void(CachedFetcherHandler* handler)> FailureCallback;

  CachedFetcherHandler(const std::string& url, Fetcher::Handler* handler,
                       const SuccessCallback& on_success_callback,
                       const FailureCallback& on_failure_callback)
      : url_(url),
        handler_(handler),
        on_success_callback_(on_success_callback),
        on_failure_callback_(on_failure_callback) {
    DCHECK(handler_);
    DCHECK(!on_success_callback_.is_null());
    DCHECK(!on_failure_callback_.is_null());
  }

 private:
  // From Fetcher::Handler.
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override {
    // TODO: Respect HttpResponseHeaders::GetMaxAgeValue().
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    auto response = handler_->OnResponseStarted(fetcher, headers);
    if (response == kLoadResponseContinue && headers) {
      headers_ = headers;
      auto content_length = headers_->GetContentLength();
      if (content_length > 0) {
        data_.reserve(static_cast<size_t>(content_length));
      }
    }
    return response;
  }

  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    data_.insert(data_.end(), data, data + size);
    handler_->OnReceived(fetcher, data, size);
  }
  void OnReceivedPassed(Fetcher* fetcher,
                        std::unique_ptr<std::string> data) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    data_.insert(data_.end(), data->begin(), data->end());
    handler_->OnReceivedPassed(fetcher, std::move(data));
  }
  void OnDone(Fetcher* fetcher) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    last_url_origin_ = fetcher->last_url_origin();
    handler_->OnDone(fetcher);
    on_success_callback_.Run(this, url_, headers_, last_url_origin_, &data_);
  }
  void OnError(Fetcher* fetcher, const std::string& error) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    handler_->OnError(fetcher, error);
    on_failure_callback_.Run(this);
  }

  THREAD_CHECKER(thread_checker_);

  const std::string url_;
  Fetcher::Handler* const handler_;
  const SuccessCallback on_success_callback_;
  const FailureCallback on_failure_callback_;

  scoped_refptr<net::HttpResponseHeaders> headers_;
  Origin last_url_origin_;
  std::string data_;
};

class FetcherCache::CacheEntry {
 public:
  CacheEntry(const scoped_refptr<net::HttpResponseHeaders>& headers,
             const Origin& last_url_origin, std::string* data)
      : headers_(headers), last_url_origin_(last_url_origin) {
    DCHECK(data);
    data_.swap(*data);
  }

  const scoped_refptr<net::HttpResponseHeaders>& headers() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return headers_;
  }
  const Origin& last_url_origin() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return last_url_origin_;
  }
  const char* data() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return data_.data();
  }
  size_t size() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return data_.size();
  }

 private:
  THREAD_CHECKER(thread_checker_);

  scoped_refptr<net::HttpResponseHeaders> headers_;
  Origin last_url_origin_;
  std::string data_;
};

class FetcherCache::CachedFetcher : public Fetcher {
 public:
  CachedFetcher(const GURL& url, const FetcherCache::CacheEntry& entry,
                Handler* handler)
      : Fetcher(handler) {
    SetLastUrlOrigin(entry.last_url_origin());
    auto response_type = handler->OnResponseStarted(this, entry.headers());
    if (response_type == kLoadResponseAbort) {
      std::string error_msg(base::StringPrintf(
          "Handler::OnResponseStarted() aborted URL %s", url.spec().c_str()));
      handler->OnError(this, error_msg.c_str());
      return;
    }
    handler->OnReceived(this, entry.data(), entry.size());
    handler->OnDone(this);
  }
};

FetcherCache::FetcherCache(const char* name, size_t capacity)
    : capacity_(capacity),
      memory_size_in_bytes_(
          base::StringPrintf("Memory.%s.FetcherCache.Size", name), 0,
          "Total number of bytes currently used by the cache."),
      count_resources_cached_(
          base::StringPrintf("Count.%s.FetcherCache.Cached", name), 0,
          "The number of resources that are currently being cached.") {}

FetcherCache::~FetcherCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  while (!ongoing_fetchers_.empty()) {
    delete *ongoing_fetchers_.begin();
    ongoing_fetchers_.erase(ongoing_fetchers_.begin());
  }

  while (!cache_entries_.empty()) {
    delete cache_entries_.begin()->second;
    cache_entries_.erase(cache_entries_.begin());
  }

  memory_size_in_bytes_ = 0;
  count_resources_cached_ = 0;
}

Loader::FetcherCreator FetcherCache::GetFetcherCreator(
    const GURL& url, const Loader::FetcherCreator& real_fetcher_creator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!real_fetcher_creator.is_null());

  return base::Bind(&FetcherCache::CreateCachedFetcher, base::Unretained(this),
                    url, real_fetcher_creator);
}

void FetcherCache::NotifyResourceRequested(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iter = cache_entries_.find(url);
  if (iter != cache_entries_.end()) {
    auto entry = iter->second;
    cache_entries_.erase(iter);
    cache_entries_.insert(std::make_pair(url, entry));
  }
}

std::unique_ptr<Fetcher> FetcherCache::CreateCachedFetcher(
    const GURL& url, const Loader::FetcherCreator& real_fetcher_creator,
    Fetcher::Handler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!real_fetcher_creator.is_null());
  DCHECK(handler);

  auto iterator = cache_entries_.find(url.spec());
  if (iterator != cache_entries_.end()) {
    auto entry = iterator->second;
    cache_entries_.erase(iterator);
    cache_entries_.insert(std::make_pair(url.spec(), entry));
    return std::unique_ptr<Fetcher>(new CachedFetcher(url, *entry, handler));
  }

  auto cached_handler = new CachedFetcherHandler(
      url.spec(), handler,
      base::Bind(&FetcherCache::OnFetchSuccess, base::Unretained(this)),
      base::Bind(&FetcherCache::OnFetchFailure, base::Unretained(this)));
  ongoing_fetchers_.insert(cached_handler);
  return real_fetcher_creator.Run(cached_handler);
}

void FetcherCache::OnFetchSuccess(
    CachedFetcherHandler* handler, const std::string& url,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const Origin& last_url_origin, std::string* data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(data);

  auto iterator = ongoing_fetchers_.find(handler);
  DCHECK(iterator != ongoing_fetchers_.end());

  if (data->size() <= capacity_) {
    auto entry = new CacheEntry(headers, last_url_origin, data);
    total_size_ += entry->size();
    cache_entries_.insert(std::make_pair(url, entry));
    while (total_size_ > capacity_) {
      DCHECK(!cache_entries_.empty());
      total_size_ -= cache_entries_.begin()->second->size();
      delete cache_entries_.begin()->second;
      cache_entries_.erase(cache_entries_.begin());
      --count_resources_cached_;
    }
    ++count_resources_cached_;
    memory_size_in_bytes_ = total_size_;
  }

  delete *iterator;
  ongoing_fetchers_.erase(iterator);
}

void FetcherCache::OnFetchFailure(CachedFetcherHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iterator = ongoing_fetchers_.find(handler);
  DCHECK(iterator != ongoing_fetchers_.end());
  delete *iterator;
  ongoing_fetchers_.erase(iterator);
}

}  // namespace loader
}  // namespace cobalt
