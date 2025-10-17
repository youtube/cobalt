// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "components/optimization_guide/content/mojom/ai_page_content_metadata.mojom.h"

namespace content {
class WebContents;
}

namespace optimization_guide {
blink::mojom::AIPageContentOptionsPtr DefaultAIPageContentOptions();

// A DocumentUserData that stores a serialized unguessable token for a given
// RenderFrameHost.
class DocumentIdentifierUserData
    : public content::DocumentUserData<DocumentIdentifierUserData> {
 public:
  explicit DocumentIdentifierUserData(content::RenderFrameHost* rfh)
      : DocumentUserData<DocumentIdentifierUserData>(rfh),
        serialized_token_(base::UnguessableToken::Create().ToString()) {}
  ~DocumentIdentifierUserData() override = default;

  std::string serialized_token() const { return serialized_token_; }

  static std::optional<std::string> GetDocumentIdentifier(
      content::GlobalRenderFrameHostToken token);

 private:
  std::string serialized_token_;

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

// The result of a call to GetAIPageContent.  It contains the
// AnnotatedPageContent proto and metadata about the page content.
struct AIPageContentResult {
  AIPageContentResult();
  AIPageContentResult(const AIPageContentResult& other) = delete;
  AIPageContentResult(AIPageContentResult&& other);
  AIPageContentResult& operator=(const AIPageContentResult& other) = delete;
  AIPageContentResult& operator=(AIPageContentResult&& other);
  ~AIPageContentResult();

  optimization_guide::proto::AnnotatedPageContent proto;
  optimization_guide::mojom::PageMetadataPtr metadata;
  // A map from a serialized unguessable token to the document pointer.
  // Callers should use this to map the frame identifiers in the proto to the
  // right frame host.
  base::flat_map<std::string, content::WeakDocumentPtr> document_identifiers;
};

// Provides AIPageContentResult (AnnotatedPageContent proto and metadata) for
// the primary page displayed in a WebContents.
using OnAIPageContentDone =
    base::OnceCallback<void(std::optional<AIPageContentResult>)>;
void GetAIPageContent(content::WebContents* web_contents,
                      blink::mojom::AIPageContentOptionsPtr options,
                      OnAIPageContentDone done_callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_PROVIDER_H_
