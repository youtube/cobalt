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

namespace {

bool IsResponseCodeSuccess(int response_code) {
  // NetFetcher only considers success to be if the network request
  // was successful *and* we get a 2xx response back.
  // We also accept a response code of -1 for URLs where response code is not
  // meaningful, like "data:"
  // TODO: 304s are unexpected since we don't enable the HTTP cache,
  // meaning we don't add the If-Modified-Since header to our request.
  // However, it's unclear what would happen if we did, so DCHECK.
  DCHECK_NE(response_code, 304) << "Unsupported status code";
  return response_code == -1 || response_code / 100 == 2;
}
}  // namespace

NetFetcher::NetFetcher(const GURL& url,
                       const csp::SecurityCallback& security_callback,
                       Handler* handler,
                       const network::NetworkModule* network_module,
                       const Options& options)
    : Fetcher(handler),
      security_callback_(security_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(start_callback_(
          base::Bind(&NetFetcher::Start, base::Unretained(this)))) {
  url_fetcher_.reset(
      net::URLFetcher::Create(url, options.request_method, this));
  url_fetcher_->SetRequestContext(network_module->url_request_context_getter());
  url_fetcher_->DiscardResponse();

  // Delay the actual start until this function is complete. Otherwise we might
  // call handler's callbacks at an unexpected time- e.g. receiving OnError()
  // while a loader is still being constructed.
  MessageLoop::current()->PostTask(FROM_HERE, start_callback_.callback());
}

void NetFetcher::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  const GURL& original_url = url_fetcher_->GetOriginalURL();
  if (security_callback_.is_null() ||
      security_callback_.Run(original_url, false /* did not redirect */)) {
    url_fetcher_->Start();
  } else {
    std::string msg(base::StringPrintf("URL %s rejected by security policy.",
                                       original_url.spec().c_str()));
    return HandleError(msg).InvalidateThis();
  }
}

void NetFetcher::OnURLFetchResponseStarted(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source->GetURL() != source->GetOriginalURL()) {
    // A redirect occured. Re-check the security policy.
    if (!security_callback_.is_null() &&
        !security_callback_.Run(source->GetURL(), true /* did redirect */)) {
      std::string msg(base::StringPrintf(
          "URL %s rejected by security policy after a redirect from %s.",
          source->GetURL().spec().c_str(),
          source->GetOriginalURL().spec().c_str()));
      return HandleError(msg).InvalidateThis();
    }
  }

  if (IsResponseCodeSuccess(source->GetResponseCode())) {
    if (handler()->OnResponseStarted(this, source->GetResponseHeaders()) ==
        kLoadResponseAbort) {
      std::string msg(
          base::StringPrintf("Handler::OnResponseStarted aborted URL %s",
                             source->GetURL().spec().c_str()));
      return HandleError(msg).InvalidateThis();
    }
  }
}

void NetFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const net::URLRequestStatus& status = source->GetStatus();
  const int response_code = source->GetResponseCode();
  if (status.is_success() && IsResponseCodeSuccess(response_code)) {
    handler()->OnDone(this);
  } else {
    std::string msg(
        base::StringPrintf("NetFetcher error on %s: %s, response code %d",
                           source->GetURL().spec().c_str(),
                           net::ErrorToString(status.error()), response_code));
    return HandleError(msg).InvalidateThis();
  }
}

bool NetFetcher::ShouldSendDownloadData() { return true; }

void NetFetcher::OnURLFetchDownloadData(const net::URLFetcher* source,
                                        scoped_ptr<std::string> download_data) {
  if (IsResponseCodeSuccess(source->GetResponseCode())) {
    handler()->OnReceived(this, download_data->data(), download_data->length());
  }
}

NetFetcher::~NetFetcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  start_callback_.Cancel();
}

NetFetcher::ReturnWrapper NetFetcher::HandleError(const std::string& message) {
  url_fetcher_.reset();
  handler()->OnError(this, message);
  return ReturnWrapper();
}

}  // namespace loader
}  // namespace cobalt
