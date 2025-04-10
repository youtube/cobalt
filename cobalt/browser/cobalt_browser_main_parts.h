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

#include "cobalt/browser/global_features.h"
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
class GlobalFeatures;

// TODO(b/390021478): When CobaltContentBrowserClient stops deriving from
// ShellContentBrowserClient, this should implement BrowserMainParts.
class CobaltBrowserMainParts : public content::ShellBrowserMainParts {
 public:
  CobaltBrowserMainParts();

  CobaltBrowserMainParts(const CobaltBrowserMainParts&) = delete;
  CobaltBrowserMainParts& operator=(const CobaltBrowserMainParts&) = delete;

  ~CobaltBrowserMainParts() override;

  // ShellBrowserMainParts overrides.
  int PreEarlyInitialization() override;
  int PreCreateThreads() override;
  int PreMainMessageLoopRun() override;

  void set_experiment_config(std::unique_ptr<PrefService> experiment_config);

  void set_local_state(std::unique_ptr<PrefService> local_state);

  // Sets |metrics_services_manager_| and |metrics_services_manager_client_|
  // which is owned by it.
  void SetMetricsServices(
      std::unique_ptr<metrics_services_manager::MetricsServicesManager> manager,
      metrics_services_manager::MetricsServicesManagerClient* client);

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

  std::unique_ptr<GlobalFeatures> global_features_;

  // Fetch and, if necessary, initializes MetricsService instance owned
  // by this class.
  metrics::MetricsService* GetMetricsService();

  // Fetch and, if necessary, initializes MetricsServicesManager owned by this
  // class instance.
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager();

  // This is owned by |metrics_services_manager_| but we need to expose it.
  raw_ptr<CobaltMetricsServicesManagerClient, DanglingUntriaged>
      metrics_services_manager_client_;
  // Must be destroyed before |local_state_|.
  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  // If TakePrefService() is called for the following two variables, the caller
  // will take the ownership of the the variable. Stop using the variable
  // afterwards.
  std::unique_ptr<PrefService> experiment_config_;
  std::unique_ptr<PrefService> local_state_;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_MAIN_PARTS_H_
