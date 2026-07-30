// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include <memory>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/common/actor.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(ClickToolRequestTest,
     BuildToolRequest_MapsLeftOnOccludedTargetToMojomClickType) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ClickAction* click =
      actions.add_actions()->mutable_click();
  click->set_tab_id(123);
  click->set_click_type(
      optimization_guide::proto::ClickAction_ClickType_LEFT_ON_OCCLUDED_TARGET);
  click->set_click_count(
      optimization_guide::proto::ClickAction_ClickCount_SINGLE);
  click->mutable_target()->set_content_node_id(456);
  click->mutable_target()->mutable_document_identifier()->set_serialized_token(
      "doc_id");

  ASSERT_OK_AND_ASSIGN(std::vector<std::unique_ptr<ToolRequest>> requests,
                       BuildToolRequest(actions));
  ASSERT_EQ(1u, requests.size());

  // This covers actor_proto_conversion.cc's explicit mapping for the new
  // click-behind type. The other proto fields above only make the request
  // valid enough to reach the click-type conversion branch.
  ClickToolRequest& request = static_cast<ClickToolRequest&>(*requests.front());
  EXPECT_EQ(mojom::ClickType::kLeftOnOccludedTarget, request.GetClickType());
}

}  // namespace
}  // namespace actor
