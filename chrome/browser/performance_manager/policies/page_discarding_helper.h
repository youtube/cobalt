// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/cannot_discard_reason.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "components/memory_pressure/reclaim_target.h"
#include "components/memory_pressure/unnecessary_discard_monitor.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace url_matcher {
class URLMatcher;
}  // namespace url_matcher

namespace performance_manager {

namespace mechanism {
class PageDiscarder;
}  // namespace mechanism

namespace policies {

#if BUILDFLAG(IS_CHROMEOS)
constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::TimeDelta();
#else
// Time during which non visible pages are protected from urgent discarding
// (not on ChromeOS).
constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::Minutes(10);
#endif

// Time during which a tab cannot be discarded after having played audio.
constexpr base::TimeDelta kTabAudioProtectionTime = base::Minutes(1);

// Whether a page can be discarded.
enum class CanDiscardResult {
  // The page can be discarded. The user should experience minimal disruption
  // from discarding.
  kEligible,
  // The page can be discarded. The user will likely find discarding disruptive.
  kProtected,
  // The page cannot be discarded.
  kDisallowed,
};

// Caches page node properties to facilitate sorting.
class PageNodeSortProxy {
 public:
  PageNodeSortProxy(const PageNode* page_node,
                    CanDiscardResult can_discard_result,
                    bool is_visible,
                    bool is_focused,
                    base::TimeDelta last_visible)
      : page_node_(page_node),
        can_discard_result_(can_discard_result),
        is_visible_(is_visible),
        is_focused_(is_focused),
        last_visible_(last_visible) {}

  const PageNode* page_node() const { return page_node_; }
  bool is_disallowed() const {
    return can_discard_result_ == CanDiscardResult::kDisallowed;
  }
  bool is_protected() const {
    return can_discard_result_ == CanDiscardResult::kProtected;
  }
  bool is_visible() const { return is_visible_; }
  bool is_focused() const { return is_focused_; }
  base::TimeDelta last_visible() const { return last_visible_; }

  // Returns true if the rhs is more important.
  bool operator<(const PageNodeSortProxy& rhs) const {
    if (is_disallowed() != rhs.is_disallowed()) {
      return rhs.is_disallowed();
    }
    if (is_focused_ != rhs.is_focused_) {
      return rhs.is_focused_;
    }
    if (is_visible_ != rhs.is_visible_) {
      return rhs.is_visible_;
    }
    if (is_protected() != rhs.is_protected()) {
      return rhs.is_protected();
    }
    return last_visible_ > rhs.last_visible_;
  }

 private:
  raw_ptr<const PageNode> page_node_;
  CanDiscardResult can_discard_result_;
  bool is_visible_;
  bool is_focused_;
  // Delta between current time and last visibility change time.
  base::TimeDelta last_visible_;
};

// Helper class to be used by the policies that want to discard tabs.
//
// This is a GraphRegistered object and should be accessed via
// PageDiscardingHelper::GetFromGraph(graph()).
class PageDiscardingHelper
    : public GraphOwnedAndRegistered<PageDiscardingHelper>,
      public NodeDataDescriberDefaultImpl,
      public PageNodeObserver {
 public:
  // Export discard reason in the public interface.
  using DiscardReason = ::mojom::LifecycleUnitDiscardReason;
  // DiscardCallback passes the time of first discarding is done.
  // If discarding fails or there is no candidate for discarding, this passes
  // nullopt.
  using DiscardCallback =
      base::OnceCallback<void(std::optional<base::TimeTicks>)>;

  PageDiscardingHelper();
  ~PageDiscardingHelper() override;
  PageDiscardingHelper(const PageDiscardingHelper& other) = delete;
  PageDiscardingHelper& operator=(const PageDiscardingHelper&) = delete;

  base::WeakPtr<PageDiscardingHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Selects and discards a tab. This will try to discard a tab until there's
  // been a successful discard or until there's no more discard candidate.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see the comment
  // there about its usage.
  std::optional<base::TimeTicks> DiscardAPage(
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime);

  // Selects and discards multiple tabs to meet the reclaim target. This will
  // keep trying again until there's been at least a single successful discard
  // or until there's no more discard candidate. If |reclaim_target_kb| is
  // nullopt, only discard one tab. If |discard_protected_tabs| is true,
  // protected tabs (CanDiscard() returns kProtected) can also be discarded.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see the comment
  // there about its usage.
  std::optional<base::TimeTicks> DiscardMultiplePages(
      std::optional<memory_pressure::ReclaimTarget> reclaim_target,
      bool discard_protected_tabs,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime);

  // Immediately discards as many pages as possible in `page_nodes`. Does not
  // check for a minimum time in the background.
  std::optional<base::TimeTicks> ImmediatelyDiscardMultiplePages(
      const std::vector<const PageNode*>& page_nodes,
      DiscardReason discard_reason);

  // Immediately discards as many pages as possible in `page_nodes`.
  // `minimum_time_in_background` is passed to `CanDiscard()`, see the comment
  // there about its usage.
  std::optional<base::TimeTicks> ImmediatelyDiscardMultiplePages(
      const std::vector<const PageNode*>& page_nodes,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background);

  void SetNoDiscardPatternsForProfile(const std::string& browser_context_id,
                                      const std::vector<std::string>& patterns);
  void ClearNoDiscardPatternsForProfile(const std::string& browser_context_id);

  void SetMockDiscarderForTesting(
      std::unique_ptr<mechanism::PageDiscarder> discarder);

  // Indicates if `page_node` can be urgently discarded, using a list of
  // criteria depending on `discard_reason`. If `minimum_time_in_background` is
  // non-zero, the page will not be discarded if it has not spent at least
  // `minimum_time_in_background` in the not-visible state.
  CanDiscardResult CanDiscard(
      const PageNode* page_node,
      DiscardReason discard_reason,
      base::TimeDelta minimum_time_in_background =
          kNonVisiblePagesUrgentProtectionTime,
      std::vector<CannotDiscardReason>* cannot_discard_reasons = nullptr) const;

  static void AddDiscardAttemptMarkerForTesting(PageNode* page_node);
  static void RemovesDiscardAttemptMarkerForTesting(PageNode* page_node);

  // Sets an additional callback that should be invoked whenever the
  // SetNoDiscardPatternsForProfile() or ClearNoDiscardPatternsForProfile()
  // methosd is called, with the method's `browser_context_id` argument.
  void SetOptOutPolicyChangedCallback(
      base::RepeatingCallback<void(std::string_view)> callback);

  bool IsPageOptedOutOfDiscarding(const std::string& browser_context_id,
                                  const GURL& url) const;

  // PageNodeObserver:
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;

 protected:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Returns the PageLiveStateDecorator::Data associated with a PageNode.
  // Exposed and made virtual to allowed injecting some fake data in tests.
  virtual const PageLiveStateDecorator::Data* GetPageNodeLiveStateData(
      const PageNode* page_node) const;

 private:
  // NodeDataDescriber implementation:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // The mechanism used to do the actual discarding.
  std::unique_ptr<mechanism::PageDiscarder> page_discarder_;

  std::map<std::string, std::unique_ptr<url_matcher::URLMatcher>>
      profiles_no_discard_patterns_ GUARDED_BY_CONTEXT(sequence_checker_);

  memory_pressure::UnnecessaryDiscardMonitor unnecessary_discard_monitor_;

  base::RepeatingCallback<void(std::string_view)>
      opt_out_policy_changed_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PageDiscardingHelper> weak_factory_{this};
};

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PAGE_DISCARDING_HELPER_H_
