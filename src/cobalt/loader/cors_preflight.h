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

#ifndef COBALT_LOADER_CORS_PREFLIGHT_H_
#define COBALT_LOADER_CORS_PREFLIGHT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/cors_preflight_cache.h"
#include "cobalt/network/network_module.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace cobalt {
namespace loader {

// This class provide implementation for sending cors-preflight request and
// all cors-related checks. All input origin strings should be serialized
// origins.
class CORSPreflight : public net::URLFetcherDelegate {
 public:
  CORSPreflight(GURL url, net::URLFetcher::RequestType method,
                const network::NetworkModule* network_module,
                base::Closure success_callback, std::string origin,
                base::Closure error_callback,
                scoped_refptr<CORSPreflightCache> preflight_cache);
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void set_force_preflight(bool forcepreflight) {
    force_preflight_ = forcepreflight;
  }
  void set_credentials_mode_is_include(bool credentialsmodeisinclude) {
    credentials_mode_is_include_ = credentialsmodeisinclude;
  }
  void set_headers(const net::HttpRequestHeaders& headers) {
    headers_ = headers;
  }
  // Determine if CORS Preflight is needed by a cross origin request
  bool IsPreflightNeeded();
  // The send method can be called after initializing this class.
  // If a preflight is needed, send will send that preflight and return
  // true; if not send will just return false.
  bool Send();
  // CORS Check is done on response to ensure simple CORS request is
  // allowed by the server.
  static bool CORSCheck(const net::HttpResponseHeaders& response_headers,
                        const std::string& serialized_origin,
                        bool credentials_mode_is_include);
  // Checks if a header(a name-value pair) is a CORS-Safelisted request-header.
  static bool IsSafeRequestHeader(const std::string& name,
                                  const std::string& value);
  // Returns true if name is a CORS-Safelisted response-header.
  // Call GetHeadersVector before to put server-allowed headers in vector.
  static bool IsSafeResponseHeader(
      const std::string& name,
      const std::vector<std::string>& CORS_exposed_header_name_list,
      bool credentials_mode_is_include);
  // This function populates input vector with headers extracted from
  // Access-Control-Expose-Headers header.
  static void GetServerAllowedHeaders(
      const net::HttpResponseHeaders& response_headers,
      std::vector<std::string>* expose_headers);

 private:
  void Start();

  bool credentials_mode_is_include_;
  bool force_preflight_;
  GURL url_;
  net::URLFetcher::RequestType method_;
  const network::NetworkModule* network_module_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  base::ThreadChecker thread_checker_;
  net::HttpRequestHeaders headers_;
  std::string origin_;
  base::Closure error_callback_;
  base::Closure success_callback_;
  scoped_refptr<CORSPreflightCache> preflight_cache_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_CORS_PREFLIGHT_H_
