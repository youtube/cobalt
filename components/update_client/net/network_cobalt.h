// Copyright 2019 The Cobalt Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NET_NETWORK_COBALT_H_
#define COMPONENTS_UPDATE_CLIENT_NET_NETWORK_COBALT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cobalt/network/network_module.h"
#include "components/update_client/network.h"

namespace update_client {

class NetworkFetcherCobaltFactory : public NetworkFetcherFactory {
 public:
  explicit NetworkFetcherCobaltFactory(
      cobalt::network::NetworkModule* network_module);

  std::unique_ptr<NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherCobaltFactory() override;

 private:
  cobalt::network::NetworkModule* network_module_;
  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherCobaltFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_NETWORK_COBALT_H_
