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

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/browser/page_load_metrics_memory_tracker_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"

namespace page_load_metrics {

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<page_load_metrics::PageLoadMetricsMemoryTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PageLoadMetricsMemoryTrackerFactory*
PageLoadMetricsMemoryTrackerFactory::GetInstance() {
  return base::Singleton<PageLoadMetricsMemoryTrackerFactory>::get();
}

PageLoadMetricsMemoryTrackerFactory::PageLoadMetricsMemoryTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "PageLoadMetricsMemoryTracker",
          BrowserContextDependencyManager::GetInstance()) {}

bool PageLoadMetricsMemoryTrackerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring);
}

std::unique_ptr<KeyedService>
PageLoadMetricsMemoryTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<page_load_metrics::PageLoadMetricsMemoryTracker>();
}

content::BrowserContext*
PageLoadMetricsMemoryTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace page_load_metrics
