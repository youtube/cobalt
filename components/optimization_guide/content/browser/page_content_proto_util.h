// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "url/origin.h"

namespace optimization_guide {

struct RenderFrameInfo {
  content::GlobalRenderFrameHostToken global_frame_token;
  url::Origin source_origin;
};

// Converts the mojom data structure for AIPageContent to its equivalent proto
// mapping.
// Returns false if the conversion failed because the renderer provided invalid
// inputs.
using AIPageContentMap = base::flat_map<content::GlobalRenderFrameHostToken,
                                        blink::mojom::AIPageContentPtr>;
using GetRenderFrameInfo =
    base::RepeatingCallback<std::optional<RenderFrameInfo>(int child_process_id,
                                                           blink::FrameToken)>;
bool ConvertAIPageContentToProto(
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    GetRenderFrameInfo get_render_frame_info,
    optimization_guide::proto::AnnotatedPageContent* proto);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
