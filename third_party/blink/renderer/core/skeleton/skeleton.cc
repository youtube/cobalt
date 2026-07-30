// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/link_header.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void Skeleton::Render(KURL url, Document& owner_document) {
  ExecutionContext* context = owner_document.GetExecutionContext();
  skeleton_document_ = DocumentInit::Create()
                           .WithTypeFrom(keywords::kTextHtml)
                           .WithExecutionContext(context)
                           .WithAgent(*context->GetAgent())
                           .CreateDocument();
  skeleton_document_->setAllowDeclarativeShadowRoots(true);
  skeleton_document_->SetMimeType(keywords::kTextHtml);

  if (skeleton_url_.IsValid()) {
    GenerateSkeleton(skeleton_url_);
    observer_->DocumentReady(*this);
  }
}

void Skeleton::FetchSkeletonURL(KURL url, Document& owner_document) {
  ResourceRequest request(url);
  // TODO(crbug.com/513276602): The request parameters need to be tweaked
  request.SetHttpMethod(http_names::kHEAD);
  request.SetRequestContext(mojom::blink::RequestContextType::INTERNAL);
  request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);

  FetchParameters params(
      std::move(request),
      ResourceLoaderOptions(
          owner_document.GetExecutionContext()->GetCurrentWorld()));
  params.MutableOptions().initiator_info.name =
      fetch_initiator_type_names::kInternal;

  RawResource::Fetch(params, owner_document.Fetcher(), this);
}

void Skeleton::GenerateSkeleton(KURL url) {
  // Generate a placeholder skeleton document before we have resource loading
  // and parsing in place.
  StringBuilder builder;
  builder.Append(R"HTML(
    <html>
      <style>
        html {
          background: teal;
          color: white;
          overflow: hidden;
          position: fixed;
          inset: 0;
        }
        main { font-size: 32px; }
      </style>
      <body>
        <main>Skeleton for:
  )HTML");

  builder.Append(url.GetString());

  builder.Append(R"HTML(
        </main>
      </body>
    </html>
  )HTML");

  CHECK(skeleton_document_);
  skeleton_document_->SetContent(builder.ToString());
}

void Skeleton::ResponseReceived(Resource* resource,
                                const ResourceResponse& response) {
  LinkHeaderSet header_set(response.HttpHeaderField(http_names::kLink));
  for (const LinkHeader& header : header_set) {
    if (EqualIgnoringAsciiCase(header.Rel(), "skeleton")) {
      skeleton_url_ = KURL(header.Url());
      if (skeleton_url_.IsValid()) {
        // TODO(crbug.com/513276602): Fetch the skeleton_url_ instead
        if (skeleton_document_) {
          GenerateSkeleton(skeleton_url_);
          observer_->DocumentReady(*this);
        }
      }
      // TODO(crbug.com/513276602): Should we handle multiple links if the first
      // we encounter is invalid or fails to load?
      return;
    }
  }
}

void Skeleton::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(skeleton_document_);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
