// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

using performance_manager::mechanism::PageDiscarder;

namespace performance_manager {
namespace policies {
namespace {

BASE_FEATURE(kIgnoreDiscardAttemptMarker,
             "IgnoreDiscardAttemptMarker",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipDiscardsDrivenByStaleSignal,
             "SkipDiscardDrivenByStaleSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// NodeAttachedData used to indicate that there's already been an attempt to
// discard a PageNode.
class DiscardAttemptMarker
    : public ExternalNodeAttachedDataImpl<DiscardAttemptMarker> {
 public:
  explicit DiscardAttemptMarker(const PageNodeImpl* page_node) {}
  ~DiscardAttemptMarker() override = default;
};

const char kDescriberName[] = "PageDiscardingHelper";

#if BUILDFLAG(IS_CHROMEOS)
// A 25% compression ratio is very conservative, and it matches the
// value used by resourced when calculating available memory.
static const uint64_t kSwapFootprintDiscount = 4;
#endif

using NodeFootprintMap = base::flat_map<const PageNode*, uint64_t>;

// Returns the mapping from page_node to its memory footprint estimation.
NodeFootprintMap GetPageNodeFootprintEstimateKb(
    const std::vector<PageNodeSortProxy>& candidates) {
  // Initialize the result map in one shot for time complexity O(n * log(n)).
  NodeFootprintMap::container_type result_container;
  result_container.reserve(candidates.size());
  for (auto candidate : candidates) {
    result_container.emplace_back(candidate.page_node(), 0);
  }
  NodeFootprintMap result(std::move(result_container));

  // TODO(crbug.com/40194476): Use visitor to accumulate the result to avoid
  // allocating extra lists of frame nodes behind the scenes.

  // List all the processes associated with these page nodes.
  base::flat_set<const ProcessNode*> process_nodes;
  for (auto candidate : candidates) {
    base::flat_set<const ProcessNode*> processes =
        GraphOperations::GetAssociatedProcessNodes(candidate.page_node());
    process_nodes.insert(processes.begin(), processes.end());
  }

  // Compute the resident set of each page by simply summing up the estimated
  // resident set of all its frames.
  for (const ProcessNode* process_node : process_nodes) {
    ProcessNode::NodeSetView<const FrameNode*> process_frames =
        process_node->GetFrameNodes();
    if (!process_frames.size()) {
      continue;
    }
    // Get the footprint of the process and split it equally across its
    // frames.
    uint64_t footprint_kb = process_node->GetResidentSetKb();
#if BUILDFLAG(IS_CHROMEOS)
    footprint_kb += process_node->GetPrivateSwapKb() / kSwapFootprintDiscount;
#endif
    footprint_kb /= process_frames.size();
    for (const FrameNode* frame_node : process_frames) {
      // Check if the frame belongs to a discardable page, if so update the
      // resident set of the page.
      auto iter = result.find(frame_node->GetPageNode());
      if (iter == result.end()) {
        continue;
      }
      iter->second += footprint_kb;
    }
  }
  return result;
}

void RecordDiscardedTabMetrics(const PageNodeSortProxy& candidate) {
  // Logs a histogram entry to track the proportion of discarded tabs that
  // were protected at the time of discard.
  UMA_HISTOGRAM_BOOLEAN("Discarding.DiscardingProtectedTab",
                        candidate.is_protected());

  // Logs a histogram entry to track the proportion of discarded tabs that
  // were focused at the time of discard.
  UMA_HISTOGRAM_BOOLEAN("Discarding.DiscardingFocusedTab",
                        candidate.is_focused());
}

}  // namespace

PageDiscardingHelper::PageDiscardingHelper()
    : page_discarder_(std::make_unique<PageDiscarder>()) {}
PageDiscardingHelper::~PageDiscardingHelper() = default;

std::optional<base::TimeTicks> PageDiscardingHelper::DiscardAPage(
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  return DiscardMultiplePages(std::nullopt, false, discard_reason,
                              minimum_time_in_background);
}

std::optional<base::TimeTicks> PageDiscardingHelper::DiscardMultiplePages(
    std::optional<memory_pressure::ReclaimTarget> reclaim_target,
    bool discard_protected_tabs,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reclaim_target) {
    if (base::FeatureList::IsEnabled(kSkipDiscardsDrivenByStaleSignal)) {
      reclaim_target =
          unnecessary_discard_monitor_.CorrectReclaimTarget(*reclaim_target);
    }

    unnecessary_discard_monitor_.OnReclaimTargetBegin(*reclaim_target);
  }

