// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TARGET_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TARGET_UTIL_H_

#include <optional>
#include <string_view>

#include "components/actor/core/shared_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace actor {

// Returns the `RenderFrameHost` for a `PageTarget`.
content::RenderFrameHost* FindTargetLocalRootFrame(tabs::TabHandle tab_handle,
                                                   PageTarget target);

// Return `TargetNodeInfo` from hit test against the last observed APC. Returns
// std::nullopt if Target does not hit any node.
//
// IMPORTANT: `target` must be provided in view-relative DIPs. This function
// will handle scaling to the appropriate coordinate space for APC lookup.
std::optional<optimization_guide::TargetNodeInfo>
FindLastObservedNodeForActionTarget(
    const optimization_guide::proto::AnnotatedPageContent* apc,
    const PageTarget& target,
    tabs::TabInterface* tab);

// Returns the `autofill::FieldGlobalId` for a `PageTarget` given the last
// observed APC and the tab.
//
// IMPORTANT: `target` must be provided in view-relative DIPs.
autofill::FieldGlobalId GetFieldIdFromPageTarget(
    const optimization_guide::proto::AnnotatedPageContent* last_observation,
    tabs::TabInterface* tab,
    const PageTarget& target);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TARGET_UTIL_H_
