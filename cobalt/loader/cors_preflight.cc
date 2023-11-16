/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/cors_preflight.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace loader {

namespace {
// definition of the following headers can be found at:
// https://fetch.spec.whatwg.org/#http-access-control-allow-origin
const char* kOriginheadername = "Origin: ";
const char* kAccessControlRequestMethod = "Access-Control-Request-Method: ";
const char* kAccessControlRequestHeaders = "Access-Control-Request-Headers: ";
const char* kAccessControlAllowOrigin = "Access-Control-Allow-Origin";
const char* kAccessControlAllowMethod = "Access-Control-Allow-Methods";
const char* kAccessControlAllowHeaders = "Access-Control-Allow-Headers";
const char* kAccessControlAllowCredentials = "Access-Control-Allow-Credentials";
const char* kAccessControlMaxAge = "Access-Control-Max-Age";
// https://fetch.spec.whatwg.org/#http-access-control-expose-headers
const char* kAccessControlExposeHeaders = "Access-Control-Expose-Headers";

// The following constants are used to decide if a request or response header is
// safe or not.
const char* kCORSSafelistedRequestHeaders[] = {"accept", "accept-language",
                                               "content-language"};
const char* kAllowedMIMEType[] = {"application/x-www-form-urlencoded",
                                  "multipart/form-data", "text/plain"};
const char* kSafelistedHeadernofail[] = {"dpr", "downlink", "save-data",
                                         "viewport-width", "width"};
const char* kContentType = "content-type";
const char* kAuthorization = "authorization";
const char* kMethodNames[] = {"GET",    "POST", "HEAD",
                              "DELETE", "PUT",  "OPTIONS"};
const char* kCORSSafelistedResponseHeaders[] = {
    "cache-control", "content-language", "content-type",
    "expires",       "last-modified",    "pragma"};
const char* kForbiddenHeaders[] = {"accept-charset",
                                   "accept-encoding",
                                   "access-control-request-headers",
                                   "access-control-request-method",
                                   "connection",
                                   "content-length",
                                   "cookie",
                                   "cookie2",
                                   "date",
                                   "dnt",
                                   "expect",
                                   "host",
                                   "keep-alive",
                                   "origin",
                                   "referer",
                                   "te",
                                   "trailer",
                                   "transfer-encoding",
                                   "upgrade",
                                   "via"};

// Returns true if input method is a CORS-safelisted method.
bool IsCORSSafelistedMethod(net::URLFetcher::RequestType input_type) {
  if (input_type == net::URLFetcher::GET ||
      input_type == net::URLFetcher::HEAD ||
      input_type == net::URLFetcher::POST) {
    return true;
  }
  return false;
}

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
const char* RequestTypeToMethodName(net::URLFetcher::RequestType request_type) {
  if (request_type >= 0 && request_type < arraysize(kMethodNames)) {
    return kMethodNames[request_type];
  } else {
    NOTREACHED();
    return "";
  }
}
#if __clang__
#pragma clang diagnostic push
#endif

// This constant is an imposed limit on the time an entry can be alive in
// the preflight cache if the provided max-age value is even greater.
// The number is the same as the limit in WebKit.
const int kPreflightCacheMaxAgeLimit = 600;

// This helper function checks if 'input_str' is in 'array' up to 'size'.
bool IsInArray(const char* input_str, const char* array[], size_t size) {
  if (!input_str || *input_str == '\0') {
    return false;
  }
  for (size_t i = 0; i < size; ++i) {
    if (strcasecmp(input_str, array[i]) == 0) {
      return true;
    }
  }
  return false;
}

// Returns true if there is a case-insensitive match of 'find_value_name' in
// 'field_values'.
bool HasFieldValue(const std::vector<std::string>& field_values,
                   const std::string& find_value_name) {
  for (size_t i = 0; i < field_values.size(); i++) {
    if (field_values[i].empty()) {
      continue;
    }
    if (strcasecmp(field_values[i].c_str(), find_value_name.c_str()) == 0) {
      return true;
    }
  }
  return false;
}
}  // namespace

CORSPreflight::CORSPreflight(GURL url, net::URLFetcher::RequestType method,
                             const network::NetworkModule* network_module,
                             base::Closure success_callback, std::string origin,
                             base::Closure error_callback,
                             scoped_refptr<CORSPreflightCache> preflight_cache)
    : credentials_mode_is_include_(false),
      force_preflight_(false),
      url_(url),
      method_(method),
      network_module_(network_module),
      origin_(origin),
      error_callback_(error_callback),
      success_callback_(success_callback),
      preflight_cache_(preflight_cache) {
  DCHECK(!url_.is_empty());
  DCHECK(preflight_cache);
}

// https://fetch.spec.whatwg.org/#cors-safelisted-request-header
bool CORSPreflight::IsSafeRequestHeader(const std::string& name,
                                        const std::string& value) {
  // All comparison are case-insensitive.
  // Header is safe if it's CORS-safelisted request-header.
  if (IsInArray(name.c_str(), kCORSSafelistedRequestHeaders,
                arraysize(kCORSSafelistedRequestHeaders))) {
    return true;
  }

  // Safe if header name is 'Content-Type' and value is a match of
  // kAllowedMIMEType.
  if (strcasecmp(name.c_str(), kContentType) == 0) {
    std::vector<std::string> content_type_split = base::SplitString(
        value, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    auto begin_iter = content_type_split[0].cbegin();
    auto end_iter = content_type_split[0].cend();
    net::HttpUtil::TrimLWS(&begin_iter, &end_iter);
    std::string content_type_no_space(begin_iter, end_iter);
    if (IsInArray(content_type_no_space.c_str(), kAllowedMIMEType,
                  arraysize(kAllowedMIMEType))) {
      return true;
    }
  }
  // Safe if name is a match for kSafelistedHeadernofail and whose value, once
  // extracted, is not failure.
  if (IsInArray(name.c_str(), kSafelistedHeadernofail,
                arraysize(kSafelistedHeadernofail))) {
    // TODO: The extracting and verify result is not failure is not done yet.
    // https://fetch.spec.whatwg.org/#extract-header-values
    return true;
  }
  return false;
}

// https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name
bool CORSPreflight::IsSafeResponseHeader(
    const std::string& name,
    const std::vector<std::string>& CORS_exposed_header_name_list,
    bool credentials_mode_is_include) {
  // Every check in this function is case-insensitive comparison.
  // Header is safe if it's CORS-safelisted response-header name.
  if (IsInArray(name.c_str(), kCORSSafelistedResponseHeaders,
                arraysize(kCORSSafelistedResponseHeaders))) {
    return true;
  }
  // It's not safe if it's a forbidden header name.
  if (IsInArray(name.c_str(), kForbiddenHeaders,
                arraysize(kForbiddenHeaders))) {
    return false;
  }
  // The following two steps checks if given header name is in CORS-exposed
  // header-name list. If Access-Control-Expose-Headers header is '*', all
  // header names in response should be in CORS-exposed header-name list.
  if (CORS_exposed_header_name_list.size() == 1 &&
      CORS_exposed_header_name_list.at(0) == "*" &&
      !credentials_mode_is_include) {
    return true;
  }

  for (size_t i = 0; i < CORS_exposed_header_name_list.size(); i++) {
    if (strcasecmp(CORS_exposed_header_name_list.at(i).c_str(), name.c_str()) ==
        0) {
      return true;
    }
  }
  return false;
}

void CORSPreflight::GetServerAllowedHeaders(
    const net::HttpResponseHeaders& response_headers,
    std::vector<std::string>* expose_headers) {
  size_t iter = 0;
  std::string exposable_header;
  while (response_headers.EnumerateHeader(&iter, kAccessControlExposeHeaders,
                                          &exposable_header)) {
    expose_headers->push_back(exposable_header);
  }
}

bool CORSPreflight::IsPreflightNeeded() {
  // Send preflight if force_preflight flag is on.
  if (force_preflight_) {
    return true;
  }
  // Preflight is not needed if the request method is CORS-safelisted request
  // method and all headers are CORS-safelisted request-header.
  std::vector<std::string> unsafe_headers;
  if (method_ == net::URLFetcher::GET || method_ == net::URLFetcher::HEAD ||
      method_ == net::URLFetcher::POST) {
    net::HttpRequestHeaders::Iterator it(headers_);
    while (it.GetNext()) {
      if (!IsSafeRequestHeader(it.name(), it.value())) {
        unsafe_headers.push_back(it.name());
      }
    }
    if (unsafe_headers.empty()) {
      return false;
    }
  }
  // Check preflight cache for match.
  return !preflight_cache_->HaveEntry(url_.spec(), origin_,
                                      credentials_mode_is_include_, method_,
                                      unsafe_headers);
}

bool CORSPreflight::Send() {
  if (!IsPreflightNeeded()) {
    return false;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch-0

  // 1. Let preflight be a new request whose method is 'OPTIONS', url is
  //    request's current url, initiator is request's initiator, type is
  //    request's type, destination is request's destination, origin is
  //    request's origin, referrer is request's referrer, and referrer
  //    policy is request's referrer policy.
  url_fetcher_ = net::URLFetcher::Create(url_, net::URLFetcher::OPTIONS, this);
  url_fetcher_->SetRequestContext(
      network_module_->url_request_context_getter().get());
  network_module_->AddClientHintHeaders(*url_fetcher_,
                                        network::kCallTypePreflight);
  url_fetcher_->AddExtraRequestHeader(kOriginheadername + origin_);
  // 3. Let headers be the names of request's header list's headers,
  //    excluding CORS-safelisted request-headers and duplicates, sorted
  //    lexicographically, and byte-lowercased.
  // 4. If headers is not empty, then:
  //    Let value be the items in headers separated from each other
  //    by `,`. Set `Access-Control-Request-Headers` to value in
  //    preflight's header list.
  if (!headers_.IsEmpty()) {
    net::HttpRequestHeaders::Iterator it(headers_);
    std::string headers_string;
    while (it.GetNext()) {
      if (!headers_string.empty()) {
        headers_string += ',';
      }
      headers_string += it.name();
    }
    url_fetcher_->AddExtraRequestHeader(kAccessControlRequestHeaders +
                                        headers_string);
  }
  // 2. Set `Access-Control-Request-Method` to request's method in
  //    preflight's header list.
  url_fetcher_->AddExtraRequestHeader(std::string(kAccessControlRequestMethod) +
                                      RequestTypeToMethodName(method_));
  // 5. Let response be the result of performing an HTTP-network-or-
  //    cache fetch using preflight.
  Start();
  return true;
}

void CORSPreflight::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Preflight does not allow redirect, status 300+ should not fail
  url_fetcher_->SetStopOnRedirect(true);
  url_fetcher_->Start();
}

void CORSPreflight::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    error_callback_.Run();
    return;
  }

  // Preflight response check:
  // Procedure 6 of https://fetch.spec.whatwg.org/#cors-preflight-fetch-0

  if (source->GetResponseHeaders()) {
    net::HttpResponseHeaders* response_headers = source->GetResponseHeaders();
    std::string methods, headernames;
    // If status is not ok status, return network error
    if (!CORSCheck(*response_headers, origin_, credentials_mode_is_include_,
                   cors_policy_) ||
        source->GetResponseCode() < 200 || source->GetResponseCode() > 299) {
      error_callback_.Run();
      return;
    }
    // 3. Let headerNames be the result of extracting header list values given
    // `Access-Control-Allow-Headers` and response's header list.
    if (!response_headers->GetNormalizedHeader(kAccessControlAllowMethod,
                                               &methods)) {
      // 6. If methods is null and request's use-CORS-preflight flag is set,
      //    then set methods to a new list containing request's method.
      if (force_preflight_) {
        methods = RequestTypeToMethodName(method_);
      }
    }
    response_headers->GetNormalizedHeader(kAccessControlAllowHeaders,
                                          &headernames);
    // 5. If methods or headerNames contains `*`, and request's credentials mode
    //    is "include", then return a network error.
    std::vector<std::string> methods_vec = base::SplitString(
        methods, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::vector<std::string> headernames_vec = base::SplitString(
        headernames, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if ((HasFieldValue(methods_vec, "*") ||
         HasFieldValue(headernames_vec, "*")) &&
        credentials_mode_is_include_) {
      error_callback_.Run();
      return;
    }  // 7. If request's method is not in methods, is not a CORS-safelisted
    //    method, and methods does not contain `*`, then return a network error.
    if (!HasFieldValue(methods_vec, RequestTypeToMethodName(method_)) &&
        !IsCORSSafelistedMethod(method_) && !HasFieldValue(methods_vec, "*")) {
      error_callback_.Run();
      return;
    }
    // 8. If one of request's header list's names is a CORS non-wildcard
    //    request-header name and is not a byte-case-insensitive match for an
    //    item in headerNames, then return a network error.
    // 9. If one of request's header list' names is not a byte-case-insensitive
    //    match for an item in headerNames, its corresponding header is not a
    //    CORS-safelisted request-header, and headerNames does not contain `*`,
    //    then return a network error.
    net::HttpRequestHeaders::Iterator it(headers_);
    while (it.GetNext()) {
      if (HasFieldValue(headernames_vec, it.name())) {
        continue;
      }
      if (strcasecmp(it.name().c_str(), kAuthorization) == 0 ||
          (!HasFieldValue(headernames_vec, "*") &&
           !IsSafeRequestHeader(it.name(), it.value()))) {
        error_callback_.Run();
        return;
      }
    }
    // step 10-18 for adding entry to preflight cache.
    std::string max_age_str;
    int max_age = 0;
    if (response_headers->GetNormalizedHeader(kAccessControlMaxAge,
                                              &max_age_str)) {
      max_age = std::min(atoi(max_age_str.c_str()), kPreflightCacheMaxAgeLimit);
    }
    preflight_cache_->AppendEntry(source->GetURL().spec(), origin_, max_age,
                                  credentials_mode_is_include_, methods_vec,
                                  headernames_vec);
  } else {
    DLOG(ERROR) << "CORS preflight did not get response headers";
    error_callback_.Run();
  }

  success_callback_.Run();
}

// https://fetch.spec.whatwg.org/#concept-cors-check
bool CORSPreflight::CORSCheck(const net::HttpResponseHeaders& response_headers,
                              const std::string& serialized_origin,
                              bool credentials_mode_is_include,
                              network::CORSPolicy cors_policy) {
#ifndef COBALT_FORCE_CORS
  if (cors_policy == network::kCORSOptional) {
    DLOG(WARNING) << "Cors check disabled, allowing request without checking.";
    return true;
  }
#endif
  // 1. Let origin be the result of extracting header list values given `Access-
  //    Control-Allow-Origin` and response's header list.
  std::string allowed_origin, empty_container, allow_credentials;
  size_t iter = 0;
  if (!response_headers.EnumerateHeader(&iter, kAccessControlAllowOrigin,
                                        &allowed_origin)) {
    LOG(WARNING) << "Insecure cross-origin network request returned response "
                    "with no Access-Control-Allow-Origin header. Request "
                    "aborted: ";
    return false;
  }
  DCHECK(iter);
  if (response_headers.EnumerateHeader(&iter, kAccessControlAllowOrigin,
                                       &empty_container)) {
    LOG(WARNING) << "Insecure cross-origin network request returned response "
                    "with multiple Access-Control-Allow-Origin headers. "
                    "Behavior disallowed and request aborted: ";
    return false;
  }
  // 3. If request's credentials mode is not "include" and origin is `*`, return
  //    success.
  if (!credentials_mode_is_include && allowed_origin == "*") {
    return true;
  }
  // 2. If origin is null or failure, return failure.
  if (allowed_origin.empty()) {
    LOG(WARNING) << "CORS check failed: Origin is null or failure.";
    return false;
  }
  // 4. If request's origin, serialized and UTF-8 encoded, is not origin, return
  //    failure.
  if (allowed_origin != serialized_origin) {
    LOG(WARNING) << "Network request origin is not allowed by server's "
                    "Access-Control-Allow-Origin header, request aborted.";
    return false;
  }
  // 5. If request's credentials mode is not "include", return success.
  if (!credentials_mode_is_include) {
    return true;
  }
  // 6. Let credentials be the result of extracting header list values given
  //    `Access-Control-Allow-Credentials` and response's header list.
  if (response_headers.GetNormalizedHeader(kAccessControlAllowCredentials,
                                           &allow_credentials)) {
    // 7. If credentials is `true`, return success.
    if (allow_credentials != "true") {
      LOG(WARNING)
          << "Network request failed because request want to include credential"
             "but server disallow it.";
      return false;
    } else {
      return true;
    }
  }
  LOG(WARNING) << "CORS check failed.";
  return false;
}

}  // namespace loader
}  // namespace cobalt