  LOG(WARNING) << "Discarding multiple pages with target (kb): "
               << (reclaim_target ? reclaim_target->target_kb : 0)
               << ", discard_protected_tabs: " << discard_protected_tabs;

  std::vector<PageNodeSortProxy> candidates;
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    CanDiscardResult can_discard_result =
        CanDiscard(page_node, discard_reason, minimum_time_in_background);
    if (can_discard_result == CanDiscardResult::kDisallowed) {
      continue;
    }
    if (can_discard_result == CanDiscardResult::kProtected &&
        !discard_protected_tabs) {
      continue;
    }
    candidates.emplace_back(page_node, can_discard_result,
                            page_node->IsVisible(), page_node->IsFocused(),
                            page_node->GetTimeSinceLastVisibilityChange());
  }

  // Sorts with ascending importance.
  std::sort(candidates.begin(), candidates.end());

  UMA_HISTOGRAM_COUNTS_100("Discarding.DiscardCandidatesCount",
                           candidates.size());

  // Returns early when candidate is empty to avoid infinite loop in
  // DiscardMultiplePages.
  if (candidates.empty()) {
    return std::nullopt;
  }
  std::vector<const PageNode*> discard_attempts;

  if (!reclaim_target) {
    const PageNode* oldest = candidates[0].page_node();
    discard_attempts.emplace_back(oldest);

    // Record metrics about the tab that is about to be discarded.
    RecordDiscardedTabMetrics(candidates[0]);
  } else {
    const uint64_t reclaim_target_kb_value = reclaim_target->target_kb;
    uint64_t total_reclaim_kb = 0;
    auto page_node_footprint_kb = GetPageNodeFootprintEstimateKb(candidates);
    for (auto& candidate : candidates) {
      if (total_reclaim_kb >= reclaim_target_kb_value) {
        break;
      }
      const PageNode* node = candidate.page_node();
      discard_attempts.emplace_back(node);

      // Record metrics about the tab that is about to be discarded.
      RecordDiscardedTabMetrics(candidate);

      // The node footprint value is updated by ProcessMetricsDecorator
      // periodically. The footprint value is 0 for nodes that have never been
      // updated, estimate the RSS value to 80 MiB for these nodes. 80 MiB is
      // the average Memory.Renderer.PrivateMemoryFootprint histogram value on
      // Windows in August 2021.
      uint64_t node_reclaim_kb = (page_node_footprint_kb[node])
                                     ? page_node_footprint_kb[node]
                                     : 80 * 1024;
      total_reclaim_kb += node_reclaim_kb;

      LOG(WARNING) << "Queueing discard attempt, type="
                   << performance_manager::PageNode::ToString(node->GetType())
                   << ", flags=[" << (candidate.is_focused() ? " focused" : "")
                   << (candidate.is_protected() ? " protected" : "")
                   << (candidate.is_visible() ? " visible" : "")
                   << " ] to save " << node_reclaim_kb << " KiB";
    }
  }

  // Clear the candidates vector to avoid holding on to pointers of the pages
  // that are about to be discarded.
  candidates.clear();

  if (discard_attempts.empty()) {
    // No pages left that are available for discarding.
    return std::nullopt;
  }

  // Adorns the PageNodes with a discard attempt marker to make sure that we
  // don't try to discard it multiple times if it fails to be discarded. In
  // practice this should only happen to prerenderers.
  for (auto* attempt : discard_attempts) {
    DiscardAttemptMarker::GetOrCreate(PageNodeImpl::FromNode(attempt));
  }

  std::vector<PageDiscarder::DiscardEvent> discard_events =
      page_discarder_->DiscardPageNodes(discard_attempts, discard_reason);

  if (discard_events.empty()) {
    // DiscardAttemptMarker will force the retry to choose different pages.
    return DiscardMultiplePages(reclaim_target, discard_protected_tabs,
                                discard_reason, minimum_time_in_background);
  }

  for (const auto& discard_event : discard_events) {
    unnecessary_discard_monitor_.OnDiscard(
        discard_event.estimated_memory_freed_kb, discard_event.discard_time);
  }

  unnecessary_discard_monitor_.OnReclaimTargetEnd();

