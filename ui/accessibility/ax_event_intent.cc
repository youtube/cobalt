// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_intent.h"

#include "ui/accessibility/ax_enum_util.h"

namespace ui {

AXEventIntent::AXEventIntent() = default;

AXEventIntent::AXEventIntent(ax::mojom::Command command) : command(command) {}

AXEventIntent::AXEventIntent(ax::mojom::Command command,
                             ax::mojom::InputEventType input_event_type)
    : command(command), input_event_type(input_event_type) {}

AXEventIntent::AXEventIntent(ax::mojom::Command command,
                             ax::mojom::TextBoundary text_boundary,
                             ax::mojom::MoveDirection move_direction)
    : command(command),
      text_boundary(text_boundary),
      move_direction(move_direction) {}

AXEventIntent::~AXEventIntent() = default;

AXEventIntent::AXEventIntent(const AXEventIntent& intent) = default;

AXEventIntent& AXEventIntent::operator=(const AXEventIntent& intent) = default;

bool operator==(const AXEventIntent& a, const AXEventIntent& b) {
  return a.command == b.command && a.input_event_type == b.input_event_type &&
         a.text_boundary == b.text_boundary &&
         a.move_direction == b.move_direction;
}

bool operator!=(const AXEventIntent& a, const AXEventIntent& b) {
  return !(a == b);
}

std::string AXEventIntent::ToString() const {
  return std::string("AXEventIntent(") + ui::ToString(command) + ',' +
         ui::ToString(input_event_type) + ',' + ui::ToString(text_boundary) +
         ',' + ui::ToString(move_direction) + ')';
}

}  // namespace ui
