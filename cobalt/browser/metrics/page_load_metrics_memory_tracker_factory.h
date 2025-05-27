// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_METRICS_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_
#define COBALT_BROWSER_METRICS_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace page_load_metrics {

class PageLoadMetricsMemoryTracker;

class PageLoadMetricsMemoryTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static page_load_metrics::PageLoadMetricsMemoryTracker* GetForBrowserContext(
      content::BrowserContext* context);

  static PageLoadMetricsMemoryTrackerFactory* GetInstance();

  PageLoadMetricsMemoryTrackerFactory();

 private:
  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace page_load_metrics

#endif  // COBALT_BROWSER_METRICS_PAGE_LOAD_METRICS_MEMORY_TRACKER_FACTORY_H_
