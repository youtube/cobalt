// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_event_mojom_traits.h"

#include <vector>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/mojom/ax_event.mojom.h"
#include "ui/accessibility/mojom/ax_event_intent.mojom.h"

using mojo::test::SerializeAndDeserialize;

TEST(AXEventMojomTraitsTest, RoundTrip) {
  ui::AXEvent input;
  input.event_type = ax::mojom::Event::kTextChanged;
  input.id = 111;
  input.event_from = ax::mojom::EventFrom::kUser;
  input.event_from_action = ax::mojom::Action::kDoDefault;
  ui::AXEventIntent editing_intent;
  editing_intent.command = ax::mojom::Command::kDelete;
  editing_intent.input_event_type =
      ax::mojom::InputEventType::kDeleteWordForward;
  ui::AXEventIntent selection_intent;
  selection_intent.command = ax::mojom::Command::kMoveSelection;
  selection_intent.text_boundary = ax::mojom::TextBoundary::kWordStart;
  selection_intent.move_direction = ax::mojom::MoveDirection::kForward;
  const std::vector<ui::AXEventIntent> event_intents{editing_intent,
                                                     selection_intent};
  input.event_intents = event_intents;
  input.action_request_id = 222;

  ui::AXEvent output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXEvent>(input, output));
  EXPECT_EQ(ax::mojom::Event::kTextChanged, output.event_type);
  EXPECT_EQ(111, output.id);
  EXPECT_EQ(ax::mojom::EventFrom::kUser, output.event_from);
  EXPECT_EQ(ax::mojom::Action::kDoDefault, output.event_from_action);
  EXPECT_THAT(output.event_intents, testing::ContainerEq(event_intents));
  EXPECT_EQ(222, output.action_request_id);
}
