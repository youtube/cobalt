// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_test_utils.h"

#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace optimization_guide {

const proto::ContentNode* FindFirstNodeWithAttributeType(
    const proto::ContentNode& root,
    proto::ContentAttributeType attribute_type) {
  std::vector<const proto::ContentNode*> nodes_to_visit;
  nodes_to_visit.push_back(&root);
  while (!nodes_to_visit.empty()) {
    const auto* current = nodes_to_visit.back();
    nodes_to_visit.pop_back();
    if (current->content_attributes().attribute_type() == attribute_type) {
      return current;
    }
    for (const auto& child : current->children_nodes()) {
      nodes_to_visit.push_back(&child);
    }
  }
  return nullptr;
}

}  // namespace optimization_guide