  return discard_events[0].discard_time;
}

std::optional<base::TimeTicks>
PageDiscardingHelper::ImmediatelyDiscardMultiplePages(
    const std::vector<const PageNode*>& page_nodes,
    DiscardReason discard_reason) {
  // Pass 0 TimeDelta to bypass the minimum time in background check.
  return ImmediatelyDiscardMultiplePages(
      page_nodes, discard_reason,
      /*minimum_time_in_background=*/base::TimeDelta());
}

std::optional<base::TimeTicks>
PageDiscardingHelper::ImmediatelyDiscardMultiplePages(
    const std::vector<const PageNode*>& page_nodes,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  std::vector<const PageNode*> eligible_nodes;
  for (const PageNode* node : page_nodes) {
    if (CanDiscard(node, discard_reason, minimum_time_in_background) ==
        CanDiscardResult::kEligible) {
      eligible_nodes.emplace_back(node);
    }
  }

  if (eligible_nodes.empty()) {
    return std::nullopt;
  } else {
    auto discard_events = page_discarder_->DiscardPageNodes(
        std::move(eligible_nodes), discard_reason);
    if (discard_events.size() > 0) {
      return discard_events[0].discard_time;
    }
    return std::nullopt;
  }
}

void PageDiscardingHelper::SetNoDiscardPatternsForProfile(
    const std::string& browser_context_id,
    const std::vector<std::string>& patterns) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<url_matcher::URLMatcher>& entry =
      profiles_no_discard_patterns_[browser_context_id];
  entry = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFiltersWithLimit(entry.get(), patterns);
  if (opt_out_policy_changed_callback_) {
    opt_out_policy_changed_callback_.Run(browser_context_id);
  }
}

void PageDiscardingHelper::ClearNoDiscardPatternsForProfile(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  profiles_no_discard_patterns_.erase(browser_context_id);
  if (opt_out_policy_changed_callback_) {
    opt_out_policy_changed_callback_.Run(browser_context_id);
  }
}

void PageDiscardingHelper::SetMockDiscarderForTesting(
    std::unique_ptr<PageDiscarder> discarder) {
  page_discarder_ = std::move(discarder);
}

// static
void PageDiscardingHelper::AddDiscardAttemptMarkerForTesting(
    PageNode* page_node) {
  DiscardAttemptMarker::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

// static
void PageDiscardingHelper::RemovesDiscardAttemptMarkerForTesting(
    PageNode* page_node) {
  DiscardAttemptMarker::Destroy(PageNodeImpl::FromNode(page_node));
}

void PageDiscardingHelper::SetOptOutPolicyChangedCallback(
    base::RepeatingCallback<void(std::string_view)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  opt_out_policy_changed_callback_ = std::move(callback);
}

void PageDiscardingHelper::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageDiscardingHelper::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemovePageNodeObserver(this);
}

const PageLiveStateDecorator::Data*
PageDiscardingHelper::GetPageNodeLiveStateData(
    const PageNode* page_node) const {
  return PageLiveStateDecorator::Data::FromPageNode(page_node);
}

