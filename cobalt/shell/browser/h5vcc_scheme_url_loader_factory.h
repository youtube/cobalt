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

#ifndef COBALT_SHELL_BROWSER_H5VCC_SCHEME_URL_LOADER_FACTORY_H_
#define COBALT_SHELL_BROWSER_H5VCC_SCHEME_URL_LOADER_FACTORY_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "cobalt/shell/embedded_resources/embedded_resources.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// An enum for UMA histogram to indicate the state of retrieving splash screen.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SplashScreenFetchedState {
  kOkBuiltIn = 0,
  kOkCache = 1,
  kErrorOnCacheEmptyContent = 2,
  kErrorOnCacheFileOversize = 3,
  kErrorOnReadCache = 4,
  kErrorOnResourceNotFound = 5,
  // The built-in resource, without reading the cache, as
  // kEnableCachedSplashFile is disabled. Unlike kOkBuiltIn, this isn't a
  // cache miss.
  kSkippedCachedAssets = 6,
  kMaxValue = kSkippedCachedAssets,
};

// The outcome of splash.html's WebAssembly policy (kSplashWasmPolicy), which
// the page reports in the "policy" query param of its request for the asset
// that it shows. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. Keep in sync with
// SplashWasmPolicyOutcome in
// tools/metrics/histograms/metadata/cobalt/enums.xml.
enum class SplashWasmPolicyOutcome {
  // The policy chose a built-in asset ("builtin").
  kChoseBuiltIn = 0,
  // The policy chose an asset that the WebApp cached ("cached").
  kChoseCached = 1,
  // The page couldn't get the splash asset list, e.g., as the cached assets
  // couldn't all be listed in time, and made its own choice
  // ("list_unavailable").
  kListUnavailable = 2,
  // The policy failed, e.g., its module didn't load, or it made no or an
  // unexpected choice, and the page made its own choice ("failed").
  kFailed = 3,
  kMaxValue = kFailed,
};

// H5vccSchemeURLLoaderFactory is a URLLoaderFactory implementation that handles
// requests for the custom kH5vccEmbeddedScheme. It is designed to serve
// embedded resources, such as splash screen videos or test HTML pages, directly
// from memory.
//
// This factory creates H5vccSchemeURLLoader instances to handle individual
// requests. The resources are expected to be compiled into the binary using
// the "embed_loader_resources_as_header_files" action in the BUILD.gn file,
// making them available via the LoaderEmbeddedResources map.
//
// The loader resolves the requested resource based on the URL's path component
// (e.g., "h5vcc-embedded://my_resource.html" -> "my_resource.html").
class H5vccSchemeURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  explicit H5vccSchemeURLLoaderFactory(BrowserContext* browser_context);

  H5vccSchemeURLLoaderFactory(const H5vccSchemeURLLoaderFactory&) = delete;
  H5vccSchemeURLLoaderFactory& operator=(const H5vccSchemeURLLoaderFactory&) =
      delete;

  ~H5vccSchemeURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // Whether to read the splash assets that the WebApp cached:
  // kEnableCachedSplashFile, unless overridden for testing.
  static bool IsCachedSplashFileEnabled();

  // Returns the built-in resource `name`, e.g., "splash.webm", or an empty span
  // if there's none. The data has static storage duration.
  static base::span<const uint8_t> GetBuiltInResource(const std::string& name);

  // The origin whose Cache API holds the splash screen assets that the WebApp
  // cached, e.g., "https://www.youtube.com/".
  static std::string GetSplashDomain();

  // Testing seam to inject a resource map.
  static void SetResourceMapForTesting(
      const GeneratedResourceMap* resource_map_test);
  static void SetSplashDomainForTesting(
      const std::optional<std::string>& domain);
  static void SetSplashContentSizeForTesting(const std::optional<int>& size);
  // Overrides kEnableCachedSplashFile, as browser tests can't override
  // features once the browser starts. std::nullopt uses the feature.
  static void SetCachedSplashFileEnabledForTesting(
      const std::optional<bool>& enabled);

 private:
  GeneratedResourceMap resource_map_;
  static const GeneratedResourceMap* resource_map_test_;
  std::string splash_domain_;
  uint64_t splash_content_size_limit_;
  static std::optional<std::string> global_splash_domain_test_;
  static std::optional<int> global_splash_content_size_test_;
  static std::optional<bool> global_cached_splash_file_enabled_test_;
  BrowserContext* browser_context_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_H5VCC_SCHEME_URL_LOADER_FACTORY_H_
