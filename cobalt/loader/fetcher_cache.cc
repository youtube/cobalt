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
namespace {

// Wraps a Fetcher::Handler and saves all data fetched and its associated
// information.
class CachedFetcherHandler : public Fetcher::Handler {
 public:
  typedef base::Callback<void(
      const std::string& url,
      const scoped_refptr<net::HttpResponseHeaders>& headers,
      const Origin& last_url_origin, bool did_fail_from_transient_error,
      std::string* data)>
      SuccessCallback;

  CachedFetcherHandler(const std::string& url, Fetcher::Handler* handler,
                       const SuccessCallback& on_success_callback)
      : url_(url),
        handler_(handler),
        on_success_callback_(on_success_callback) {
    DCHECK(handler_);
    DCHECK(!on_success_callback_.is_null());
  }

  // Attach a wrapping fetcher so it can be used when forwarding the callbacks.
  // This ensures that the underlying handler sees the same Fetcher object in
  // the callback as the one returned by CreateCachedFetcher().
  void AttachFetcher(Fetcher* wrapping_fetcher) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!wrapping_fetcher_);
    DCHECK(wrapping_fetcher);
    wrapping_fetcher_ = wrapping_fetcher;
  }

 private:
  // From Fetcher::Handler.
  LoadResponseType OnResponseStarted(
      Fetcher*,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override {
    // TODO: Respect HttpResponseHeaders::GetMaxAgeValue().
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(wrapping_fetcher_);
    auto response = handler_->OnResponseStarted(wrapping_fetcher_, headers);
    if (response == kLoadResponseContinue && headers) {
      headers_ = headers;
      auto content_length = headers_->GetContentLength();
      if (content_length > 0) {
        data_.reserve(static_cast<size_t>(content_length));
      }
    }
    return response;
  }

  void OnReceived(Fetcher*, const char* data, size_t size) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(wrapping_fetcher_);
    data_.insert(data_.end(), data, data + size);
    handler_->OnReceived(wrapping_fetcher_, data, size);
  }

  void OnReceivedPassed(Fetcher*, std::unique_ptr<std::string> data) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(wrapping_fetcher_);
    data_.insert(data_.end(), data->begin(), data->end());
    handler_->OnReceivedPassed(wrapping_fetcher_, std::move(data));
  }

  void OnDone(Fetcher*) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(wrapping_fetcher_);
    handler_->OnDone(wrapping_fetcher_);
    on_success_callback_.Run(
        url_, headers_, wrapping_fetcher_->last_url_origin(),
        wrapping_fetcher_->did_fail_from_transient_error(), &data_);
  }

  void OnError(Fetcher*, const std::string& error) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(wrapping_fetcher_);
    handler_->OnError(wrapping_fetcher_, error);
  }

  THREAD_CHECKER(thread_checker_);

  const std::string url_;
  Fetcher* wrapping_fetcher_ = nullptr;
  Fetcher::Handler* const handler_;
  const SuccessCallback on_success_callback_;

  scoped_refptr<net::HttpResponseHeaders> headers_;
  std::string data_;
};

// Wraps an underlying, real Fetcher for an ongoing request, so we can ensure
// that |handler_| is deleted when the Fetcher object is deleted.
class OngoingFetcher : public Fetcher {
 public:
  OngoingFetcher(std::unique_ptr<CachedFetcherHandler> handler,
                 const Loader::FetcherCreator& real_fetcher_creator)
      : Fetcher(handler.get()), handler_(std::move(handler)) {
    DCHECK(handler_);
    handler_->AttachFetcher(this);
    fetcher_ = real_fetcher_creator.Run(handler_.get());
  }
  ~OngoingFetcher() override { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

  Origin last_url_origin() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // Some Fetchers may call callbacks inside its ctor, in such case |fetcher_|
    // hasn't been assigned and a default value is returned.
    return fetcher_ ? fetcher_->last_url_origin() : Origin();
  }
  bool did_fail_from_transient_error() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // Some Fetchers may call callbacks inside its ctor, in such case |fetcher_|
    // hasn't been assigned and a default value is returned.
    return fetcher_ ? fetcher_->did_fail_from_transient_error() : false;
  }

 private:
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<CachedFetcherHandler> handler_;
  std::unique_ptr<Fetcher> fetcher_;
};

// Fulfills the request directly using the data passed to ctor.
class CachedFetcher : public Fetcher {
 public:
  CachedFetcher(const GURL& url,
                const scoped_refptr<net::HttpResponseHeaders>& headers,
                const char* data, size_t size, const Origin& last_url_origin,
                bool did_fail_from_transient_error, Handler* handler)
      : Fetcher(handler),
        last_url_origin_(last_url_origin),
        did_fail_from_transient_error_(did_fail_from_transient_error) {
    auto response_type = handler->OnResponseStarted(this, headers);
    if (response_type == kLoadResponseAbort) {
      std::string error_msg(base::StringPrintf(
          "Handler::OnResponseStarted() aborted URL %s", url.spec().c_str()));
      handler->OnError(this, error_msg.c_str());
      return;
    }
    handler->OnReceived(this, data, size);
    handler->OnDone(this);
  }

  Origin last_url_origin() const override { return last_url_origin_; }
  bool did_fail_from_transient_error() const override {
    return did_fail_from_transient_error_;
  }

 private:
  Origin last_url_origin_;
  bool did_fail_from_transient_error_;
};

}  // namespace

class FetcherCache::CacheEntry {
 public:
  CacheEntry(const scoped_refptr<net::HttpResponseHeaders>& headers,
             const Origin& last_url_origin, bool did_fail_from_transient_error,
             std::string* data)
      : headers_(headers),
        last_url_origin_(last_url_origin),
        did_fail_from_transient_error_(did_fail_from_transient_error) {
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
  bool did_fail_from_transient_error() const {
    return did_fail_from_transient_error_;
  }
  const char* data() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return data_.data();
  }
  size_t size() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return data_.size();
  }
  size_t capacity() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return data_.capacity();
  }

 private:
  THREAD_CHECKER(thread_checker_);

  scoped_refptr<net::HttpResponseHeaders> headers_;
  Origin last_url_origin_;
  bool did_fail_from_transient_error_;
  std::string data_;
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
    return std::unique_ptr<Fetcher>(
        new CachedFetcher(url, entry->headers(), entry->data(), entry->size(),
                          entry->last_url_origin(),
                          entry->did_fail_from_transient_error(), handler));
  }

  std::unique_ptr<CachedFetcherHandler> cached_handler(new CachedFetcherHandler(
      url.spec(), handler,
      base::Bind(&FetcherCache::OnFetchSuccess, base::Unretained(this))));
  return std::unique_ptr<Fetcher>(
      new OngoingFetcher(std::move(cached_handler), real_fetcher_creator));
}

void FetcherCache::OnFetchSuccess(
    const std::string& url,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const Origin& last_url_origin, bool did_fail_from_transient_error,
    std::string* data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(data);

  if (data->size() <= capacity_) {
    auto entry = new CacheEntry(headers, last_url_origin,
                                did_fail_from_transient_error, data);
    total_size_ += entry->capacity();
    cache_entries_.insert(std::make_pair(url, entry));
    while (total_size_ > capacity_) {
      DCHECK(!cache_entries_.empty());
      total_size_ -= cache_entries_.begin()->second->capacity();
      delete cache_entries_.begin()->second;
      cache_entries_.erase(cache_entries_.begin());
      --count_resources_cached_;
    }
    ++count_resources_cached_;
    memory_size_in_bytes_ = total_size_;
  }
}

}  // namespace loader
}  // namespace cobalt
