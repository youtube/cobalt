// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

class MockResourceFactory final : public NonTextResourceFactory {
 public:
  MockResourceFactory() : NonTextResourceFactory(ResourceType::kMock) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options) const override {
    return MakeGarbageCollected<MockResource>(request, options);
  }
};

}  // namespace

// static
MockResource* MockResource::Fetch(FetchParameters& params,
                                  ResourceFetcher* fetcher,
                                  ResourceClient* client) {
  params.SetRequestContext(mojom::blink::RequestContextType::SUBRESOURCE);
  return static_cast<MockResource*>(
      fetcher->RequestResource(params, MockResourceFactory(), client));
}

MockResource::MockResource(const KURL& url)
    : MockResource(ResourceRequest(url)) {}
MockResource::MockResource(const ResourceRequest& request)
    : MockResource(request, ResourceLoaderOptions(nullptr /* world */)) {}
MockResource::MockResource(const ResourceRequest& request,
                           const ResourceLoaderOptions& options)
    : Resource(request, ResourceType::kMock, options) {}

}  // namespace blink
