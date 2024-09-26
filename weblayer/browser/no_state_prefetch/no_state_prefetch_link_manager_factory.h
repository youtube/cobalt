// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_LINK_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_LINK_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class NoStatePrefetchLinkManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the prerender::NoStatePrefetchLinkManager for |context|.
  static prerender::NoStatePrefetchLinkManager* GetForBrowserContext(
      content::BrowserContext* context);

  static NoStatePrefetchLinkManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NoStatePrefetchLinkManagerFactory>;

  NoStatePrefetchLinkManagerFactory();
  ~NoStatePrefetchLinkManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_LINK_MANAGER_FACTORY_H_
