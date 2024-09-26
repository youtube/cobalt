// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/url_checker.h"

#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"

namespace safe_search_api {

namespace {

const size_t kDefaultCacheSize = 1000;
const size_t kDefaultCacheTimeoutSeconds = 3600;

}  // namespace

struct URLChecker::Check {
  Check(const GURL& url, CheckCallback callback);
  ~Check();

  GURL url;
  std::vector<CheckCallback> callbacks;
};

URLChecker::Check::Check(const GURL& url, CheckCallback callback) : url(url) {
  callbacks.push_back(std::move(callback));
}

URLChecker::Check::~Check() = default;

URLChecker::CheckResult::CheckResult(Classification classification,
                                     bool uncertain)
    : classification(classification),
      uncertain(uncertain),
      timestamp(base::TimeTicks::Now()) {}

URLChecker::URLChecker(std::unique_ptr<URLCheckerClient> async_checker)
    : URLChecker(std::move(async_checker), kDefaultCacheSize) {}

URLChecker::URLChecker(std::unique_ptr<URLCheckerClient> async_checker,
                       size_t cache_size)
    : async_checker_(std::move(async_checker)),
      cache_(cache_size),
      cache_timeout_(base::Seconds(kDefaultCacheTimeoutSeconds)) {}

URLChecker::~URLChecker() = default;

bool URLChecker::CheckURL(const GURL& url, CheckCallback callback) {
  auto cache_it = cache_.Get(url);
  if (cache_it != cache_.end()) {
    const CheckResult& result = cache_it->second;
    base::TimeDelta age = base::TimeTicks::Now() - result.timestamp;
    if (age < cache_timeout_) {
      DVLOG(1) << "Cache hit! " << url.spec() << " is "
               << (result.classification == Classification::UNSAFE ? "NOT" : "")
               << " safe; certain: " << !result.uncertain;
      std::move(callback).Run(url, result.classification, result.uncertain);
      return true;
    }
    DVLOG(1) << "Outdated cache entry for " << url.spec() << ", purging";
    cache_.Erase(cache_it);
  }

  // See if we already have a check in progress for this URL.
  for (const auto& check : checks_in_progress_) {
    if (check->url == url) {
      DVLOG(1) << "Adding to pending check for " << url.spec();
      check->callbacks.push_back(std::move(callback));
      return false;
    }
  }

  auto it = checks_in_progress_.insert(
      checks_in_progress_.begin(),
      std::make_unique<Check>(url, std::move(callback)));
  async_checker_->CheckURL(url,
                           base::BindOnce(&URLChecker::OnAsyncCheckComplete,
                                          weak_factory_.GetWeakPtr(), it));

  return false;
}

void URLChecker::OnAsyncCheckComplete(CheckList::iterator it,
                                      const GURL& url,
                                      ClientClassification api_classification) {
  bool uncertain = api_classification == ClientClassification::kUnknown;

  // Fallback to a |SAFE| classification when the result is not explicitly
  // marked as restricted.
  Classification classification = Classification::SAFE;
  if (api_classification == ClientClassification::kRestricted) {
    classification = Classification::UNSAFE;
  }

  std::vector<CheckCallback> callbacks = std::move(it->get()->callbacks);
  checks_in_progress_.erase(it);

  cache_.Put(url, CheckResult(classification, uncertain));

  for (size_t i = 0; i < callbacks.size(); i++)
    std::move(callbacks[i]).Run(url, classification, uncertain);
}

}  // namespace safe_search_api
