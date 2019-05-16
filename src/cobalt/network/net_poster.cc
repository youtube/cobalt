// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/net_poster.h"

#include <algorithm>
#include <memory>

#include "cobalt/network/network_module.h"
#include "net/base/net_errors.h"

namespace cobalt {
namespace network {

NetPoster::NetPoster(NetworkModule* network_module)
    : network_module_(network_module) {}

NetPoster::~NetPoster() {}

void NetPoster::Send(const GURL& url, const std::string& content_type,
                     const std::string& data) {
  if (network_module_->task_runner() !=
      base::MessageLoop::current()->task_runner()) {
    network_module_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&NetPoster::Send, base::Unretained(this), url,
                              content_type, data));
    return;
  }

  std::unique_ptr<net::URLFetcher> url_fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::POST, this);

  // In general it doesn't make sense to follow redirects for POST requests.
  // And for CSP reporting we are required not to follow them.
  // https://www.w3.org/TR/CSP2/#send-violation-reports
  url_fetcher->SetStopOnRedirect(true);
  url_fetcher->SetRequestContext(
      network_module_->url_request_context_getter().get());

  if (data.size()) {
    url_fetcher->SetUploadData(content_type, data);
  }
  url_fetcher->Start();
  fetchers_.push_back(std::move(url_fetcher));
}

void NetPoster::OnURLFetchComplete(const net::URLFetcher* source) {
  // Make sure the thread that created the fetcher is the same one that deletes
  // it. Otherwise we have unsafe access to the fetchers_ list.
  DCHECK_EQ(base::MessageLoop::current()->task_runner(),
            network_module_->task_runner());
  net::URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    DLOG(WARNING) << "NetPoster failed to POST to " << source->GetURL()
                  << " with error " << net::ErrorToString(status.error());
  }
  std::vector<std::unique_ptr<net::URLFetcher>>::iterator it =
      std::find_if(fetchers_.begin(), fetchers_.end(),
                   [&](const std::unique_ptr<net::URLFetcher>& i) {
                     return source == i.get();
                   });
  if (it != fetchers_.end()) {
    fetchers_.erase(it);
  } else {
    NOTREACHED() << "Expected to find net::URLFetcher";
  }
}

}  // namespace network
}  // namespace cobalt
