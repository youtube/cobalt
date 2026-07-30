// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_COBALT_H_
#define COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_COBALT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/update_client/network.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace update_client {

// NetworkFetcher factory for Cobalt Evergreen builds. IN_MEMORY_UPDATES
// (defined by the Evergreen platform configuration) replaces
// NetworkFetcher::DownloadToFile with an in-memory DownloadToString that the
// Chromium NetworkFetcherImpl does not implement, so network_impl cannot be
// built for Evergreen. The fetchers created here implement the full Evergreen
// interface on top of network::SimpleURLLoader.
class NetworkFetcherCobaltFactory : public NetworkFetcherFactory {
 public:
  explicit NetworkFetcherCobaltFactory(
      scoped_refptr<network::SharedURLLoaderFactory>
          shared_url_network_factory);

  NetworkFetcherCobaltFactory(const NetworkFetcherCobaltFactory&) = delete;
  NetworkFetcherCobaltFactory& operator=(const NetworkFetcherCobaltFactory&) =
      delete;

  std::unique_ptr<NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherCobaltFactory() override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_COBALT_H_
