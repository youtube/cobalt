// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_TEST_UTILS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_TEST_UTILS_H_

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace optimization_guide {

// Returns the first node in `root` and its sub-nodes (recursively in pre-order)
// that matches `attribute_type`. Returns nullptr if no match is found.
const proto::ContentNode* FindFirstNodeWithAttributeType(
    const proto::ContentNode& root,
    proto::ContentAttributeType attribute_type);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_TEST_UTILS_H_
