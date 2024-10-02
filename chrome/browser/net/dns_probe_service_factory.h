// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_DNS_PROBE_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace chrome_browser_net {

class DnsProbeService;

class DnsProbeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*(void)>;
  using DnsConfigChangeManagerGetter = base::RepeatingCallback<
      mojo::Remote<network::mojom::DnsConfigChangeManager>(void)>;

  // Returns the DnsProbeService that supports NetworkContexts for
  // |browser_context|.
  static DnsProbeService* GetForContext(
      content::BrowserContext* browser_context);

  // Returns the NetworkContextServiceFactory singleton.
  static DnsProbeServiceFactory* GetInstance();

  DnsProbeServiceFactory(const DnsProbeServiceFactory&) = delete;
  DnsProbeServiceFactory& operator=(const DnsProbeServiceFactory&) = delete;

  // Creates a DnsProbeService which will use the supplied
  // |network_context_getter| and |dns_config_change_manager_getter| instead of
  // getting them from a BrowserContext, and uses |tick_clock| for cache
  // expiration.
  static std::unique_ptr<DnsProbeService> CreateForTesting(
      const NetworkContextGetter& network_context_getter,
      const DnsConfigChangeManagerGetter& dns_config_change_manager_getter,
      const base::TickClock* tick_clock);

 private:
  friend struct base::DefaultSingletonTraits<DnsProbeServiceFactory>;

  DnsProbeServiceFactory();
  ~DnsProbeServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_SERVICE_FACTORY_H_
