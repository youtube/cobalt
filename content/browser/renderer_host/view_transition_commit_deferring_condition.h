// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_COMMIT_DEFERRING_CONDITION_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "content/common/content_export.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {
class NavigationRequest;

class CONTENT_EXPORT ViewTransitionCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request);

  ViewTransitionCommitDeferringCondition(
      const ViewTransitionCommitDeferringCondition&) = delete;
  ViewTransitionCommitDeferringCondition& operator=(
      const ViewTransitionCommitDeferringCondition&) = delete;

  ~ViewTransitionCommitDeferringCondition() override;

  Result WillCommitNavigation(base::OnceClosure resume) override;

 private:
  explicit ViewTransitionCommitDeferringCondition(
      NavigationRequest& navigation_request);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_COMMIT_DEFERRING_CONDITION_H_
