// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"

#include "base/containers/contains.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/features.h"

namespace performance_manager::policies {

namespace {

HighEfficiencyModePolicy* g_high_efficiency_mode_policy = nullptr;

}

const base::TimeDelta HighEfficiencyModePolicy::kDefaultDiscardTimeInterval =
    base::Hours(2);

HighEfficiencyModePolicy::HighEfficiencyModePolicy()
    : time_before_discard_(kDefaultDiscardTimeInterval) {
  DCHECK(!g_high_efficiency_mode_policy);
  g_high_efficiency_mode_policy = this;
}

HighEfficiencyModePolicy::~HighEfficiencyModePolicy() {
  DCHECK_EQ(this, g_high_efficiency_mode_policy);
  g_high_efficiency_mode_policy = nullptr;
}

// static
HighEfficiencyModePolicy* HighEfficiencyModePolicy::GetInstance() {
  return g_high_efficiency_mode_policy;
}

void HighEfficiencyModePolicy::OnPageNodeAdded(const PageNode* page_node) {
  if (page_node->GetType() == PageType::kTab && !page_node->IsVisible()) {
    // Some mechanisms (like "session restore" and "open all bookmarks") can
    // create pages that are non-visible. If that happens, start a discard timer
    // so that the pages are discarded if they don't ever become visible.
    // TODO(anthonyvd): High Efficiency Mode should make it so non-visible pages
    // are simply not loaded until they become visible.
    StartDiscardTimerIfEnabled(page_node, time_before_discard_);
  }
}

void HighEfficiencyModePolicy::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  if (page_node->GetType() != PageType::kTab) {
    DCHECK(!base::Contains(active_discard_timers_, page_node));
    return;
  }

  RemoveActiveTimer(page_node);
}

void HighEfficiencyModePolicy::OnIsVisibleChanged(const PageNode* page_node) {
  if (page_node->GetType() != PageType::kTab)
    return;

  // If the page is made visible, any existing timers that refer to it should be
  // cancelled. `RemoveActiveTimer` handles the case where no timer exists
  // gracefully.
  if (page_node->IsVisible()) {
    RemoveActiveTimer(page_node);
  } else {
    StartDiscardTimerIfEnabled(page_node, time_before_discard_);
  }
}

void HighEfficiencyModePolicy::OnTypeChanged(const PageNode* page_node,
                                             PageType previous_type) {
  if (page_node->GetType() != PageType::kTab) {
    RemoveActiveTimer(page_node);
  } else if (!page_node->IsVisible()) {
    // If the page is a tab now, it wasn't before so it doesn't yet have a timer
    // running. Add it to the timer map if it's not visible, otherwise it will
    // be added as needed when its OnVisibleChange event fires.
    StartDiscardTimerIfEnabled(page_node, time_before_discard_);
  }
}

void HighEfficiencyModePolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  graph->AddPageNodeObserver(this);
}

void HighEfficiencyModePolicy::OnTakenFromGraph(Graph* graph) {
  // The logic in this class depends on being notified of pages being removed,
  // otherwise there's no guarantee PageNode pointers are still valid when
  // timers fire. To avoid possibly having callbacks manipulate invalid PageNode
  // pointers, clear all the existing timers before unregistering the observer.
  active_discard_timers_.clear();
  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void HighEfficiencyModePolicy::OnHighEfficiencyModeChanged(bool enabled) {
  high_efficiency_mode_enabled_ = enabled;

  if (high_efficiency_mode_enabled_) {
    DCHECK(active_discard_timers_.empty());
    for (const PageNode* page_node : graph_->GetAllPageNodes()) {
      if (page_node->GetType() == PageType::kTab && !page_node->IsVisible()) {
        base::TimeDelta time_before_discard =
            time_before_discard_ -
            page_node->GetTimeSinceLastVisibilityChange();
        StartDiscardTimerIfEnabled(page_node, time_before_discard.is_negative()
                                                  ? base::Microseconds(0)
                                                  : time_before_discard);
      }
    }
  } else {
    active_discard_timers_.clear();
  }
}

bool HighEfficiencyModePolicy::IsHighEfficiencyDiscardingEnabled() const {
  return high_efficiency_mode_enabled_;
}

void HighEfficiencyModePolicy::StartDiscardTimerIfEnabled(
    const PageNode* page_node,
    base::TimeDelta time_before_discard) {
  DCHECK_EQ(page_node->GetType(), PageType::kTab);
  if (IsHighEfficiencyDiscardingEnabled()) {
    // High Efficiency mode is enabled, so the tab should be discarded after the
    // amount of time specified by finch is elapsed.
    active_discard_timers_[page_node].Start(
        FROM_HERE, time_before_discard,
        base::BindOnce(&HighEfficiencyModePolicy::DiscardPageTimerCallback,
                       base::Unretained(this), page_node));
  }
}

void HighEfficiencyModePolicy::RemoveActiveTimer(const PageNode* page_node) {
  // If there's a discard timer already running for this page, erase it from the
  // map which will stop the timer when it is destroyed.
  active_discard_timers_.erase(page_node);
}

void HighEfficiencyModePolicy::DiscardPageTimerCallback(
    const PageNode* page_node) {
  // When this callback is invoked, the `page_node` is guaranteed to still be
  // valid otherwise `OnBeforePageNodeRemoved` would've been called and the
  // timer destroyed.
  RemoveActiveTimer(page_node);

  // Turning off High Efficiency Mode would delete the timer, so it's not
  // possible to get here and for High Efficiency Mode to be off.
  DCHECK(IsHighEfficiencyDiscardingEnabled());

  PageDiscardingHelper::GetFromGraph(graph_)->ImmediatelyDiscardSpecificPage(
      page_node, PageDiscardingHelper::DiscardReason::PROACTIVE);
}

}  // namespace performance_manager::policies
