// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_PROPOSED_LAYOUT_H_
#define UI_VIEWS_LAYOUT_PROPOSED_LAYOUT_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Represents layout information for a child view within a host being laid
// out.
struct VIEWS_EXPORT ChildLayout {
  // Note that comparison ignores available size; as two layouts with the same
  // geometry are the same even if the available size is different.
  bool operator==(const ChildLayout& other) const;
  bool operator!=(const ChildLayout& other) const { return !(*this == other); }

  std::string ToString() const;

  raw_ptr<View, DanglingUntriaged> child_view = nullptr;
  bool visible = false;
  gfx::Rect bounds;
  SizeBounds available_size;
};

// Contains a full layout specification for the children of the host view.
struct VIEWS_EXPORT ProposedLayout {
  ProposedLayout();
  ~ProposedLayout();
  ProposedLayout(const ProposedLayout& other);
  ProposedLayout(ProposedLayout&& other);
  ProposedLayout(const gfx::Size& size,
                 const std::initializer_list<ChildLayout>& children);
  ProposedLayout& operator=(const ProposedLayout& other);
  ProposedLayout& operator=(ProposedLayout&& other);

  bool operator==(const ProposedLayout& other) const;
  bool operator!=(const ProposedLayout& other) const {
    return !(*this == other);
  }

  std::string ToString() const;

  // The size of the host view given the size bounds for this layout. If both
  // dimensions of the size bounds are specified, this will be the same size.
  gfx::Size host_size;

  // Contains an entry for each child view included in the layout.
  std::vector<ChildLayout> child_layouts;
};

// Returns a layout that's linearly interpolated between |start| and |target|
// by |value|, which should be between 0 and 1. See
// gfx::Tween::LinearIntValueBetween() for the exact math involved.
VIEWS_EXPORT ProposedLayout ProposedLayoutBetween(double value,
                                                  const ProposedLayout& start,
                                                  const ProposedLayout& target);

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_PROPOSED_LAYOUT_H_
