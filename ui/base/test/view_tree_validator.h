// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_VIEW_TREE_VALIDATOR_H_
#define UI_BASE_TEST_VIEW_TREE_VALIDATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

@class NSView;

namespace ui {

struct ViewTreeProblemDetails {
  enum ProblemType {
    // |view_a| (the child view) is not entirely contained within the bounds of
    // |view_b| (the parent view).
    VIEW_OUTSIDE_PARENT,

    // |view_a| and |view_b|, neither of which is an ancestor of the other,
    // overlap each other, and at least one of |view_a| or |view_b| has
    // localizable text (is an NSControl or NSText).
    VIEWS_OVERLAP,
  };

  ProblemType type;
  NSView* view_a;
  NSView* view_b;

  std::string ToString();
};

// Validates the view tree rooted at |root|. If at least one problem is found,
// returns a |ViewTreeProblemDetails| as described above; if not, returns an
// empty option.
absl::optional<ViewTreeProblemDetails> ValidateViewTree(NSView* root);

}  // namespace ui

#endif  // UI_BASE_TEST_VIEW_TREE_VALIDATOR_H_
