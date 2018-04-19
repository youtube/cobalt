/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "cobalt/loader/cors_preflight_cache.h"

namespace {
bool MethodNameToRequestType(const std::string& method,
                             net::URLFetcher::RequestType* request_type) {
  if (LowerCaseEqualsASCII(method, "get")) {
    *request_type = net::URLFetcher::GET;
  } else if (LowerCaseEqualsASCII(method, "post")) {
    *request_type = net::URLFetcher::POST;
  } else if (LowerCaseEqualsASCII(method, "head")) {
    *request_type = net::URLFetcher::HEAD;
  } else if (LowerCaseEqualsASCII(method, "delete")) {
    *request_type = net::URLFetcher::DELETE_REQUEST;
  } else if (LowerCaseEqualsASCII(method, "put")) {
    *request_type = net::URLFetcher::PUT;
  } else {
    return false;
  }
  return true;
}
const char* kAuthorization = "authorization";
}  // namespace

namespace cobalt {
namespace loader {

void CORSPreflightCache::AppendEntry(
    const std::string& url_str, const std::string& origin, int max_age,
    bool has_credentials, const std::vector<std::string>& methods_vec,
    const std::vector<std::string>& headernames_vec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (max_age <= 0) {
    return;
  }
  base::TimeDelta valid_duration = base::TimeDelta::FromSeconds(max_age);
  scoped_refptr<CORSPreflightCacheEntry> new_entry =
      new CORSPreflightCacheEntry();
  new_entry->credentials = has_credentials;
  new_entry->expiration_time = base::Time::Now() + valid_duration;

  if (methods_vec.size() == 1 && methods_vec.at(0) == "*") {
    new_entry->allow_all_methods = true;
  } else {
    for (const auto& method : methods_vec) {
      net::URLFetcher::RequestType request_type;
      if (MethodNameToRequestType(method, &request_type)) {
        new_entry->methods.insert(request_type);
      }
    }
  }

  if (headernames_vec.size() == 1 && headernames_vec.at(0) == "*") {
    new_entry->allow_all_headers_except_non_wildcard = true;
  }
  // TODO: Consider change this function to use std::copy with std::inserter.
  // Currently compilers on some machines do not support it.
  for (const auto& headername : headernames_vec) {
    new_entry->headernames.insert(headername);
  }

  auto insert_result =
      content_[url_str].insert(std::make_pair(origin, new_entry));
  if (!insert_result.second) {
    insert_result.first->second = new_entry;
  }

  expiration_time_heap_.push(
      ExpirationHeapEntry{new_entry->expiration_time, url_str, origin});
}

// https://fetch.spec.whatwg.org/#concept-cache-match
bool CORSPreflightCache::HaveEntry(
    const std::string& url_str, const std::string& origin,
    bool credentials_mode_is_include,
    const net::URLFetcher::RequestType& new_request_method,
    const std::vector<std::string>& unsafe_headernames) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (url_str.empty() || origin.empty()) {
    return false;
  }
  ClearObsoleteEntries();
  // There is a cache match for request if origin is request's origin,
  // url is request's current url, either credentials is true or request's
  // credentials_mode is not "include"
  auto url_iter = content_.find(url_str);
  if (url_iter == content_.end()) {
    return false;
  }
  auto origin_iter = url_iter->second.find(origin);
  if (origin_iter == url_iter->second.end()) {
    return false;
  }
  auto entry_ptr = origin_iter->second;
  if (!entry_ptr->credentials && credentials_mode_is_include) {
    return false;
  }
  // Either last preflight's Access-Control-Allow-Methods header has * or
  // new request's method, otherwise return false.
  if (!entry_ptr->allow_all_methods &&
      entry_ptr->methods.find(new_request_method) == entry_ptr->methods.end()) {
    return false;
  }
  // Header name is safe if it's not CORS non-wildcard request-header
  // name("Authentication") and last preflight allowed * headers.
  if (entry_ptr->allow_all_headers_except_non_wildcard) {
    bool has_auth_header = false;
    for (const auto& header : unsafe_headernames) {
      if (SbStringCompareNoCase(header.c_str(), kAuthorization)) {
        has_auth_header = true;
        break;
      }
    }
    // wildcard header is allowed if entry's allowed headers include it.
    return !has_auth_header ||
           (entry_ptr->headernames.find(std::string(kAuthorization)) !=
            entry_ptr->headernames.end());
  }
  // If last preflight does not allow arbitrary header, then match each header
  // with allowed headers.
  for (const auto& unsafe_headername : unsafe_headernames) {
    if (entry_ptr->headernames.find(unsafe_headername) ==
        entry_ptr->headernames.end()) {
      return false;
    }
  }
  return true;
}

void CORSPreflightCache::ClearObsoleteEntries() {
  while (expiration_time_heap_.size() > 0 &&
         expiration_time_heap_.top().expiration_time < base::Time::Now()) {
    auto heap_top = expiration_time_heap_.top();
    expiration_time_heap_.pop();
    auto url_iter = content_.find(heap_top.url_str);
    if (url_iter == content_.end()) {
      continue;
    }
    auto entry_iter = url_iter->second.find(heap_top.origin);
    if (entry_iter == url_iter->second.end()) {
      continue;
    }
    // The entry could have been updated and should only delete obselete ones.
    if (entry_iter->second->expiration_time < base::Time::Now()) {
      url_iter->second.erase(entry_iter);
    }
  }
}

}  // namespace loader
}  // namespace cobalt
