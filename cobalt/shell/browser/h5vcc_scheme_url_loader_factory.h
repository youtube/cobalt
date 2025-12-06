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

#include "cobalt/shell/embedded_resources/embedded_resources.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

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
  H5vccSchemeURLLoaderFactory();

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

  // Testing seam to inject a resource map.
  void SetResourceMapForTesting(const GeneratedResourceMap* resource_map_test);

 private:
  const GeneratedResourceMap* resource_map_test_ = nullptr;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_H5VCC_SCHEME_URL_LOADER_FACTORY_H_
