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

#include "cobalt/browser/metrics/page_load_metrics_initialize.h"

#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace cobalt {

namespace {

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

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<page_load_metrics::PageLoadMetricsMemoryTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
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

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents);

  PageLoadMetricsEmbedder(const PageLoadMetricsEmbedder&) = delete;
  PageLoadMetricsEmbedder& operator=(const PageLoadMetricsEmbedder&) = delete;

  ~PageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override { return false; }
  bool IsNoStatePrefetch(content::WebContents* web_contents) override {
    return false;
  }
  bool IsExtensionUrl(const GURL& url) override { return false; }
  bool IsSidePanel(content::WebContents* web_contents) { return false; }
  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override;

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterEmbedderObservers(
      page_load_metrics::PageLoadTracker* tracker) override;
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : PageLoadMetricsEmbedderBase(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() = default;

void PageLoadMetricsEmbedder::RegisterEmbedderObservers(
    page_load_metrics::PageLoadTracker* tracker) {
  // Intentionally not implemented, since useful observers are already
  // registered in the parent class.
  NOTIMPLEMENTED();
}

page_load_metrics::PageLoadMetricsMemoryTracker*
PageLoadMetricsEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  return PageLoadMetricsMemoryTrackerFactory::GetForBrowserContext(
      browser_context);
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, std::make_unique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace cobalt
