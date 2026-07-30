// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_IMAGE_EXTRACTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_IMAGE_EXTRACTOR_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {

// Gets the raw encoded image bytes (e.g., JPEG, PNG) for a specific image node.
void GetImageBytes(
    content::WebContents* web_contents,
    const std::string& document_identifier,
    int32_t dom_node_id,
    base::OnceCallback<
        void(blink::mojom::AIPageContentImageBytesResultPtr result)> callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_IMAGE_EXTRACTOR_H_
