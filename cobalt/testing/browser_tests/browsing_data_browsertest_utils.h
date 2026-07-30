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

#ifndef COBALT_TESTING_BROWSER_TESTS_BROWSING_DATA_BROWSERTEST_UTILS_H_
#define COBALT_TESTING_BROWSER_TESTS_BROWSING_DATA_BROWSERTEST_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class StoragePartition;

namespace browsing_data_browsertest_utils {

// TODO(msramek): A class like this already exists in ServiceWorkerBrowserTest.
// Consider extracting it to a different test utils file.
class ServiceWorkerActivationObserver
    : public ServiceWorkerContextCoreObserver {
 public:
  // |callback| is called when |context| is activated.
  static void SignalActivation(ServiceWorkerContextWrapper* context,
                               base::OnceClosure callback);

 private:
  ServiceWorkerActivationObserver(ServiceWorkerContextWrapper* context,
                                  base::OnceClosure callback);

  ~ServiceWorkerActivationObserver() override;

  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override;

  raw_ptr<ServiceWorkerContextWrapper> context_;
  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      scoped_observation_{this};
  base::OnceClosure callback_;
};

// Appends a switch to the |command_line| based on whether the network service
// is enabled. The browser will ignore certificate errors if the network service
// is not enabled.
void SetIgnoreCertificateErrors(base::CommandLine* command_line);

// Adds a service worker for the given |origin|. The EmbeddedTestServer
// |https_server| is required to retrieve a URL to the server based on the
// |origin|. Returns the scope url with port number.
GURL AddServiceWorker(const std::string& origin,
                      StoragePartition* storage_partition,
                      net::EmbeddedTestServer* https_server);

// Retrieves the list of all service workers.
std::vector<StorageUsageInfo> GetServiceWorkers(
    StoragePartition* storage_partition);

// Populates the content and content type fields of a given HTTP |response|
// based on the file extension of the |url| as follows:
//
// For .js:
// Example: "https://localhost/?file=file.js"
// will set the response header as
// Content-Type: application/javascript
//
// For .html:
// Example: "https://localhost/?file=file.html"
// will set the response header as
// Content-Type: text/html
//
// Response content type is only set for .js and .html files.
void SetResponseContent(const GURL& url,
                        std::string* value,
                        net::test_server::BasicHttpResponse* response);

// Sets up a MockCertVerifier with default return value |default_result|.
void SetUpMockCertVerifier(int32_t default_result);

}  // namespace browsing_data_browsertest_utils

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_BROWSING_DATA_BROWSERTEST_UTILS_H_
