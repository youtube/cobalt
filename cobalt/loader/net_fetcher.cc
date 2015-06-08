/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/net_fetcher.h"

#include <string>

#include "base/stringprintf.h"
#include "cobalt/network/network_module.h"
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace loader {

NetFetcher::NetFetcher(const GURL& url, Handler* handler,
                       const network::NetworkModule* network_module,
                       const Options& options)
    : Fetcher(handler) {
  url_fetcher_.reset(
      net::URLFetcher::Create(url, options.request_method, this));
  url_fetcher_->SetRequestContext(network_module->url_request_context_getter());
  url_fetcher_->SetExtraRequestHeaders(options.request_headers);
  if (options.request_body.size()) {
    // If applicable, the request body Content-Type is already set in
    // options.request_headers.
    url_fetcher_->SetUploadData("", options.request_body);
  }
  url_fetcher_->Start();
}

void NetFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::URLRequestStatus& status = source->GetStatus();
  if (status.is_success()) {
    std::string response_string;
    if (source->GetResponseAsString(&response_string)) {
      handler()->OnReceived(response_string.c_str(), response_string.length());
    } else {
      // The URLFetcher was misconfigured in this case.
      NOTREACHED() << "GetResponseAsString() failed.";
    }
    handler()->OnDone();
  } else {
    handler()->OnError(base::StringPrintf("NetFetcher error: %s",
                                          net::ErrorToString(status.error())));
  }
}

NetFetcher::~NetFetcher() { DCHECK(thread_checker_.CalledOnValidThread()); }

}  // namespace loader
}  // namespace cobalt
