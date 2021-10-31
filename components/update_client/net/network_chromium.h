// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NET_NETWORK_CHROMIUM_H_
#define COMPONENTS_UPDATE_CLIENT_NET_NETWORK_CHROMIUM_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/network.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace update_client {

class NetworkFetcherChromiumFactory : public NetworkFetcherFactory {
 public:
  explicit NetworkFetcherChromiumFactory(
      scoped_refptr<network::SharedURLLoaderFactory>
          shared_url_network_factory);

  std::unique_ptr<NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherChromiumFactory() override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherChromiumFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_NETWORK_CHROMIUM_H_
