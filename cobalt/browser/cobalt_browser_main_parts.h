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

#ifndef COBALT_BROWSER_MAIN_PARTS_H_
#define COBALT_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "components/metrics_services_manager/metrics_services_manager.h"
// TODO(b/390021478): Remove this include when CobaltBrowserMainParts stops
// being a ShellBrowserMainParts.
#include "content/shell/browser/shell_browser_main_parts.h"

class PrefService;

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace cobalt {

class CobaltMetricsServiceClient;
class CobaltMetricsServicesManagerClient;

// TODO(b/390021478): When CobaltContentBrowserClient stops deriving from
// ShellContentBrowserClient, this should implement BrowserMainParts.
class CobaltBrowserMainParts : public content::ShellBrowserMainParts {
 public:
  CobaltBrowserMainParts();

  CobaltBrowserMainParts(const CobaltBrowserMainParts&) = delete;
  CobaltBrowserMainParts& operator=(const CobaltBrowserMainParts&) = delete;

  ~CobaltBrowserMainParts() override;

  // ShellBrowserMainParts overrides.
  int PreCreateThreads() override;
  int PreMainMessageLoopRun() override;

// TODO(cobalt, b/383301493): we should consider moving any ATV-specific
// behaviors into an ATV implementation of BrowserMainParts. For example, see
// Chrome's ChromeBrowserMainPartsAndroid.
#if BUILDFLAG(IS_ANDROIDTV)
  void PostCreateThreads() override;
#endif  // BUILDFLAG(IS_ANDROIDTV)

#if BUILDFLAG(IS_LINUX)
  void PostCreateMainMessageLoop() override;
#endif  // BUILDFLAG(IS_LINUX)

 private:
  // Initializes, but doesn't start, UMA.
  void SetupMetrics();

  // Starts metrics recording.
  void StartMetricsRecording();

  // Fetch and, if necessary, initializes MetricsService instance owned
  // by this class.
  metrics::MetricsService* GetMetricsService();

  // Fetch and, if necessary, initializes MetricsServicesManager owned by this
  // class instance.
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager();

  PrefService* local_state();

  // Must be destroyed before |local_state_|.
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  std::unique_ptr<PrefService> local_state_;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_MAIN_PARTS_H_