CanDiscardResult PageDiscardingHelper::CanDiscard(
    const PageNode* page_node,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background,
    std::vector<CannotDiscardReason>* cannot_discard_reasons) const {
  auto add_reason = [&](CannotDiscardReason reason) {
    if (cannot_discard_reasons) {
      cannot_discard_reasons->push_back(reason);
    }
  };

  // Don't discard pages which aren't tabs.
  if (page_node->GetType() != PageType::kTab) {
    add_reason(CannotDiscardReason::kNotATab);
    return CanDiscardResult::kDisallowed;
  }

  // Don't discard tabs for which discarding has already been attempted.
  if (DiscardAttemptMarker::Get(PageNodeImpl::FromNode(page_node)) &&
      !base::FeatureList::IsEnabled(kIgnoreDiscardAttemptMarker)) {
    add_reason(CannotDiscardReason::kDiscardAttempted);
    return CanDiscardResult::kDisallowed;
  }

  // Don't discard tabs that don't have a main frame (restored tab which is not
  // loaded yet, discarded tab, crashed tab).
  if (!page_node->GetMainFrameNode()) {
    add_reason(CannotDiscardReason::kNoMainFrame);
    return CanDiscardResult::kDisallowed;
  }

  bool is_proactive_or_suggested;
  switch (discard_reason) {
    case DiscardReason::EXTERNAL:
    case DiscardReason::FROZEN_WITH_GROWING_MEMORY:
      // Always allow discards.
      return CanDiscardResult::kEligible;
    case DiscardReason::URGENT:
      is_proactive_or_suggested = false;
      break;
    case DiscardReason::PROACTIVE:
    case DiscardReason::SUGGESTED:
      is_proactive_or_suggested = true;
      break;
  }

  CanDiscardResult result = CanDiscardResult::kEligible;
  auto add_reason_and_update_result = [&](CannotDiscardReason reason,
                                          CanDiscardResult new_result) {
    if (cannot_discard_reasons) {
      cannot_discard_reasons->push_back(reason);
    }
    result = std::underlying_type_t<CanDiscardResult>(result) <
                     std::underlying_type_t<CanDiscardResult>(new_result)
                 ? new_result
                 : result;
  };

  if (page_node->IsVisible()) {
    add_reason_and_update_result(CannotDiscardReason::kVisible,
                                 CanDiscardResult::kProtected);
  } else if (page_node->GetTimeSinceLastVisibilityChange() <
             minimum_time_in_background) {
    add_reason_and_update_result(CannotDiscardReason::kRecentlyVisible,
                                 CanDiscardResult::kProtected);
  }

  // Don't discard tabs that are playing or have recently played audio.
  if (page_node->IsAudible()) {
    add_reason_and_update_result(CannotDiscardReason::kAudible,
                                 CanDiscardResult::kProtected);
  } else if (page_node->GetTimeSinceLastAudibleChange().value_or(
                 base::TimeDelta::Max()) < kTabAudioProtectionTime) {
    add_reason_and_update_result(CannotDiscardReason::kRecentlyAudible,
                                 CanDiscardResult::kProtected);
  }

  // Don't discard pages that are displaying content in picture-in-picture.
  if (page_node->HasPictureInPicture()) {
    add_reason_and_update_result(CannotDiscardReason::kPictureInPicture,
                                 CanDiscardResult::kProtected);
  }

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  if (page_node->GetContentsMimeType() == "application/pdf") {
    add_reason_and_update_result(CannotDiscardReason::kPdf,
                                 CanDiscardResult::kProtected);
  }

  const GURL& main_frame_url = page_node->GetMainFrameUrl();
  if (!main_frame_url.is_valid() || main_frame_url.is_empty()) {
    add_reason_and_update_result(CannotDiscardReason::kInvalidURL,
                                 CanDiscardResult::kProtected);
  }

  // Only discard http(s) pages and internal pages to make sure that we don't
  // discard extensions or other PageNode that don't correspond to a tab.
  //
  // TODO(crbug.com/40910297): Due to a state tracking bug, sometimes there are
  // two frames marked "current". In that case GetMainFrameNode() returns an
  // arbitrary one, which may not have the url set correctly. Therefore, use
  // GetMainFrameUrl() for the url.
  bool is_web_page_or_internal_or_data_page =
      main_frame_url.SchemeIsHTTPOrHTTPS() ||
      main_frame_url.SchemeIs("chrome") ||
      main_frame_url.SchemeIs(url::kDataScheme);
  if (!is_web_page_or_internal_or_data_page) {
    add_reason_and_update_result(CannotDiscardReason::kNotWebOrInternal,
                                 CanDiscardResult::kProtected);
  }

  // The enterprise policy to except pages from discarding applies to both
  // proactive and urgent discards.
  if (IsPageOptedOutOfDiscarding(page_node->GetBrowserContextID(),
                                 main_frame_url)) {
    add_reason_and_update_result(CannotDiscardReason::kOptedOut,
                                 CanDiscardResult::kProtected);
  }

  if (is_proactive_or_suggested &&
      page_node->GetNotificationPermissionStatus() ==
          blink::mojom::PermissionStatus::GRANTED) {
    add_reason_and_update_result(CannotDiscardReason::kNotificationsEnabled,
                                 CanDiscardResult::kProtected);
  }

  const auto* live_state_data = GetPageNodeLiveStateData(page_node);

  // The live state data won't be available if none of these events ever
  // happened on the page.
  if (live_state_data) {
    // Don't discard the page if an extension is protecting it from discards.
    if (!live_state_data->IsAutoDiscardable()) {
      add_reason_and_update_result(CannotDiscardReason::kExtensionProtected,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingVideo()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingVideo,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingAudio()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingAudio,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsBeingMirrored()) {
      add_reason_and_update_result(CannotDiscardReason::kBeingMirrored,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingWindow()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingWindow,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsCapturingDisplay()) {
      add_reason_and_update_result(CannotDiscardReason::kCapturingDisplay,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsConnectedToBluetoothDevice()) {
      add_reason_and_update_result(CannotDiscardReason::kConnectedToBluetooth,
                                   CanDiscardResult::kProtected);
    }
    if (live_state_data->IsConnectedToUSBDevice()) {
      add_reason_and_update_result(CannotDiscardReason::kConnectedToUSB,
                                   CanDiscardResult::kProtected);
    }
    // Don't discard the active tab in any window, even if the window is not
    // visible. Otherwise the user would see a blank page when the window
    // becomes visible again, as the tab isn't reloaded until they click on it.
    if (live_state_data->IsActiveTab()) {
      add_reason_and_update_result(CannotDiscardReason::kActiveTab,
                                   CanDiscardResult::kProtected);
    }
    // Pinning a tab is a strong signal the user wants to keep it.
    if (live_state_data->IsPinnedTab()) {
      add_reason_and_update_result(CannotDiscardReason::kPinnedTab,
                                   CanDiscardResult::kProtected);
    }
    // Don't discard pages with devtools attached, because when it's restored
    // the devtools window won't come back. The user may be monitoring the page
    // in the background with devtools.
    if (live_state_data->IsDevToolsOpen()) {
      add_reason_and_update_result(CannotDiscardReason::kDevToolsOpen,
                                   CanDiscardResult::kProtected);
    }
    if (is_proactive_or_suggested &&
        live_state_data->UpdatedTitleOrFaviconInBackground()) {
      add_reason_and_update_result(CannotDiscardReason::kBackgroundActivity,
                                   CanDiscardResult::kProtected);
    }
#if !BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/391179510): This check validates the assumption that the
    // WasDiscarded() property is not set correctly. If that assumption holds,
    // remove all code that depends on it as discussed on the bug.
    CHECK(!live_state_data->WasDiscarded(), base::NotFatalUntil::M136);

    if (live_state_data->WasDiscarded()) {
      add_reason_and_update_result(CannotDiscardReason::kWasDiscarded,
                                   CanDiscardResult::kProtected);
    }
    // TODO(sebmarchand): Consider resetting the |WasDiscarded| value when the
    // main frame document changes, also remove the DiscardAttemptMarker in
    // this case.
#endif
  }

  // `HadUserEdits()` is currently a superset of `HadFormInteraction()` but
  // that may change so check both here (the check is not expensive).
  if (page_node->HadFormInteraction()) {
    add_reason_and_update_result(CannotDiscardReason::kFormInteractions,
                                 CanDiscardResult::kProtected);
  }

  if (page_node->HadUserEdits()) {
    add_reason_and_update_result(CannotDiscardReason::kUserEdits,
                                 CanDiscardResult::kProtected);
  }

  return result;
}

bool PageDiscardingHelper::IsPageOptedOutOfDiscarding(
    const std::string& browser_context_id,
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = profiles_no_discard_patterns_.find(browser_context_id);
  if (it == profiles_no_discard_patterns_.end()) {
    // There can be a narrow window between profile creation and when prefs are
    // read, which is when `profiles_no_discard_patterns_` is populated. During
    // that time assume that a page might be opted out of discarding.
    return true;
  }
  return !it->second->MatchURL(url).empty();
}

void PageDiscardingHelper::OnMainFrameDocumentChanged(
    const PageNode* page_node) {
  // When activated a discarded tab will re-navigate, instantiating a new
  // document. Ensure the DiscardAttemptMarker is cleared in these cases to
  // ensure a given tab remains eligible for discarding.
  DiscardAttemptMarker::Destroy(PageNodeImpl::FromNode(page_node));
}

base::Value::Dict PageDiscardingHelper::DescribePageNodeData(
    const PageNode* node) const {
  auto can_discard = [this, node](DiscardReason discard_reason) {
    switch (this->CanDiscard(node, discard_reason, base::TimeDelta())) {
      case CanDiscardResult::kEligible:
        return "eligible";
      case CanDiscardResult::kProtected:
        return "protected";
      case CanDiscardResult::kDisallowed:
        return "disallowed";
    }
  };

  base::Value::Dict ret;
  ret.Set("can_urgently_discard", can_discard(DiscardReason::URGENT));
  ret.Set("can_proactively_discard", can_discard(DiscardReason::PROACTIVE));
  if (!node->GetMainFrameUrl().is_empty()) {
    ret.Set("opted_out", IsPageOptedOutOfDiscarding(node->GetBrowserContextID(),
                                                    node->GetMainFrameUrl()));
  }

  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(node);
  if (tab_handle) {
    TabRevisitTracker* revisit_tracker =
        GetOwningGraph()->GetRegisteredObjectAs<TabRevisitTracker>();
    CHECK(revisit_tracker);
    TabRevisitTracker::StateBundle state =
        revisit_tracker->GetStateForTabHandle(tab_handle);
    ret.Set("num_revisits", static_cast<int>(state.num_revisits));
  }

  return ret;
}

}  // namespace policies
}  // namespace performance_manager
