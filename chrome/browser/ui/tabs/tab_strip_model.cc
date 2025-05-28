// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/not_fatal_until.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/commerce/browser_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/ui_utils.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/tabs/unpinned_tab_collection.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/range/range.h"

using base::UserMetricsAction;
using content::WebContents;

namespace {

TabGroupModelFactory* factory_instance = nullptr;

// Works similarly to base::AutoReset but checks for access from the wrong
// thread as well as ensuring that the previous value of the re-entrancy guard
// variable was false.
class ReentrancyCheck {
 public:
  explicit ReentrancyCheck(bool* guard_flag) : guard_flag_(guard_flag) {
    CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M130);
    CHECK(!*guard_flag_, base::NotFatalUntil::M130);
    *guard_flag_ = true;
  }

  ~ReentrancyCheck() { *guard_flag_ = false; }

 private:
  const raw_ptr<bool> guard_flag_;
};

// Returns true if the specified transition is one of the types that cause the
// opener relationships for the tab in which the transition occurred to be
// forgotten. This is generally any navigation that isn't a link click (i.e.
// any navigation that can be considered to be the start of a new task distinct
// from what had previously occurred in that tab).
bool ShouldForgetOpenersForTransition(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_KEYWORD) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

}  // namespace

TabGroupModelFactory::TabGroupModelFactory() {
  DCHECK(!factory_instance);
  factory_instance = this;
}

// static
TabGroupModelFactory* TabGroupModelFactory::GetInstance() {
  if (!factory_instance) {
    factory_instance = new TabGroupModelFactory();
  }
  return factory_instance;
}

std::unique_ptr<TabGroupModel> TabGroupModelFactory::Create(
    TabGroupController* controller) {
  return std::make_unique<TabGroupModel>(controller);
}

DetachedTabGroup::DetachedTabGroup(
    std::unique_ptr<tabs::TabGroupTabCollection> collection)
    : collection_(std::move(collection)) {}

DetachedTabGroup::~DetachedTabGroup() = default;
DetachedTabGroup::DetachedTabGroup(DetachedTabGroup&&) = default;

DetachedTab::DetachedTab(int index_before_any_removals,
                         int index_at_time_of_removal,
                         std::unique_ptr<tabs::TabModel> tab,
                         TabStripModelChange::RemoveReason remove_reason,
                         tabs::TabInterface::DetachReason tab_detach_reason,
                         std::optional<SessionID> id)
    : tab(std::move(tab)),
      index_before_any_removals(index_before_any_removals),
      index_at_time_of_removal(index_at_time_of_removal),
      remove_reason(remove_reason),
      tab_detach_reason(tab_detach_reason),
      id(id) {}
DetachedTab::~DetachedTab() = default;
DetachedTab::DetachedTab(DetachedTab&&) = default;

// Holds all state necessary to send notifications for detached tabs.
struct TabStripModel::DetachNotifications {
  DetachNotifications(tabs::TabInterface* initially_active_tab,
                      const ui::ListSelectionModel& selection_model)
      : initially_active_tab(initially_active_tab),
        selection_model(selection_model) {}
  DetachNotifications(const DetachNotifications&) = delete;
  DetachNotifications& operator=(const DetachNotifications&) = delete;
  ~DetachNotifications() = default;

  // The tab that was active prior to any detaches happening. If this
  // is nullptr, the active tab was not removed.
  //
  // It's safe to use a raw pointer here because the active tab, if
  // detached, is owned by `detached_tab`.
  //
  // Once the notification for change of active tab has been sent,
  // this field is set to nullptr.
  raw_ptr<tabs::TabInterface> initially_active_tab = nullptr;

  // The WebContents that were recently detached. Observers need to be notified
  // about these. These must be updated after construction.
  std::vector<std::unique_ptr<DetachedTab>> detached_tab;

  // The selection model prior to any tabs being detached.
  const ui::ListSelectionModel selection_model;
};

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

constexpr int TabStripModel::kNoTab;

TabStripModel::TabStripModel(TabStripModelDelegate* delegate,
                             Profile* profile,
                             TabGroupModelFactory* group_model_factory)
    : delegate_(delegate), profile_(profile) {
  DCHECK(delegate_);

  contents_data_ = std::make_unique<tabs::TabStripCollection>();

  if (group_model_factory) {
    group_model_ = group_model_factory->Create(this);
  }
  scrubbing_metrics_.Init();
}

TabStripModel::~TabStripModel() {
  for (auto& observer : observers_) {
    observer.ModelDestroyed(TabStripModelObserver::ModelPasskey(), this);
  }
}

void TabStripModel::SetTabStripUI(TabStripModelObserver* observer) {
  DCHECK(!tab_strip_ui_was_set_);

  std::vector<TabStripModelObserver*> new_observers{observer};
  for (auto& old_observer : observers_) {
    new_observers.push_back(&old_observer);
  }

  observers_.Clear();

  for (auto* new_observer : new_observers) {
    observers_.AddObserver(new_observer);
  }

  observer->StartedObserving(TabStripModelObserver::ModelPasskey(), this);
  tab_strip_ui_was_set_ = true;
}

void TabStripModel::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
  observer->StartedObserving(TabStripModelObserver::ModelPasskey(), this);
}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {
  observer->StoppedObserving(TabStripModelObserver::ModelPasskey(), this);
  observers_.RemoveObserver(observer);
}

int TabStripModel::count() const {
  return contents_data_->TotalTabCount();
}

bool TabStripModel::empty() const {
  return contents_data_->TotalTabCount() == 0;
}

bool TabStripModel::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModel::AppendWebContents(std::unique_ptr<WebContents> contents,
                                      bool foreground) {
  InsertWebContentsAt(
      count(), std::move(contents),
      foreground ? (ADD_INHERIT_OPENER | ADD_ACTIVE) : ADD_NONE);
}

void TabStripModel::AppendTab(std::unique_ptr<tabs::TabModel> tab,
                              bool foreground) {
  InsertDetachedTabAt(
      count(), std::move(tab),
      foreground ? (ADD_INHERIT_OPENER | ADD_ACTIVE) : ADD_NONE);
}

int TabStripModel::InsertWebContentsAt(
    int index,
    std::unique_ptr<WebContents> contents,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  return InsertDetachedTabAt(
      index, std::make_unique<tabs::TabModel>(std::move(contents), this),
      add_types, group);
}

int TabStripModel::InsertDetachedTabAt(
    int index,
    std::unique_ptr<tabs::TabModel> tab,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  tab->OnAddedToModel(this);
  return InsertTabAtImpl(index, std::move(tab), add_types, group);
}

std::unique_ptr<content::WebContents> TabStripModel::DiscardWebContentsAt(
    int index,
    std::unique_ptr<WebContents> new_contents) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  delegate()->WillAddWebContents(new_contents.get());

  CHECK(ContainsIndex(index));

  FixOpeners(index);

  TabStripSelectionChange selection(GetActiveTab(), selection_model_);
  WebContents* raw_new_contents = new_contents.get();
  std::unique_ptr<WebContents> old_contents =
      GetTabModelAtIndex(index)->DiscardContents(std::move(new_contents));

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    selection.new_contents = raw_new_contents;
    selection.reason = TabStripModelObserver::CHANGE_REASON_REPLACED;
  }

  TabStripModelChange::Replace replace;
  replace.tab = GetTabAtIndex(index);
  replace.old_contents = old_contents.get();
  replace.new_contents = raw_new_contents;
  replace.index = index;
  TabStripModelChange change(replace);
  OnChange(change, selection);

  return old_contents;
}

std::unique_ptr<tabs::TabModel> TabStripModel::DetachTabAtForInsertion(
    int index) {
  auto dt = DetachTabWithReasonAt(
      index, TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip,
      tabs::TabInterface::DetachReason::kInsertIntoOtherWindow);
  return std::move(dt->tab);
}

std::unique_ptr<content::WebContents>
TabStripModel::DetachWebContentsAtForInsertion(int index) {
  auto dt = DetachTabWithReasonAt(
      index, TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip,
      tabs::TabInterface::DetachReason::kDelete);
  return tabs::TabModel::DestroyAndTakeWebContents(std::move(dt->tab));
}

void TabStripModel::DetachAndDeleteWebContentsAt(int index) {
  // Drops the returned unique pointer.
  DetachTabWithReasonAt(index, TabStripModelChange::RemoveReason::kDeleted,
                        tabs::TabInterface::DetachReason::kDelete);
}

std::unique_ptr<DetachedTab> TabStripModel::DetachTabWithReasonAt(
    int index,
    TabStripModelChange::RemoveReason web_contents_remove_reason,
    tabs::TabInterface::DetachReason tab_detach_reason) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK_NE(active_index(), kNoTab) << "Activate the TabStripModel by "
                                      "selecting at least one tab before "
                                      "trying to detach web contents.";
  tabs::TabModel* active_tab_model = GetTabModelAtIndex(active_index());
  if (index == active_index() && !closing_all_) {
    active_tab_model->WillEnterBackground(base::PassKey<TabStripModel>());
  }
  GetTabModelAtIndex(index)->WillDetach(base::PassKey<TabStripModel>(),
                                        tab_detach_reason);

  DetachNotifications notifications(active_tab_model, selection_model_);
  auto dt = DetachTabImpl(index, index,
                          /*create_historical_tab=*/false,
                          web_contents_remove_reason, tab_detach_reason);
  notifications.detached_tab.push_back(std::move(dt));
  SendDetachWebContentsNotifications(&notifications);
  return std::move(notifications.detached_tab[0]);
}

std::unique_ptr<DetachedTabGroup> TabStripModel::DetachTabGroupForInsertion(
    const tab_groups::TabGroupId group_id) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(group_model_);
  CHECK(group_model_->ContainsTabGroup(group_id));

  return DetachTabGroupImpl(group_id);
}

void TabStripModel::InsertDetachedTabGroupAt(
    std::unique_ptr<DetachedTabGroup> group,
    int index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(group_model_);
  CHECK(!group_model_->ContainsTabGroup(group->collection_->GetTabGroupId()));

  InsertDetachedTabGroupImpl(std::move(group), index);
}

tabs::TabModel* TabStripModel::GetTabModelAtIndex(int index) const {
  return contents_data_->GetTabAtIndexRecursive(index);
}

void TabStripModel::OnChange(const TabStripModelChange& change,
                             const TabStripSelectionChange& selection) {
  OnActiveTabChanged(selection);

  for (auto& observer : observers_) {
    observer.OnTabStripModelChanged(this, change, selection);
  }
}

void TabStripModel::OnTabGroupDetached(const TabGroup& group) {
  for (auto& observer : observers_) {
    observer.OnTabGroupDetached(this, group);
  }
}

void TabStripModel::OnTabGroupAttached(const TabGroup& group) {
  for (auto& observer : observers_) {
    observer.OnTabGroupAttached(this, group);
  }
}

std::unique_ptr<DetachedTabGroup> TabStripModel::DetachTabGroupImpl(
    const tab_groups::TabGroupId& group_id) {
  NOTIMPLEMENTED();
  return std::make_unique<DetachedTabGroup>(nullptr);
}

void TabStripModel::InsertDetachedTabGroupImpl(
    std::unique_ptr<DetachedTabGroup> detached_group,
    int index) {
  NOTIMPLEMENTED();
}

std::unique_ptr<DetachedTab> TabStripModel::DetachTabImpl(
    int index_before_any_removals,
    int index_at_time_of_removal,
    bool create_historical_tab,
    TabStripModelChange::RemoveReason web_contents_remove_reason,
    tabs::TabInterface::DetachReason tab_detach_reason) {
  if (empty()) {
    return nullptr;
  }
  CHECK(ContainsIndex(index_at_time_of_removal));

  for (auto& observer : observers_) {
    observer.OnTabWillBeRemoved(
        GetTabAtIndex(index_at_time_of_removal)->GetContents(),
        index_at_time_of_removal);
  }

  FixOpeners(index_at_time_of_removal);

  // Ask the delegate to save an entry for this tab in the historical tab
  // database.
  tabs::TabModel* tab = GetTabModelAtIndex(index_at_time_of_removal);
  std::optional<SessionID> id = std::nullopt;
  if (create_historical_tab) {
    id = delegate_->CreateHistoricalTab(tab->GetContents());
  }
  if (tab_detach_reason == tabs::TabInterface::DetachReason::kDelete) {
    tab->DestroyTabFeatures();
  }

  std::unique_ptr<tabs::TabModel> old_tab_model =
      RemoveTabFromIndexImpl(index_at_time_of_removal);

  old_tab_model->OnRemovedFromModel();
  return std::make_unique<DetachedTab>(
      index_before_any_removals, index_at_time_of_removal,
      std::move(old_tab_model), web_contents_remove_reason, tab_detach_reason,
      id);
}

void TabStripModel::SendDetachWebContentsNotifications(
    DetachNotifications* notifications) {
  // Sort the DetachedTab in decreasing order of
  // |index_before_any_removals|. This is because |index_before_any_removals| is
  // used by observers to update their own copy of TabStripModel state, and each
  // removal affects subsequent removals of higher index.
  std::sort(
      notifications->detached_tab.begin(), notifications->detached_tab.end(),
      [](const std::unique_ptr<DetachedTab>& dt1,
         const std::unique_ptr<DetachedTab>& dt2) {
        return dt1->index_before_any_removals > dt2->index_before_any_removals;
      });

  // `change` must be deleted before the unique_ptr<Tab>s in `notifications` are
  // reset, or their raw_ptr<Tab>s will dangle.
  {
    TabStripModelChange::Remove remove;
    for (auto& dt : notifications->detached_tab) {
      remove.contents.emplace_back(dt->tab.get(), dt->index_before_any_removals,
                                   dt->remove_reason, dt->tab_detach_reason,
                                   dt->id);
    }

    TabStripModelChange change(std::move(remove));

    TabStripSelectionChange selection;
    selection.old_tab = notifications->initially_active_tab;
    selection.new_tab = GetActiveTab();
    selection.old_contents =
        selection.old_tab ? selection.old_tab->GetContents() : nullptr;
    selection.new_contents = GetActiveWebContents();
    selection.old_model = notifications->selection_model;
    selection.new_model = selection_model_;
    selection.reason = TabStripModelObserver::CHANGE_REASON_NONE;
    selection.selected_tabs_were_removed = std::ranges::any_of(
        notifications->detached_tab, [&notifications](auto& dt) {
          return notifications->selection_model.IsSelected(
              dt->index_before_any_removals);
        });
    OnChange(change, selection);

    // Prevent this from dangling in case a detached tab was initially active.
    notifications->initially_active_tab = nullptr;
  }

  for (auto& dt : notifications->detached_tab) {
    if (dt->remove_reason == TabStripModelChange::RemoveReason::kDeleted) {
      // This destroys the WebContents, which will also send
      // WebContentsDestroyed notifications.
      dt->tab.reset();
    }
  }

  if (empty()) {
    for (auto& observer : observers_) {
      observer.TabStripEmpty();
    }
  }
}

void TabStripModel::ActivateTabAt(int index,
                                  TabStripUserGestureDetails user_gesture) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));
  TRACE_EVENT0("ui", "TabStripModel::ActivateTabAt");

  scrubbing_metrics_.IncrementPressCount(user_gesture);

  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectedIndex(index);
  SetSelection(
      std::move(new_model),
      user_gesture.type != TabStripUserGestureDetails::GestureType::kNone
          ? TabStripModelObserver::CHANGE_REASON_USER_GESTURE
          : TabStripModelObserver::CHANGE_REASON_NONE,
      /*triggered_by_other_operation=*/false);
}

int TabStripModel::MoveWebContentsAt(int index,
                                     int to_position,
                                     bool select_after_move) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));
  const bool pinned = IsTabPinned(index);

  to_position = ConstrainMoveIndex(to_position, pinned);

  if (index == to_position) {
    return to_position;
  }

  std::optional<tab_groups::TabGroupId> group =
      GetGroupToAssign(index, to_position);
  MoveTabToIndexImpl(index, to_position, group, pinned, select_after_move);

  return to_position;
}

int TabStripModel::MoveWebContentsAt(
    int index,
    int to_position,
    bool select_after_move,
    std::optional<tab_groups::TabGroupId> group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(ContainsIndex(index));

  bool pinned = IsTabPinned(index);
  to_position = ConstrainMoveIndex(to_position, pinned);
  MoveTabToIndexImpl(index, to_position, group, pinned, select_after_move);
  return to_position;
}

void TabStripModel::MoveSelectedTabsTo(
    int index,
    std::optional<tab_groups::TabGroupId> into_group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const std::vector<int> pinned_selected_indices = GetSelectedPinnedTabs();
  const std::vector<int> unpinned_selected_indices = GetSelectedUnpinnedTabs();

  const int last_pinned_index =
      std::clamp(index + static_cast<int>(pinned_selected_indices.size()) - 1,
                 static_cast<int>(pinned_selected_indices.size()) - 1,
                 pinned_tab_count - 1);

  if (group_model_) {
    std::vector<tab_groups::TabGroupId> groups_to_destroy;
    for (const auto& group_id : group_model_->ListTabGroups()) {
      if (into_group.has_value() && into_group.value() == group_id) {
        continue;
      }
      TabGroup* tab_group = group_model_->GetTabGroup(group_id);
      const gfx::Range tabs_in_group = tab_group->ListTabs();
      bool all_selected = true;
      for (size_t i = tabs_in_group.start(); i < tabs_in_group.end(); ++i) {
        if (std::find(unpinned_selected_indices.begin(),
                      unpinned_selected_indices.end(),
                      static_cast<int>(i)) == unpinned_selected_indices.end()) {
          all_selected = false;
          break;
        }
      }
      if (all_selected) {
        groups_to_destroy.push_back(group_id);
      }
    }

    for (const auto& group_id : groups_to_destroy) {
      delegate_->WillCloseGroup(group_id);
    }
  }

  MoveTabsToIndexImpl(
      pinned_selected_indices,
      last_pinned_index - static_cast<int>(pinned_selected_indices.size()) + 1,
      std::nullopt);

  const int first_unpinned_index =
      std::clamp(index + static_cast<int>(pinned_selected_indices.size()),
                 pinned_tab_count,
                 count() - static_cast<int>(unpinned_selected_indices.size()));

  MoveTabsToIndexImpl(unpinned_selected_indices, first_unpinned_index,
                      into_group);
}

void TabStripModel::MoveGroupTo(const tab_groups::TabGroupId& group,
                                int to_index) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK_NE(to_index, kNoTab);
  to_index = ConstrainMoveIndex(to_index, false /* pinned tab */);

  if (!group_model_) {
    return;
  }

  MoveGroupToImpl(group, to_index);
}

void TabStripModel::MoveGroupToImpl(const tab_groups::TabGroupId& group,
                                    int to_index) {
  const gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  CHECK_GT(tabs_in_group.length(), 0u);

  std::vector<int> tab_indices;
  for (size_t i = tabs_in_group.start(); i < tabs_in_group.end(); ++i) {
    tab_indices.push_back(i);
  }

  const std::vector<MoveNotification> notifications =
      PrepareTabsToMoveToIndex(tab_indices, to_index);

  // Remove all the tabs from the model.
  contents_data_->MoveGroupTo(group, to_index);

  UpdateSelectionModelForMoves(tab_indices, to_index);
  ValidateTabStripModel();

  for (auto notification : notifications) {
    const int final_index = GetIndexOfTab(notification.tab);
    tabs::TabInterface* tab = GetTabAtIndex(final_index);
    if (notification.initial_index != final_index) {
      SendMoveNotificationForTab(notification.initial_index, final_index, tab,
                                 notification.selection_change);
    }

    if (notification.intial_group != tab->GetGroup()) {
      TabGroupStateChanged(final_index, tab, notification.intial_group,
                           tab->GetGroup());
    }
  }

  MoveTabGroup(group);
}

WebContents* TabStripModel::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

tabs::TabInterface* TabStripModel::GetActiveTab() const {
  const int index = active_index();
  if (ContainsIndex(index)) {
    return GetTabAtIndex(index);
  }
  return nullptr;
}

WebContents* TabStripModel::GetWebContentsAt(int index) const {
  if (ContainsIndex(index)) {
    return GetTabAtIndex(index)->GetContents();
  }
  return nullptr;
}

int TabStripModel::GetIndexOfWebContents(const WebContents* contents) const {
  for (int i = 0; i < GetTabCount(); ++i) {
    if (GetTabAtIndex(i)->GetContents() == contents) {
      return i;
    }
  }
  return kNoTab;
}

void TabStripModel::NotifyTabChanged(const tabs::TabInterface* const tab,
                                     TabChangeType change_type) {
  const int index = GetIndexOfTab(tab);
  for (auto& observer : observers_) {
    observer.TabChangedAt(tab->GetContents(), index, change_type);
  }
}

void TabStripModel::UpdateWebContentsStateAt(int index,
                                             TabChangeType change_type) {
  const tabs::TabInterface* const tab = GetTabAtIndex(index);

  for (auto& observer : observers_) {
    observer.TabChangedAt(tab->GetContents(), index, change_type);
  }
}

void TabStripModel::SetTabNeedsAttentionAt(int index, bool attention) {
  CHECK(ContainsIndex(index));

  for (auto& observer : observers_) {
    observer.SetTabNeedsAttentionAt(index, attention);
  }
}

void TabStripModel::CloseAllTabs() {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseWebContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(count());
  for (int i = count() - 1; i >= 0; --i) {
    closing_tabs.push_back(GetWebContentsAt(i));
  }
  CloseTabs(closing_tabs, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseAllTabsInGroup(const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_) {
    return;
  }

  delegate_->WillCloseGroup(group);

  for (TabStripModelObserver& observer : observers_) {
    observer.OnTabGroupWillBeRemoved(group);
  }

  TabGroup* const tab_group = group_model_->GetTabGroup(group);
  tab_group->SetGroupIsClosing(/*is_closing=*/true);

  gfx::Range tabs_in_group = tab_group->ListTabs();
  if (static_cast<int>(tabs_in_group.length()) == count()) {
    closing_all_ = true;
  }

  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(tabs_in_group.length());
  for (uint32_t i = tabs_in_group.end(); i > tabs_in_group.start(); --i) {
    closing_tabs.push_back(GetWebContentsAt(i - 1));
  }
  CloseTabs(closing_tabs, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void TabStripModel::CloseWebContentsAt(int index, uint32_t close_types) {
  CHECK(ContainsIndex(index));
  CloseTabs({GetWebContentsAt(index)}, close_types);
}

bool TabStripModel::TabsNeedLoadingUI() const {
  for (int i = 0; i < GetTabCount(); i++) {
    const tabs::TabInterface* const tab = GetTabAtIndex(i);
    if (tab->GetContents()->ShouldShowLoadingUI()) {
      return true;
    }
  }

  return false;
}

tabs::TabInterface* TabStripModel::GetOpenerOfTabAt(const int index) const {
  CHECK(ContainsIndex(index));
  const tabs::TabModel* const tab = GetTabModelAtIndex(index);

  return tab->opener();
}

void TabStripModel::SetOpenerOfWebContentsAt(int index, WebContents* opener) {
  CHECK(ContainsIndex(index));
  // The TabStripModel only maintains the references to openers that it itself
  // owns; trying to set an opener to an external WebContents can result in
  // the opener being used after its freed. See crbug.com/698681.
  DCHECK(!opener || GetIndexOfWebContents(opener) != kNoTab)
      << "Cannot set opener to a web contents not owned by this tab strip.";
  GetTabModelAtIndex(index)->set_opener(GetTabForWebContents(opener));
}

int TabStripModel::GetIndexOfLastWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  CHECK(ContainsIndex(start_index));

  std::set<const WebContents*> opener_and_descendants;
  opener_and_descendants.insert(opener);
  int last_index = kNoTab;

  for (int i = start_index + 1; i < count(); ++i) {
    tabs::TabModel* tab = GetTabModelAtIndex(i);
    // Test opened by transitively, i.e. include tabs opened by tabs opened by
    // opener, etc. Stop when we find the first non-descendant.
    if (!opener_and_descendants.count(
            tab->opener() ? tab->opener()->GetContents() : nullptr)) {
      // Skip over pinned tabs as new tabs are added after pinned tabs.
      if (tab->IsPinned()) {
        continue;
      }
      break;
    }
    opener_and_descendants.insert(tab->GetContents());
    last_index = i;
  }
  return last_index;
}

void TabStripModel::TabNavigating(WebContents* contents,
                                  ui::PageTransition transition) {
  if (ShouldForgetOpenersForTransition(transition)) {
    // Don't forget the openers if this tab is a New Tab page opened at the
    // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
    // navigation of one of these transition types before resetting the
    // opener relationships (this allows for the use case of opening a new
    // tab to do a quick look-up of something while viewing a tab earlier in
    // the strip). We can make this heuristic more permissive if need be.
    if (!IsNewTabAtEndOfTabStrip(contents)) {
      // If the user navigates the current tab to another page in any way
      // other than by clicking a link, we want to pro-actively forget all
      // TabStrip opener relationships since we assume they're beginning a
      // different task by reusing the current tab.
      ForgetAllOpeners();
    }
  }
}

void TabStripModel::SetTabBlocked(int index, bool blocked) {
  CHECK(ContainsIndex(index));
  tabs::TabModel* tab_model = GetTabModelAtIndex(index);
  if (tab_model->blocked() == blocked) {
    return;
  }
  tab_model->set_blocked(blocked);
  for (auto& observer : observers_) {
    observer.TabBlockedStateChanged(tab_model->GetContents(), index);
  }
}

int TabStripModel::SetTabPinned(int index, bool pinned) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(ContainsIndex(index));

  const tabs::TabInterface* const tab = GetTabAtIndex(index);

  if (tab->IsPinned() == pinned) {
    return index;
  }

  const int final_index =
      pinned ? IndexOfFirstNonPinnedTab() : IndexOfFirstNonPinnedTab() - 1;

  MoveTabToIndexImpl(index, final_index, std::nullopt, pinned, false);
  return final_index;
}

bool TabStripModel::IsTabPinned(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return index < IndexOfFirstNonPinnedTab();
}

bool TabStripModel::IsTabCollapsed(int index) const {
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  return group.has_value() && IsGroupCollapsed(group.value());
}

bool TabStripModel::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  DCHECK(group_model_);

  return group_model()->ContainsTabGroup(group) &&
         group_model()->GetTabGroup(group)->visual_data()->is_collapsed();
}

bool TabStripModel::IsTabSplit(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return GetTabModelAtIndex(index)->IsSplit();
}

bool TabStripModel::IsTabBlocked(int index) const {
  CHECK(ContainsIndex(index)) << index;
  return GetTabModelAtIndex(index)->blocked();
}

bool TabStripModel::IsTabClosable(int index) const {
  return PolicyAllowsTabClosing(GetWebContentsAt(index));
}

bool TabStripModel::IsTabClosable(const content::WebContents* contents) const {
  return IsTabClosable(GetIndexOfWebContents(contents));
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetTabGroupForTab(
    int index) const {
  return ContainsIndex(index) ? GetTabAtIndex(index)->GetGroup() : std::nullopt;
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetSurroundingTabGroup(
    int index) const {
  if (!ContainsIndex(index - 1) || !ContainsIndex(index)) {
    return std::nullopt;
  }

  // If the tab before is not in a group, a tab inserted at |index|
  // wouldn't be surrounded by one group.
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index - 1);
  if (!group) {
    return std::nullopt;
  }

  // If the tab after is in a different (or no) group, a new tab at
  // |index| isn't surrounded.
  if (group != GetTabGroupForTab(index)) {
    return std::nullopt;
  }
  return group;
}

int TabStripModel::IndexOfFirstNonPinnedTab() const {
  return contents_data_->IndexOfFirstNonPinnedTab();
}

void TabStripModel::ExtendSelectionTo(int index) {
  CHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::ToggleSelectionAt(int index) {
  if (!delegate()->IsTabStripEditable()) {
    return false;
  }
  CHECK(ContainsIndex(index));
  const size_t index_size_t = static_cast<size_t>(index);
  ui::ListSelectionModel new_model = selection_model();
  if (selection_model_.IsSelected(index_size_t)) {
    if (selection_model_.size() == 1) {
      // One tab must be selected and this tab is currently selected so we can't
      // unselect it.
      return false;
    }
    new_model.RemoveIndexFromSelection(index_size_t);
    new_model.set_anchor(index_size_t);
    if (!new_model.active().has_value() || new_model.active() == index_size_t) {
      new_model.set_active(*new_model.selected_indices().begin());
    }
  } else {
    new_model.AddIndexToSelection(index_size_t);
    new_model.set_anchor(index_size_t);
    new_model.set_active(index_size_t);
  }
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
  return true;
}

void TabStripModel::AddSelectionFromAnchorTo(int index) {
  ui::ListSelectionModel new_model = selection_model_;
  new_model.AddSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

bool TabStripModel::IsTabSelected(int index) const {
  CHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

void TabStripModel::SetSelectionFromModel(ui::ListSelectionModel source) {
  CHECK(source.active().has_value());
  SetSelection(std::move(source), TabStripModelObserver::CHANGE_REASON_NONE,
               /*triggered_by_other_operation=*/false);
}

const ui::ListSelectionModel& TabStripModel::selection_model() const {
  return selection_model_;
}

bool TabStripModel::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<ScopedTabStripModalUI> TabStripModel::ShowModalUI() {
  return std::make_unique<ScopedTabStripModalUIImpl>(this);
}

void TabStripModel::ForceShowingModalUIForTesting(bool showing) {
  showing_modal_ui_ = showing;
}

void TabStripModel::AddWebContents(
    std::unique_ptr<WebContents> contents,
    int index,
    ui::PageTransition transition,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  auto tab = std::make_unique<tabs::TabModel>(std::move(contents), this);
  AddTab(std::move(tab), index, transition, add_types, group);
}

void TabStripModel::AddTab(std::unique_ptr<tabs::TabModel> tab,
                           int index,
                           ui::PageTransition transition,
                           int add_types,
                           std::optional<tab_groups::TabGroupId> group) {
  for (auto& observer : observers_) {
    observer.OnTabWillBeAdded();
  }

  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // If the newly-opened tab is part of the same task as the parent tab, we want
  // to inherit the parent's opener attribute, so that if this tab is then
  // closed we'll jump back to the parent tab.
  bool inherit_opener = (add_types & ADD_INHERIT_OPENER) == ADD_INHERIT_OPENER;

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_LINK) &&
      (add_types & ADD_FORCE_INDEX) == 0) {
    // We assume tabs opened via link clicks are part of the same task as their
    // parent.  Note that when |force_index| is true (e.g. when the user
    // drag-and-drops a link to the tab strip), callers aren't really handling
    // link clicks, they just want to score the navigation like a link click in
    // the history backend, so we don't inherit the opener in this case.
    index = DetermineInsertionIndex(transition, add_types & ADD_ACTIVE);
    inherit_opener = true;

    // The current active index is our opener. If the tab we are adding is not
    // in a group, set the group of the tab to that of its opener.
    if (!group.has_value()) {
      group = GetTabGroupForTab(active_index());
    }
  } else {
    // For all other types, respect what was passed to us, normalizing -1s and
    // values that are too large.
    if (index < 0 || index > count()) {
      index = count();
    }
  }

  // Prevent the tab from being inserted at an index that would make the group
  // non-contiguous. Most commonly, the new-tab button always attempts to insert
  // at the end of the tab strip. Extensions can insert at an arbitrary index,
  // so we have to handle the general case.
  if (group_model_) {
    if (group.has_value()) {
      gfx::Range grouped_tabs =
          group_model_->GetTabGroup(group.value())->ListTabs();
      if (grouped_tabs.length() > 0) {
        index = std::clamp(index, static_cast<int>(grouped_tabs.start()),
                           static_cast<int>(grouped_tabs.end()));
      }
    } else if (GetTabGroupForTab(index - 1) == GetTabGroupForTab(index)) {
      group = GetTabGroupForTab(index);
    }

    // Pinned tabs cannot be added to a group.
    if (add_types & ADD_PINNED) {
      group = std::nullopt;
    }
  } else {
    group = std::nullopt;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_TYPED) &&
      index == count()) {
    // Also, any tab opened at the end of the TabStrip with a "TYPED"
    // transition inherit opener as well. This covers the cases where the user
    // creates a New Tab (e.g. Ctrl+T, or clicks the New Tab button), or types
    // in the address bar and presses Alt+Enter. This allows for opening a new
    // Tab to quickly look up something. When this Tab is closed, the old one
    // is re-activated, not the next-adjacent.
    inherit_opener = true;
  }
  tabs::TabModel* tab_ptr = tab.get();
  tab->OnAddedToModel(this);
  InsertTabAtImpl(index, std::move(tab),
                  add_types | (inherit_opener ? ADD_INHERIT_OPENER : 0), group);
  // Reset the index, just in case insert ended up moving it on us.
  index = GetIndexOfTab(tab_ptr);

  // In the "quick look-up" case detailed above, we want to reset the opener
  // relationship on any active tab change, even to another tab in the same tree
  // of openers. A jump would be too confusing at that point.
  if (inherit_opener && ui::PageTransitionTypeIncludingQualifiersIs(
                            transition, ui::PAGE_TRANSITION_TYPED)) {
    tab_ptr->set_reset_opener_on_active_tab_change(true);
  }

  // TODO(sky): figure out why this is here and not in InsertWebContentsAt. When
  // here we seem to get failures in startup perf tests.
  // Ensure that the new WebContentsView begins at the same size as the
  // previous WebContentsView if it existed.  Otherwise, the initial WebKit
  // layout will be performed based on a width of 0 pixels, causing a
  // very long, narrow, inaccurate layout.  Because some scripts on pages (as
  // well as WebKit's anchor link location calculation) are run on the
  // initial layout and not recalculated later, we need to ensure the first
  // layout is performed with sane view dimensions even when we're opening a
  // new background tab.
  if (WebContents* old_contents = GetActiveWebContents()) {
    if ((add_types & ADD_ACTIVE) == 0) {
      tab_ptr->GetContents()->Resize(
          gfx::Rect(old_contents->GetContainerBounds().size()));
    }
  }
}

void TabStripModel::CloseSelectedTabs() {
  auto get_indices = base::BindRepeating(
      [](const ui::ListSelectionModel& selection_model) {
        const ui::ListSelectionModel::SelectedIndices& sel =
            selection_model.selected_indices();
        return std::vector<int>(sel.begin(), sel.end());
      },
      selection_model_);

  ExecuteCloseTabsByIndicesCommand(std::move(get_indices),
                                   /*delete_groups=*/true);
}

void TabStripModel::SelectNextTab(TabStripUserGestureDetails detail) {
  SelectRelativeTab(TabRelativeDirection::kNext, detail);
}

void TabStripModel::SelectPreviousTab(TabStripUserGestureDetails detail) {
  SelectRelativeTab(TabRelativeDirection::kPrevious, detail);
}

void TabStripModel::SelectLastTab(TabStripUserGestureDetails detail) {
  ActivateTabAt(count() - 1, detail);
}

void TabStripModel::MoveTabNext() {
  MoveTabRelative(TabRelativeDirection::kNext);
}

void TabStripModel::MoveTabPrevious() {
  MoveTabRelative(TabRelativeDirection::kPrevious);
}

void TabStripModel::AddToNewSplit(const std::vector<int> indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  // Ensure that there are exactly 2 indices, and that they are sorted and
  // unique.
  CHECK_EQ(indices.size(), 2u);
  CHECK(std::ranges::is_sorted(indices));
  CHECK(std::ranges::adjacent_find(indices) == indices.end());

  std::vector<tabs::TabInterface*> tabs = {};
  for (int i : indices) {
    tabs::TabModel* tab_model = GetTabModelAtIndex(i);
    CHECK(!tab_model->IsSplit());
    tabs.push_back(tab_model);
    tab_model->set_split(true);
  }

  // Find a destination for the first tab that's not pinned or grouped. We will
  // stack the rest of the tabs up to its right.
  int destination_index = -1;
  for (int i = indices[0]; i < count(); i++) {
    const int destination_candidate = i + 1;

    // Splitting at the end of the tabstrip is always valid.
    if (!ContainsIndex(destination_candidate)) {
      destination_index = destination_candidate;
      break;
    }

    // Splitting in the middle of pinned tabs is never valid.
    if (IsTabPinned(destination_candidate)) {
      continue;
    }

    // Otherwise, splitting is valid if the destination is not in a group
    std::optional<tab_groups::TabGroupId> destination_group =
        GetTabGroupForTab(destination_candidate);
    if (!destination_group.has_value()) {
      destination_index = destination_candidate;
      break;
    }
  }

  MoveTabsAndSetGroupImpl(indices, destination_index, std::nullopt);

  for (TabStripModelObserver& observer : observers_) {
    observer.OnSplitViewAdded(tabs);
  }
}

void TabStripModel::AddTabGroup(const tab_groups::TabGroupId group_id,
                                tab_groups::TabGroupVisualData visual_data) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(SupportsTabGroups());
  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(group_id, visual_data,
                                                    this);
  group_model_->AddTabGroup(group_collection->GetTabGroup(),
                            base::PassKey<TabStripModel>());
  contents_data_->CreateTabGroup(std::move(group_collection));
}

tab_groups::TabGroupId TabStripModel::AddToNewGroup(
    const std::vector<int> indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  CHECK(SupportsTabGroups());

  // Ensure that the indices are nonempty, sorted, and unique.
  CHECK_GT(indices.size(), 0u);
  CHECK(std::ranges::is_sorted(indices));
  CHECK(std::ranges::adjacent_find(indices) == indices.end());

  // The odds of |new_group| colliding with an existing group are astronomically
  // low. If there is a collision, a DCHECK will fail in |AddToNewGroupImpl()|,
  // in which case there is probably something wrong with
  // |tab_groups::TabGroupId::GenerateNew()|.
  const tab_groups::TabGroupId new_group =
      tab_groups::TabGroupId::GenerateNew();
  AddToNewGroupImpl(indices, new_group);
  // TODO(crbug.com/339858272) : Consolidate all default save logic to
  // TabStripModel::AddToNewGroupImpl.
  delegate_->GroupAdded(new_group);

  for (TabStripModelObserver& observer : observers_) {
    observer.OnTabGroupAdded(new_group);
  }

  return new_group;
}

void TabStripModel::AddToExistingGroup(const std::vector<int> indices,
                                       const tab_groups::TabGroupId group,
                                       const bool add_to_end) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  CHECK(SupportsTabGroups());

  // Ensure that the indices are sorted and unique.
  DCHECK(std::ranges::is_sorted(indices));
  DCHECK(std::ranges::adjacent_find(indices) == indices.end());
  CHECK(ContainsIndex(*(indices.begin())));
  CHECK(ContainsIndex(*(indices.rbegin())));

  AddToExistingGroupImpl(indices, group, add_to_end);
}

void TabStripModel::AddToGroupForRestore(const std::vector<int>& indices,
                                         const tab_groups::TabGroupId& group) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  DCHECK(group_model_);
  if (!group_model_) {
    return;
  }

  const bool group_exists = group_model_->ContainsTabGroup(group);
  if (group_exists) {
    AddToExistingGroupImpl(indices, group);
  } else {
    AddToNewGroupImpl(indices, group);
  }
}

void TabStripModel::RemoveFromGroup(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  if (!group_model_) {
    return;
  }

  std::map<tab_groups::TabGroupId, std::vector<int>> indices_per_tab_group;

  for (int index : indices) {
    std::optional<tab_groups::TabGroupId> old_group = GetTabGroupForTab(index);
    if (old_group.has_value()) {
      indices_per_tab_group[old_group.value()].push_back(index);
    }
  }

  for (const auto& kv : indices_per_tab_group) {
    const TabGroup* group = group_model_->GetTabGroup(kv.first);
    const int first_tab_in_group = group->GetFirstTab().value();
    const int last_tab_in_group = group->GetLastTab().value();

    // This is an estimate. If the group is non-contiguous it will be
    // larger than the true size. This can happen while dragging tabs in
    // or out of a group.
    const int num_tabs_in_group = last_tab_in_group - first_tab_in_group + 1;
    const int group_midpoint = first_tab_in_group + num_tabs_in_group / 2;

    // Split group into |left_of_group| and |right_of_group| depending on
    // whether the index is closest to the left or right edge.
    std::vector<int> left_of_group;
    std::vector<int> right_of_group;
    for (int index : kv.second) {
      if (index < group_midpoint) {
        left_of_group.push_back(index);
      } else {
        right_of_group.push_back(index);
      }
    }
    MoveTabsAndSetGroupImpl(left_of_group, first_tab_in_group, std::nullopt);
    MoveTabsAndSetGroupImpl(right_of_group, last_tab_in_group + 1,
                            std::nullopt);
  }
}

bool TabStripModel::IsReadLaterSupportedForAny(
    const std::vector<int>& indices) {
  if (!delegate_->SupportsReadLater()) {
    return false;
  }

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(profile_);
  if (!model || !model->loaded()) {
    return false;
  }
  for (int index : indices) {
    if (model->IsUrlSupported(
            chrome::GetURLToBookmark(GetWebContentsAt(index)))) {
      return true;
    }
  }
  return false;
}

void TabStripModel::AddToReadLater(const std::vector<int>& indices) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);

  AddToReadLaterImpl(indices);
}

void TabStripModel::CreateTabGroup(const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }

  TabGroupChange change(this, group, TabGroupChange::kCreated);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::OpenTabGroupEditor(const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }

  TabGroupChange change(this, group, TabGroupChange::kEditorOpened);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::ChangeTabGroupVisuals(
    const tab_groups::TabGroupId& group,
    const TabGroupChange::VisualsChange& visuals) {
  if (!group_model_) {
    return;
  }

  TabGroupChange change(this, group, visuals);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::MoveTabGroup(const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }

  TabGroupChange change(this, group, TabGroupChange::kMoved);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

void TabStripModel::CloseTabGroup(const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }

  TabGroupChange change(this, group, TabGroupChange::kClosed);
  for (auto& observer : observers_) {
    observer.OnTabGroupChanged(change);
  }
}

std::u16string TabStripModel::GetTitleAt(int index) const {
  return TabUIHelper::FromWebContents(GetWebContentsAt(index))->GetTitle();
}

int TabStripModel::GetTabCount() const {
  return contents_data_->TotalTabCount();
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
  // Command must be valid.
  DCHECK(command_id > CommandFirst && command_id < CommandLast);

  // Context Index having an index greater than tab strip model doesnt make
  // sense since this context menu must target a tab.
  if (!ContainsIndex(context_index)) {
    return false;
  }

  switch (command_id) {
    case CommandNewTabToRight:
    case CommandCloseTab:
      return true;

    case CommandReload:
      return delegate_->CanReload();

    case CommandCloseOtherTabs:
    case CommandCloseTabsToRight: {
      return !GetIndicesClosedByCommand(context_index, command_id).empty();
    }
    case CommandDuplicate: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (int index : indices) {
        if (delegate()->CanDuplicateContentsAt(index)) {
          return true;
        }
      }
      return false;
    }

    case CommandToggleSiteMuted: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (int index : indices) {
        if (!GetWebContentsAt(index)->GetLastCommittedURL().is_empty()) {
          return true;
        }
      }
      return false;
    }

    case CommandTogglePinned:
      return true;

    case CommandToggleGrouped:
      return SupportsTabGroups();

    case CommandSendTabToSelf:
      return true;

    case CommandAddToReadLater:
      return true;

    case CommandAddToNewGroup:
      return SupportsTabGroups();

    case CommandAddToExistingGroup:
      return SupportsTabGroups();

    case CommandAddToSplit:
      return true;

    case CommandRemoveFromGroup:
      return SupportsTabGroups();

    case CommandMoveToExistingWindow:
      return true;

    case CommandMoveTabsToNewWindow: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      const bool would_leave_strip_empty =
          static_cast<int>(indices.size()) == count();
      return !would_leave_strip_empty &&
             delegate()->CanMoveTabsToWindow(indices);
    }

    case CommandOrganizeTabs:
      return true;

    case CommandCommerceProductSpecifications: {
      auto selected_web_contents =
          GetWebContentsesByIndices(GetIndicesForCommand(context_index));
      return commerce::IsProductSpecsMultiSelectMenuEnabled(
                 profile_, GetWebContentsAt(context_index)) &&
             commerce::IsWebContentsListEligibleForProductSpecs(
                 selected_web_contents);
    }

    case CommandAddToNewComparisonTable:
    case CommandAddToExistingComparisonTable:
      return commerce::IsUrlEligibleForProductSpecs(
          GetWebContentsAt(context_index)->GetLastCommittedURL());

    case CommandCopyURL:
      DCHECK(delegate()->IsForWebApp());
      return true;

    case CommandGoBack:
      DCHECK(delegate()->IsForWebApp());
      return delegate()->CanGoBack(GetWebContentsAt(context_index));

    case CommandCloseAllTabs:
      DCHECK(delegate()->IsForWebApp());
      DCHECK(web_app::HasPinnedHomeTab(this));
      return true;

    default:
      NOTREACHED();
  }
}

void TabStripModel::ExecuteContextMenuCommand(int context_index,
                                              ContextMenuCommand command_id) {
  // This should have been tested by IsContextMenuCommandEnabled.
  CHECK(command_id > CommandFirst && command_id < CommandLast);

  // The tab strip may have been modified while the context menu was open,
  // including closing the tab originally at |context_index|.
  if (!ContainsIndex(context_index)) {
    return;
  }
  switch (command_id) {
    case CommandNewTabToRight: {
      base::RecordAction(UserMetricsAction("TabContextMenu_NewTab"));
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", NewTabTypes::NEW_TAB_CONTEXT_MENU,
                                NewTabTypes::NEW_TAB_ENUM_COUNT);
      delegate()->AddTabAt(GURL(), context_index + 1, true,
                           GetTabGroupForTab(context_index));
      break;
    }

    case CommandReload: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      if (!delegate_->CanReload()) {
        break;
      }
      for (int index : GetIndicesForCommand(context_index)) {
        WebContents* tab = GetWebContentsAt(index);
        if (tab) {
          tab->GetController().Reload(content::ReloadType::NORMAL, true);
        }
      }
      break;
    }

    case CommandDuplicate: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the tabs off as the indices will change as tabs are duplicated.
      std::vector<tabs::TabInterface*> tabs;
      for (int index : indices) {
        tabs.push_back(GetTabAtIndex(index));
      }
      for (const tabs::TabInterface* const tab : tabs) {
        const int index = GetIndexOfTab(tab);
        if (index != -1 && delegate()->CanDuplicateContentsAt(index)) {
          delegate()->DuplicateContentsAt(index);
        }
      }
      break;
    }

    case CommandCloseTab: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      ExecuteCloseTabsByIndicesCommand(
          base::BindRepeating(&TabStripModel::GetIndicesForCommand,
                              base::Unretained(this), context_index),
          /*delete_groups=*/true);
      break;
    }

    case CommandCloseOtherTabs: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      ExecuteCloseTabsByIndicesCommand(
          base::BindRepeating(&TabStripModel::GetIndicesClosedByCommand,
                              base::Unretained(this), context_index,
                              command_id),
          /*delete_groups=*/false);
      break;
    }

    case CommandCloseTabsToRight: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      ExecuteCloseTabsByIndicesCommand(
          base::BindRepeating(&TabStripModel::GetIndicesClosedByCommand,
                              base::Unretained(this), context_index,
                              command_id),
          /*delete_groups=*/false);
      break;
    }

    case CommandSendTabToSelf: {
      send_tab_to_self::ShowBubble(GetWebContentsAt(context_index));
      break;
    }

    case CommandTogglePinned: {
      ReentrancyCheck reentrancy_check(&reentrancy_guard_);

      base::RecordAction(UserMetricsAction("TabContextMenu_TogglePinned"));

      std::vector<int> indices = GetIndicesForCommand(context_index);
      std::vector<tab_groups::TabGroupId> groups_to_delete =
          GetGroupsDestroyedFromRemovingIndices(indices);
      MarkTabGroupsForClosing(groups_to_delete);

      bool pin = WillContextMenuPin(context_index);

      // If there are groups that will be deleted by closing tabs from the
      // context menu, confirm the group deletion first, and then perform the
      // close, either through the callback provided to confirm, or directly if
      // the Confirm is allowing a synchronous delete.
      base::OnceCallback<void()> callback = base::BindOnce(
          [](TabStripModel* model, std::vector<int> indices, bool pin_indices) {
            model->SetTabsPinned(indices, pin_indices);
          },
          base::Unretained(this), indices, pin);

      if (pin && !groups_to_delete.empty()) {
        // If the delegate returns false for confirming the destroy of groups
        // that means that the user needs to make a decision about the
        // destruction first. prevent CloseTabs from being called.
        return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                      std::move(callback));
      } else {
        std::move(callback).Run();
      }

      break;
    }

    case CommandToggleGrouped: {
      if (!group_model_) {
        break;
      }

      std::vector<int> indices = GetIndicesForCommand(context_index);
      if (WillContextMenuGroup(context_index)) {
        std::optional<tab_groups::TabGroupId> new_group_id =
            AddToNewGroup(indices);
        if (new_group_id.has_value()) {
          OpenTabGroupEditor(new_group_id.value());
        }
      } else {
        std::vector<tab_groups::TabGroupId> groups_to_delete =
            GetGroupsDestroyedFromRemovingIndices(indices);
        MarkTabGroupsForClosing(groups_to_delete);

        base::OnceCallback<void()> callback = base::BindOnce(
            &TabStripModel::RemoveFromGroup, base::Unretained(this), indices);
        if (!groups_to_delete.empty()) {
          delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                 std::move(callback));
        } else {
          std::move(callback).Run();
        }
      }
      break;
    }

    case CommandToggleSiteMuted: {
      const bool mute = WillContextMenuMuteSites(context_index);
      if (mute) {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.MuteBy.TabStrip"));
      } else {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.UnmuteBy.TabStrip"));
      }
      SetSitesMuted(GetIndicesForCommand(context_index), mute);
      break;
    }

    case CommandAddToReadLater: {
      base::RecordAction(
          UserMetricsAction("DesktopReadingList.AddItem.FromTabContextMenu"));
      AddToReadLater(GetIndicesForCommand(context_index));
      break;
    }

    case CommandAddToNewGroup: {
      if (!group_model_) {
        break;
      }

      base::RecordAction(UserMetricsAction("TabContextMenu_AddToNewGroup"));
      AddToNewGroupFromContextIndex(context_index);
      break;
    }

    case CommandAddToExistingGroup: {
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingGroupCommand with the correct group later.
      break;
    }

    case CommandAddToSplit: {
      if (!base::FeatureList::IsEnabled(features::kSideBySide)) {
        break;
      }

      std::vector<int> indices = GetIndicesForCommand(context_index);
      CHECK(indices.size() == 1 && indices[0] != active_index());
      indices.push_back(active_index());
      std::ranges::sort(indices);
      AddToNewSplit(indices);
      break;
    }

    case CommandRemoveFromGroup: {
      if (!group_model_) {
        break;
      }

      base::RecordAction(UserMetricsAction("TabContextMenu_RemoveFromGroup"));

      std::vector<int> indices_to_remove = GetIndicesForCommand(context_index);
      std::vector<tab_groups::TabGroupId> groups_to_delete =
          GetGroupsDestroyedFromRemovingIndices(indices_to_remove);
      MarkTabGroupsForClosing(groups_to_delete);

      base::OnceCallback<void()> callback =
          base::BindOnce(&TabStripModel::RemoveFromGroup,
                         base::Unretained(this), indices_to_remove);
      if (!groups_to_delete.empty()) {
        return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                      std::move(callback));
      } else {
        std::move(callback).Run();
      }
      break;
    }

    case CommandMoveToExistingWindow: {
      // Do nothing. The submenu's delegate will invoke
      // ExecuteAddToExistingWindowCommand with the correct window later.
      break;
    }

    case CommandMoveTabsToNewWindow: {
      base::RecordAction(
          UserMetricsAction("TabContextMenu_MoveTabToNewWindow"));

      std::vector<int> indices_to_move = GetIndicesForCommand(context_index);
      std::vector<tab_groups::TabGroupId> groups_to_delete =
          GetGroupsDestroyedFromRemovingIndices(indices_to_move);
      MarkTabGroupsForClosing(groups_to_delete);

      base::OnceCallback<void()> callback =
          base::BindOnce(&TabStripModelDelegate::MoveTabsToNewWindow,
                         base::Unretained(delegate()), indices_to_move);
      if (!groups_to_delete.empty()) {
        return delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                                      std::move(callback));
      } else {
        std::move(callback).Run();
      }
      break;
    }

    case CommandOrganizeTabs: {
      base::RecordAction(UserMetricsAction("TabContextMenu_OrganizeTabs"));
      const Browser* const browser =
          chrome::FindBrowserWithTab(GetWebContentsAt(context_index));
      TabOrganizationService* const service =
          TabOrganizationServiceFactory::GetForProfile(profile_);
      CHECK(service);
      UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
      UMA_HISTOGRAM_BOOLEAN("Tab.Organization.TabContextMenu.Clicked", true);

      service->RestartSessionAndShowUI(
          browser, TabOrganizationEntryPoint::kTabContextMenu,
          GetTabAtIndex(context_index));
      break;
    }

    case CommandCommerceProductSpecifications: {
      // ProductSpecs can only be triggered on non-incognito profiles.
      DCHECK(!profile_->IsIncognitoProfile());
      auto indices = GetIndicesForCommand(context_index);
      auto selected_web_contents =
          GetWebContentsesByIndices(GetIndicesForCommand(context_index));
      auto eligible_urls =
          commerce::GetListOfProductSpecsEligibleUrls(selected_web_contents);
      Browser* browser =
          chrome::FindBrowserWithTab(GetWebContentsAt(context_index));
      chrome::OpenCommerceProductSpecificationsTab(browser, eligible_urls,
                                                   indices.back());
      break;
    }

    case CommandAddToNewComparisonTable: {
      const auto& tab_url =
          GetWebContentsAt(context_index)->GetLastCommittedURL();
      commerce::OpenProductSpecsTabForUrls({tab_url}, this, context_index);

      break;
    }

    case CommandAddToExistingComparisonTable: {
      // Handled by the existing comparison table submenu model.
      break;
    }

    case CommandCopyURL: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CopyURL"));
      delegate()->CopyURL(GetWebContentsAt(context_index));
      break;
    }

    case CommandGoBack: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Back"));
      delegate()->GoBack(GetWebContentsAt(context_index));
      break;
    }

    case CommandCloseAllTabs: {
      // Closes all tabs except the pinned home tab.
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseAllTabs"));

      base::RepeatingCallback<std::vector<int>()> get_indices =
          base::BindRepeating(
              [](base::RepeatingCallback<int()> count) {
                std::vector<int> indices;
                for (int i = count.Run() - 1; i > 0; --i) {
                  indices.push_back(i);
                }
                return indices;
              },
              base::BindRepeating(&TabStripModel::count,
                                  base::Unretained(this)));

      // Because no tabs will remain in the tab strip after this command ensure
      // the groups are also deleted.
      ExecuteCloseTabsByIndicesCommand(get_indices,
                                       /*delete_groups=*/true);
      break;
    }
    case CommandAddToNewGroupFromMenuItem: {
      if (!group_model_) {
        break;
      }

      AddToNewGroupFromContextIndex(context_index);
      break;
    }
    case CommandFirst:
    case CommandAddNote:
    case CommandLast:
      NOTREACHED();
  }
}

void TabStripModel::AddToNewGroupFromContextIndex(int context_index) {
  std::vector<int> indices_to_add = GetIndicesForCommand(context_index);
  std::vector<tab_groups::TabGroupId> groups_to_delete =
      GetGroupsDestroyedFromRemovingIndices(indices_to_add);
  MarkTabGroupsForClosing(groups_to_delete);

  base::OnceCallback<void()> callback = base::BindOnce(
      [](TabStripModel* model, std::vector<int> indices) {
        std::optional<tab_groups::TabGroupId> new_group_id =
            model->AddToNewGroup(indices);
        model->OpenTabGroupEditor(new_group_id.value());
      },
      base::Unretained(this), indices_to_add);

  if (groups_to_delete.empty()) {
    std::move(callback).Run();
  } else {
    delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                           std::move(callback));
  }
}

void TabStripModel::ExecuteAddToExistingGroupCommand(
    int context_index,
    const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }

  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingGroup"));

  if (!ContainsIndex(context_index)) {
    return;
  }

  std::vector<int> indices = GetIndicesForCommand(context_index);

  std::vector<tab_groups::TabGroupId> groups_to_delete =
      GetGroupsDestroyedFromRemovingIndices(indices);
  MarkTabGroupsForClosing(groups_to_delete);

  // If there are no groups to delete OR there is only one group that was found
  // to be deleted, but it is the group that is being added to then the there
  // are no actual deletions occuring. Otherwise the group deletion must be
  // confirmed.
  base::OnceCallback<void()> callback =
      base::BindOnce(&TabStripModel::AddToExistingGroup, base::Unretained(this),
                     indices, group, false);

  if (!groups_to_delete.empty() &&
      !(groups_to_delete.size() == 1 && groups_to_delete[0] == group)) {
    delegate_->OnRemovingAllTabsFromGroups(groups_to_delete,
                                           std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void TabStripModel::ExecuteAddToExistingWindowCommand(int context_index,
                                                      int browser_index) {
  base::RecordAction(UserMetricsAction("TabContextMenu_AddToExistingWindow"));

  if (!ContainsIndex(context_index)) {
    return;
  }
  delegate()->MoveToExistingWindow(GetIndicesForCommand(context_index),
                                   browser_index);
}

std::vector<tab_groups::TabGroupId>
TabStripModel::GetGroupsDestroyedFromRemovingIndices(
    const std::vector<int>& indices) const {
  if (!SupportsTabGroups()) {
    return std::vector<tab_groups::TabGroupId>();
  }

  // Collect indices of tabs in each group.
  std::map<tab_groups::TabGroupId, std::vector<int>> group_indices_map;
  for (const int index : indices) {
    std::optional<tab_groups::TabGroupId> tab_group = GetTabGroupForTab(index);
    if (!tab_group.has_value()) {
      continue;
    }

    if (!group_indices_map.contains(tab_group.value())) {
      group_indices_map.emplace(tab_group.value(), std::vector<int>{});
    }

    group_indices_map[tab_group.value()].emplace_back(index);
  }

  // collect the groups that are going to be destoyed because all tabs are
  // closing.
  std::vector<tab_groups::TabGroupId> groups_to_delete;
  for (const auto& [group, group_indices] : group_indices_map) {
    if (group_model_->GetTabGroup(group)->tab_count() ==
        static_cast<int>(group_indices.size())) {
      groups_to_delete.emplace_back(group);
    }
  }
  return groups_to_delete;
}

void TabStripModel::ExecuteCloseTabsByIndices(
    base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
    uint32_t close_types) {
  ReentrancyCheck reentrancy_check(&reentrancy_guard_);
  const std::vector<int> indices_to_close =
      std::move(get_indices_to_close).Run();
  CloseTabs(GetWebContentsesByIndices(indices_to_close), close_types);
}

void TabStripModel::MarkTabGroupsForClosing(
    const std::vector<tab_groups::TabGroupId> group_ids) {
  for (const tab_groups::TabGroupId& group_id : group_ids) {
    TabGroup* const tab_group = group_model()->GetTabGroup(group_id);
    CHECK(tab_group);
    tab_group->SetGroupIsClosing(true);
  }
}

void TabStripModel::ExecuteCloseTabsByIndicesCommand(
    base::RepeatingCallback<std::vector<int>()> get_indices_to_close,
    bool delete_groups) {
  std::vector<tab_groups::TabGroupId> groups_to_delete =
      GetGroupsDestroyedFromRemovingIndices(get_indices_to_close.Run());
  MarkTabGroupsForClosing(groups_to_delete);

  // If there are groups that will be deleted by closing tabs from the context
  // menu, confirm the group deletion first, and then perform the close, either
  // through the callback provided to confirm, or directly if the Confirm is
  // allowing a synchronous delete. The delegate gets to decide if the
  // groups will be deleted or closed based on where this is a bulk
  // operation.
  base::OnceCallback<void()> close_callback =
      base::BindOnce(&TabStripModel::ExecuteCloseTabsByIndices,
                     base::Unretained(this), get_indices_to_close,
                     TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                         TabCloseTypes::CLOSE_USER_GESTURE);

  if (!groups_to_delete.empty()) {
    // The delegate decides whether to close or delete the groups,
    // potentially prompting the user to decide what action to take.
    // ExecuteCloseTabs may or may not be called as a result.
    delegate_->OnGroupsDestruction(groups_to_delete, std::move(close_callback),
                                   delete_groups);
  } else {
    std::move(close_callback).Run();
  }
}

bool TabStripModel::WillContextMenuMuteSites(int index) {
  return !AreAllSitesMuted(*this, GetIndicesForCommand(index));
}

bool TabStripModel::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  for (size_t i = 0; i < indices.size() && all_pinned; ++i) {
    all_pinned = IsTabPinned(indices[i]);
  }
  return !all_pinned;
}

bool TabStripModel::WillContextMenuGroup(int index) {
  if (!group_model_) {
    return false;
  }

  std::vector<int> indices = GetIndicesForCommand(index);
  DCHECK(!indices.empty());

  // If all tabs are in the same group, then we ungroup, otherwise we group.
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(indices[0]);
  if (!group.has_value()) {
    return true;
  }

  for (size_t i = 1; i < indices.size(); ++i) {
    if (GetTabGroupForTab(indices[i]) != group) {
      return true;
    }
  }
  return false;
}

// static
bool TabStripModel::ContextMenuCommandToBrowserCommand(int cmd_id,
                                                       int* browser_cmd) {
  switch (cmd_id) {
    case CommandReload:
      *browser_cmd = IDC_RELOAD;
      break;
    case CommandDuplicate:
      *browser_cmd = IDC_DUPLICATE_TAB;
      break;
    case CommandSendTabToSelf:
      *browser_cmd = IDC_SEND_TAB_TO_SELF;
      break;
    case CommandCloseTab:
      *browser_cmd = IDC_CLOSE_TAB;
      break;
    case CommandOrganizeTabs:
      *browser_cmd = IDC_ORGANIZE_TABS;
      break;
    default:
      *browser_cmd = 0;
      return false;
  }

  return true;
}

int TabStripModel::GetIndexOfNextWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  CHECK(ContainsIndex(start_index));
  const tabs::TabInterface* opener_tab = GetTabForWebContents(opener);

  // Check tabs after start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    if (GetTabModelAtIndex(i)->opener() == opener_tab) {
      return i;
    }
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    if (GetTabModelAtIndex(i)->opener() == opener_tab) {
      return i;
    }
  }
  return kNoTab;
}

std::optional<int> TabStripModel::GetNextExpandedActiveTab(
    int start_index,
    std::optional<tab_groups::TabGroupId> collapsing_group) const {
  // Check tabs from the start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    std::optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()) &&
         current_group != collapsing_group)) {
      return i;
    }
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    std::optional<tab_groups::TabGroupId> current_group = GetTabGroupForTab(i);
    if (!current_group.has_value() ||
        (!IsGroupCollapsed(current_group.value()) &&
         current_group != collapsing_group)) {
      return i;
    }
  }
  return std::nullopt;
}

void TabStripModel::ForgetAllOpeners() {
  for (int i = 0; i < GetTabCount(); ++i) {
    GetTabModelAtIndex(i)->set_opener(nullptr);
  }
}

void TabStripModel::ForgetOpener(WebContents* contents) {
  const int index = GetIndexOfWebContents(contents);
  CHECK(ContainsIndex(index));
  GetTabModelAtIndex(index)->set_opener(nullptr);
}

void TabStripModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("active_index", active_index());
  dict.Add("tab_count", count());
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

bool TabStripModel::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return delegate_->RunUnloadListenerBeforeClosing(contents);
}

bool TabStripModel::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return contents->NeedToFireBeforeUnloadOrUnloadEvents() ||
         delegate_->ShouldRunUnloadListenerBeforeClosing(contents);
}

int TabStripModel::ConstrainInsertionIndex(int index, bool pinned_tab) const {
  return pinned_tab ? std::clamp(index, 0, IndexOfFirstNonPinnedTab())
                    : std::clamp(index, IndexOfFirstNonPinnedTab(), count());
}

int TabStripModel::ConstrainMoveIndex(int index, bool pinned_tab) const {
  return pinned_tab
             ? std::clamp(index, 0, IndexOfFirstNonPinnedTab() - 1)
             : std::clamp(index, IndexOfFirstNonPinnedTab(), count() - 1);
}

std::vector<int> TabStripModel::GetIndicesForCommand(int index) const {
  if (!IsTabSelected(index)) {
    return {index};
  }
  const ui::ListSelectionModel::SelectedIndices& sel =
      selection_model_.selected_indices();
  return std::vector<int>(sel.begin(), sel.end());
}

std::vector<int> TabStripModel::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  std::vector<int> indices;
  if (!ContainsIndex(index)) {
    return indices;
  }
  DCHECK(id == CommandCloseTabsToRight || id == CommandCloseOtherTabs);
  bool is_selected = IsTabSelected(index);
  int last_unclosed_tab = -1;
  if (id == CommandCloseTabsToRight) {
    last_unclosed_tab =
        is_selected ? *selection_model_.selected_indices().rbegin() : index;
  }

  // NOTE: callers expect the vector to be sorted in descending order.
  for (int i = count() - 1; i > last_unclosed_tab; --i) {
    if (i != index && !IsTabPinned(i) && (!is_selected || !IsTabSelected(i))) {
      indices.push_back(i);
    }
  }
  return indices;
}

bool TabStripModel::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  const GURL& url = contents->GetLastCommittedURL();
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUINewTabHost &&
         contents == GetTabAtIndex(count() - 1)->GetContents() &&
         contents->GetController().GetEntryCount() == 1;
}

std::vector<content::WebContents*> TabStripModel::GetWebContentsesByIndices(
    const std::vector<int>& indices) const {
  std::vector<content::WebContents*> items;
  items.reserve(indices.size());
  for (int index : indices) {
    items.push_back(GetTabAtIndex(index)->GetContents());
  }
  return items;
}

int TabStripModel::InsertTabAtImpl(
    int index,
    std::unique_ptr<tabs::TabModel> tab,
    int add_types,
    std::optional<tab_groups::TabGroupId> group) {
  if (group_model_ && group.has_value()) {
    CHECK(group_model_->ContainsTabGroup(group.value()),
          base::NotFatalUntil::M129);
  }

  delegate()->WillAddWebContents(tab->GetContents());

  const bool active = (add_types & ADD_ACTIVE) != 0 || empty();
  const bool pin = (add_types & ADD_PINNED) != 0;
  index = ConstrainInsertionIndex(index, pin);

  tabs::TabModel* const active_tab_model =
      selection_model_.active().has_value() ? GetTabModelAtIndex(active_index())
                                            : nullptr;

  // If there's already an active tab, and the new tab will become active, send
  // a notification.
  if (active_tab_model && active && !closing_all_) {
    active_tab_model->WillEnterBackground(base::PassKey<TabStripModel>());
  }

  // Have to get the active contents before we monkey with the contents
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  CHECK_EQ(this, tab->owning_model());
  if ((add_types & ADD_INHERIT_OPENER) && active_tab_model) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple openers active at the same time.
      ForgetAllOpeners();
    }
    tab->set_opener(active_tab_model);
  }

  // TODO(gbillock): Ask the modal dialog manager whether the WebContents should
  // be blocked, or just let the modal dialog manager make the blocking call
  // directly and not use this at all.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          tab->GetContents());
  if (manager) {
    tab->set_blocked(manager->IsDialogActive());
  }

  InsertTabAtIndexImpl(std::move(tab), index, group, pin, active);

  return index;
}

int TabStripModel::GetIndexOfTab(const tabs::TabInterface* tab) const {
  if (tab == nullptr) {
    return kNoTab;
  }

  std::optional<size_t> index_of_tab =
      contents_data_->GetIndexOfTabRecursive(tab);
  return index_of_tab.value_or(kNoTab);
}

tabs::TabInterface* TabStripModel::GetTabAtIndex(int index) const {
  return contents_data_->GetTabAtIndexRecursive(index);
}

tabs::TabInterface* TabStripModel::GetTabForWebContents(
    const content::WebContents* contents) const {
  return GetTabAtIndex(GetIndexOfWebContents(contents));
}

void TabStripModel::CloseTabs(base::span<content::WebContents* const> items,
                              uint32_t close_types) {
  std::vector<content::WebContents*> filtered_items;
  for (content::WebContents* contents : items) {
    if (IsTabClosable(contents)) {
      filtered_items.push_back(contents);
    } else {
      for (auto& observer : observers_) {
        observer.TabCloseCancelled(contents);
      }
    }
  }

  if (filtered_items.empty()) {
    return;
  }

  const bool closing_all = static_cast<int>(filtered_items.size()) == count();
  base::WeakPtr<TabStripModel> ref = weak_factory_.GetWeakPtr();
  if (closing_all) {
    for (auto& observer : observers_) {
      observer.WillCloseAllTabs(this);
    }
  }

  DetachNotifications notifications(GetActiveTab(), selection_model_);
  const bool closed_all =
      CloseWebContentses(filtered_items, close_types, &notifications);

  // When unload handler is triggered for all items, we should wait for the
  // result.
  if (!notifications.detached_tab.empty()) {
    SendDetachWebContentsNotifications(&notifications);
  }

  if (!ref) {
    return;
  }
  if (closing_all) {
    // CloseAllTabsStopped is sent with reason kCloseAllCompleted if
    // closed_all; otherwise kCloseAllCanceled is sent.
    for (auto& observer : observers_) {
      observer.CloseAllTabsStopped(
          this, closed_all ? TabStripModelObserver::kCloseAllCompleted
                           : TabStripModelObserver::kCloseAllCanceled);
    }
  }
}

bool TabStripModel::CloseWebContentses(
    base::span<content::WebContents* const> items,
    uint32_t close_types,
    DetachNotifications* notifications) {
  if (items.empty()) {
    return true;
  }

  for (WebContents* contents : items) {
    const int index = GetIndexOfWebContents(contents);
    if (index == active_index() && !closing_all_) {
      GetTabModelAtIndex(active_index())
          ->WillEnterBackground(base::PassKey<TabStripModel>());
    }
    GetTabModelAtIndex(index)->WillDetach(
        base::PassKey<TabStripModel>(),
        tabs::TabInterface::DetachReason::kDelete);
  }

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // browser_shutdown::OnShutdownStarting.
  if (!browser_shutdown::HasShutdownStarted()) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    base::flat_map<content::RenderProcessHost*, size_t> processes;
    for (content::WebContents* contents : items) {
      if (ShouldRunUnloadListenerBeforeClosing(contents)) {
        continue;
      }
      content::RenderProcessHost* process =
          contents->GetPrimaryMainFrame()->GetProcess();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (const auto& pair : processes) {
      pair.first->FastShutdownIfPossible(pair.second, false);
    }
  }

  // We now return to our regularly scheduled shutdown procedure.
  bool closed_all = true;

  // The indices of WebContents prior to any modification of the internal state.
  std::vector<int> original_indices;
  original_indices.resize(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    original_indices[i] = GetIndexOfWebContents(items[i]);
  }

  std::vector<std::unique_ptr<DetachedTab>> detached_tab;
  for (size_t i = 0; i < items.size(); ++i) {
    WebContents* closing_contents = items[i];

    // The index into contents_data_.
    int current_index = GetIndexOfWebContents(closing_contents);
    CHECK_NE(current_index, kNoTab);

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      closing_contents->SetClosedByUserGesture(
          close_types & TabCloseTypes::CLOSE_USER_GESTURE);
    }

    if (RunUnloadListenerBeforeClosing(closing_contents)) {
      closed_all = false;
      continue;
    }

    bool create_historical_tab =
        close_types & TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB;
    auto dt =
        DetachTabImpl(original_indices[i], current_index, create_historical_tab,
                      TabStripModelChange::RemoveReason::kDeleted,
                      tabs::TabInterface::DetachReason::kDelete);
    detached_tab.push_back(std::move(dt));
  }

  for (auto& dt : detached_tab) {
    notifications->detached_tab.push_back(std::move(dt));
  }

  return closed_all;
}

TabStripSelectionChange TabStripModel::SetSelection(
    ui::ListSelectionModel new_model,
    TabStripModelObserver::ChangeReason reason,
    bool triggered_by_other_operation) {
  TabStripSelectionChange selection;
  selection.old_model = selection_model_;
  selection.old_tab = GetActiveTab();
  selection.old_contents = GetActiveWebContents();
  selection.new_model = new_model;
  selection.reason = reason;

  if (selection_model_.active().has_value() && new_model.active().has_value() &&
      selection_model_.active().value() != new_model.active().value()) {
    GetTabModelAtIndex(active_index())
        ->WillEnterBackground(base::PassKey<TabStripModel>());
  }

  // Validate that |new_model| only selects tabs that actually exist.
  CHECK(empty() || new_model.active().has_value(), base::NotFatalUntil::M124);
  CHECK(empty() || ContainsIndex(new_model.active().value()),
        base::NotFatalUntil::M124);
  for (size_t selected_index : new_model.selected_indices()) {
    CHECK(ContainsIndex(selected_index), base::NotFatalUntil::M124);
  }

  // This is done after notifying TabDeactivated() because caller can assume
  // that TabStripModel::active_index() would return the index for
  // |selection.old_contents|.
  selection_model_ = new_model;
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();

  if (!triggered_by_other_operation &&
      (selection.active_tab_changed() || selection.selection_changed())) {
    if (selection.active_tab_changed()) {
      // Start measuring the tab switch compositing time. This must be the first
      // thing in this block so that the start time is saved before any changes
      // that might affect compositing.
      if (selection.new_contents) {
        selection.new_contents->SetTabSwitchStartTime(
            base::TimeTicks::Now(),
            resource_coordinator::ResourceCoordinatorTabHelper::IsLoaded(
                selection.new_contents));
      }

      if (base::FeatureList::IsEnabled(media::kEnableTabMuting)) {
        // Show the in-product help dialog pointing users to the tab mute button
        // if the user backgrounds an audible tab.
        if (selection.old_contents &&
            selection.old_contents->IsCurrentlyAudible()) {
          Browser* browser = chrome::FindBrowserWithTab(selection.old_contents);
          DCHECK(browser);
          browser->window()->MaybeShowFeaturePromo(
              feature_engagement::kIPHTabAudioMutingFeature);
        }
      }
    }
    TabStripModelChange change;
    OnChange(change, selection);
  }

  return selection;
}

void TabStripModel::SelectRelativeTab(TabRelativeDirection direction,
                                      TabStripUserGestureDetails detail) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (empty()) {
    return;
  }

  const int start_index = active_index();
  std::optional<tab_groups::TabGroupId> start_group =
      GetTabGroupForTab(start_index);

  // Ensure the active tab is not in a collapsed group so the while loop can
  // fallback on activating the active tab.
  DCHECK(!start_group.has_value() || !IsGroupCollapsed(start_group.value()));
  const int delta = direction == TabRelativeDirection::kNext ? 1 : -1;
  int index = (start_index + count() + delta) % count();
  std::optional<tab_groups::TabGroupId> group = GetTabGroupForTab(index);
  while (group.has_value() && IsGroupCollapsed(group.value())) {
    index = (index + count() + delta) % count();
    group = GetTabGroupForTab(index);
  }
  ActivateTabAt(index, detail);
}

void TabStripModel::MoveTabRelative(TabRelativeDirection direction) {
  const int offset = direction == TabRelativeDirection::kNext ? 1 : -1;
  const int current_index = active_index();
  std::optional<tab_groups::TabGroupId> current_group =
      GetTabGroupForTab(current_index);

  // Calculate the target index the tab needs to move to.
  const int first_non_pinned_tab_index = IndexOfFirstNonPinnedTab();
  const int first_valid_index =
      IsTabPinned(current_index) ? 0 : first_non_pinned_tab_index;
  const int last_valid_index =
      IsTabPinned(current_index) ? first_non_pinned_tab_index - 1 : count() - 1;
  int target_index =
      std::clamp(current_index + offset, first_valid_index, last_valid_index);

  // If the target index is the same as the current index, then the tab is at a
  // min/max boundary and being moved further in that direction. In that case,
  // the tab could still be ungrouped to move one more slot.
  std::optional<tab_groups::TabGroupId> target_group =
      (target_index == current_index) ? std::nullopt
                                      : GetTabGroupForTab(target_index);

  // If the tab is at a group boundary and the group is expanded, instead of
  // actually moving the tab just change its group membership.
  if (group_model_ && current_group != target_group) {
    if (current_group.has_value()) {
      target_index = current_index;
      target_group = std::nullopt;
    } else if (target_group.has_value()) {
      // If the tab is at a group boundary and the group is collapsed, treat the
      // collapsed group as a tab and find the next available slot for the tab
      // to move to.
      const TabGroup* group = group_model_->GetTabGroup(target_group.value());
      if (group->visual_data()->is_collapsed()) {
        const gfx::Range tabs_in_group = group->ListTabs();
        target_index = direction == TabRelativeDirection::kNext
                           ? tabs_in_group.end() - 1
                           : tabs_in_group.start();
        target_group = std::nullopt;
      } else {
        target_index = current_index;
      }
    }
  }
  // TODO: this needs to be updated for multi-selection.
  MoveTabToIndexImpl(current_index, target_index, target_group,
                     IsTabPinned(target_index), true);
}

std::pair<std::optional<int>, std::optional<int>>
TabStripModel::GetAdjacentTabsAfterSelectedMove(
    base::PassKey<TabDragController>,
    int destination_index) {
  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const std::vector<int> pinned_selected_indices = GetSelectedPinnedTabs();
  const std::vector<int> unpinned_selected_indices = GetSelectedUnpinnedTabs();
  std::pair<std::optional<int>, std::optional<int>> adjacent_tabs(std::nullopt,
                                                                  std::nullopt);

  // If `unpinned_selected_indices` is empty there are no adjacent tabs.
  if (unpinned_selected_indices.empty()) {
    return adjacent_tabs;
  }

  // The index should be clamped between the first possible unpinned tab
  // position and the end of the tabstrip.
  const int first_unpinned_selected_dst_index = std::clamp(
      destination_index + static_cast<int>(pinned_selected_indices.size()),
      pinned_tab_count,
      count() - static_cast<int>(unpinned_selected_indices.size()));

  // Get the left adjacent if the first unpinned selected is not in the start of
  // the unpinned container.
  if (first_unpinned_selected_dst_index > pinned_tab_count) {
    int non_selected_index = pinned_tab_count;
    for (int i = pinned_tab_count; i < count(); ++i) {
      if (!IsTabSelected(i)) {
        if (non_selected_index == first_unpinned_selected_dst_index - 1) {
          adjacent_tabs.first = i;
          break;
        }
        ++non_selected_index;
      }
    }
  } else {
    // Maybe the left adjacent is the last pinned tab.
    const int is_last_pinned_tab_selected =
        !pinned_selected_indices.empty() &&
        (destination_index + static_cast<int>(pinned_selected_indices.size()) -
             1 >=
         pinned_tab_count - 1);
    for (int i = pinned_tab_count - 1; i >= 0; i--) {
      if (IsTabSelected(i) == is_last_pinned_tab_selected) {
        adjacent_tabs.first = i;
        break;
      }
    }
  }

  const int last_unpinned_selected_dst_index =
      first_unpinned_selected_dst_index + unpinned_selected_indices.size() - 1;

  // Get the right adjacent if the last unpinned selected is not in the end of
  // the tabstrip.
  if (last_unpinned_selected_dst_index < count() - 1) {
    int non_selected_index = count() - 1;
    for (int i = count() - 1; i >= pinned_tab_count; i--) {
      if (!IsTabSelected(i)) {
        if (non_selected_index == last_unpinned_selected_dst_index + 1) {
          adjacent_tabs.second = i;
          break;
        }
        --non_selected_index;
      }
    }
  }

  return adjacent_tabs;
}

std::vector<int> TabStripModel::GetSelectedPinnedTabs() {
  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model_.selected_indices();

  std::vector<int> indices;

  for (int selected_index : selected_indices) {
    if (selected_index < pinned_tab_count) {
      indices.push_back(selected_index);
    } else {
      // Since selected_indices are sorted, no more pinned tabs will be found
      break;
    }
  }

  return indices;
}

std::vector<int> TabStripModel::GetSelectedUnpinnedTabs() {
  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model_.selected_indices();

  std::vector<int> indices;

  for (int selected_index : base::Reversed(selected_indices)) {
    if (selected_index >= pinned_tab_count) {
      // Insert to the start so it is in ascending order.
      indices.insert(indices.begin(), selected_index);
    } else {
      // Since selected_indices are sorted, no more unpinned tabs will be found
      break;
    }
  }

  return indices;
}

void TabStripModel::AddToNewGroupImpl(
    const std::vector<int>& indices,
    const tab_groups::TabGroupId& new_group,
    std::optional<tab_groups::TabGroupVisualData> visual_data) {
  if (!group_model_) {
    return;
  }

  DCHECK([&]() {
    for (int i = 0; i < GetTabCount(); ++i) {
      const tabs::TabInterface* const tab = GetTabAtIndex(i);
      if (tab->GetGroup() == new_group) {
        return false;
      }
    }
    return true;
  }());

  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          new_group,
          tab_groups::TabGroupVisualData(
              std::u16string(),
              group_model_->GetNextColor(base::PassKey<TabStripModel>())),
          this);
  group_model_->AddTabGroup(group_collection->GetTabGroup(),
                            base::PassKey<TabStripModel>());
  contents_data_->CreateTabGroup(std::move(group_collection));

  // Find a destination for the first tab that's not pinned or inside another
  // group. We will stack the rest of the tabs up to its right.
  int destination_index = -1;
  for (int i = indices[0]; i < count(); i++) {
    const int destination_candidate = i + 1;

    // Grouping at the end of the tabstrip is always valid.
    if (!ContainsIndex(destination_candidate)) {
      destination_index = destination_candidate;
      break;
    }

    // Grouping in the middle of pinned tabs is never valid.
    if (IsTabPinned(destination_candidate)) {
      continue;
    }

    // Otherwise, grouping is valid if the destination is not in the middle of a
    // different group.
    std::optional<tab_groups::TabGroupId> destination_group =
        GetTabGroupForTab(destination_candidate);
    if (!destination_group.has_value() ||
        destination_group != GetTabGroupForTab(indices[0])) {
      destination_index = destination_candidate;
      break;
    }
  }

  MoveTabsAndSetGroupImpl(indices, destination_index, new_group);

  // Excluding the active tab, deselect all tabs being added to the group.
  // See crbug/1301846 for more info.
  const gfx::Range tab_indices =
      group_model()->GetTabGroup(new_group)->ListTabs();
  for (auto index = tab_indices.start(); index < tab_indices.end(); ++index) {
    if (active_index() != static_cast<int>(index) && IsTabSelected(index)) {
      ToggleSelectionAt(index);
    }
  }
}

void TabStripModel::AddToExistingGroupImpl(const std::vector<int>& indices,
                                           const tab_groups::TabGroupId& group,
                                           const bool add_to_end) {
  if (!group_model_) {
    return;
  }

  // Do nothing if the "existing" group can't be found. This would only happen
  // if the existing group is closed programmatically while the user is
  // interacting with the UI - e.g. if a group close operation is started by an
  // extension while the user clicks "Add to existing group" in the context
  // menu.
  // If this happens, the browser should not crash. So here we just make it a
  // no-op, since we don't want to create unintended side effects in this rare
  // corner case.
  if (!group_model_->ContainsTabGroup(group)) {
    return;
  }

  const TabGroup* group_object = group_model_->GetTabGroup(group);
  int first_tab_in_group = group_object->GetFirstTab().value();
  int last_tab_in_group = group_object->GetLastTab().value();

  // Split |new_indices| into |tabs_left_of_group| and |tabs_right_of_group| to
  // be moved to proper destination index. Directly set the group for indices
  // that are inside the group.
  std::vector<int> tabs_left_of_group;
  std::vector<int> tabs_right_of_group;
  for (int index : indices) {
    if (index < first_tab_in_group) {
      tabs_left_of_group.push_back(index);
    } else if (index > last_tab_in_group) {
      tabs_right_of_group.push_back(index);
    }
  }

  if (add_to_end) {
    std::vector<int> all_tabs = tabs_left_of_group;
    all_tabs.insert(all_tabs.end(), tabs_right_of_group.begin(),
                    tabs_right_of_group.end());
    MoveTabsAndSetGroupImpl(all_tabs, last_tab_in_group + 1, group);
  } else {
    MoveTabsAndSetGroupImpl(tabs_left_of_group, first_tab_in_group, group);
    MoveTabsAndSetGroupImpl(tabs_right_of_group, last_tab_in_group + 1, group);
  }
}

void TabStripModel::MoveTabsAndSetGroupImpl(
    const std::vector<int>& indices,
    int destination_index,
    std::optional<tab_groups::TabGroupId> group) {
  if (!group_model_) {
    return;
  }

  // Some tabs will need to be moved to the right, some to the left. We need to
  // handle those separately. First, move tabs to the right, starting with the
  // rightmost tab so we don't cause other tabs we are about to move to shift.
  int numTabsMovingRight = 0;
  for (size_t i = 0; i < indices.size() && indices[i] < destination_index;
       i++) {
    numTabsMovingRight++;
  }
  for (int i = numTabsMovingRight - 1; i >= 0; i--) {
    MoveTabToIndexImpl(indices[i], destination_index - numTabsMovingRight + i,
                       group, false, false);
  }

  // Collect indices for tabs moving to the left.
  std::vector<int> move_left_indices;
  for (size_t i = numTabsMovingRight; i < indices.size(); i++) {
    move_left_indices.push_back(indices[i]);
  }

  // Move tabs to the left, starting with the leftmost tab.
  for (size_t i = 0; i < move_left_indices.size(); i++) {
    MoveTabToIndexImpl(move_left_indices[i], destination_index + i, group,
                       false, false);
  }
}

void TabStripModel::AddToReadLaterImpl(const std::vector<int>& indices) {
  for (int index : indices) {
    delegate_->AddToReadLater(GetWebContentsAt(index));
  }
}

void TabStripModel::InsertTabAtIndexImpl(
    std::unique_ptr<tabs::TabModel> tab_model,
    int index,
    std::optional<tab_groups::TabGroupId> group,
    bool pin,
    bool active) {
  tabs::TabModel* const tab_ptr = tab_model.get();

  tabs::TabInterface* old_active_tab = GetActiveTab();
  contents_data_->AddTabRecursive(std::move(tab_model), index, group, pin);

  // Update selection model and send the notification.
  selection_model_.IncrementFrom(index);

  // Start computing selection change after updating the indices in
  // `selection_model_`.
  TabStripSelectionChange selection(old_active_tab, selection_model_);
  if (active) {
    ui::ListSelectionModel new_model = selection_model_;
    new_model.SetSelectedIndex(index);
    SetSelection(std::move(new_model),
                 TabStripModelObserver::CHANGE_REASON_NONE,
                 /*triggered_by_other_operation=*/true);
  }

  ValidateTabStripModel();

  tab_ptr->DidInsert(base::PassKey<TabStripModel>());

  selection.new_model = selection_model_;
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();
  TabStripModelChange::Insert insert;
  insert.contents.push_back({tab_ptr, tab_ptr->GetContents(), index});
  TabStripModelChange change(std::move(insert));
  OnChange(change, selection);

  if (group_model_ && group.has_value()) {
    TabGroupStateChanged(index, tab_ptr, std::nullopt, group);
  }
}

std::unique_ptr<tabs::TabModel> TabStripModel::RemoveTabFromIndexImpl(
    int index) {
  tabs::TabInterface* tab = GetTabAtIndex(index);
  const std::optional<tab_groups::TabGroupId> old_group = tab->GetGroup();

  std::optional<int> next_selected_index = DetermineNewSelectedIndex(index);

  // Remove the tab.
  std::unique_ptr<tabs::TabModel> old_data =
      contents_data_->RemoveTabAtIndexRecursive(index);

  if (empty()) {
    selection_model_.Clear();
  } else {
    int old_active = active_index();
    selection_model_.DecrementFrom(index);
    ui::ListSelectionModel old_model;
    old_model = selection_model_;
    if (index == old_active) {
      if (!selection_model_.empty()) {
        // The active tab was removed, but there is still something selected.
        // Move the active and anchor to the first selected index.
        selection_model_.set_active(
            *selection_model_.selected_indices().begin());
        selection_model_.set_anchor(selection_model_.active());
      } else {
        // The active tab was removed and nothing is selected. Reset the
        // selection and send out notification.
        selection_model_.SetSelectedIndex(next_selected_index.value());
      }
    }
  }

  if (group_model_ && old_group) {
    TabGroupStateChanged(index, tab, old_group, std::nullopt);
  }

  ValidateTabStripModel();

  return old_data;
}

void TabStripModel::MoveTabToIndexImpl(
    int initial_index,
    int final_index,
    const std::optional<tab_groups::TabGroupId> group,
    bool pin,
    bool select_after_move) {
  CHECK(ContainsIndex(initial_index));
  CHECK_LT(initial_index, count());
  CHECK_LT(final_index, count());

  tabs::TabInterface* const tab = GetTabAtIndex(initial_index);
  const bool initial_pinned_state = tab->IsPinned();
  const std::optional<tab_groups::TabGroupId> initial_group = tab->GetGroup();

  if (initial_index != final_index) {
    FixOpeners(initial_index);
  }

  TabStripSelectionChange selection(GetActiveTab(), selection_model_);
  contents_data_->MoveTabRecursive(initial_index, final_index, group, pin);

  UpdateSelectionModelForMove(initial_index, final_index, select_after_move);

  selection.new_model = selection_model_;
  selection.new_tab = GetActiveTab();
  selection.new_contents = GetActiveWebContents();

  // Send all the notifications.
  if (initial_index != final_index) {
    SendMoveNotificationForTab(initial_index, final_index, tab, selection);
  }

  if (initial_pinned_state != tab->IsPinned()) {
    for (auto& observer : observers_) {
      observer.TabPinnedStateChanged(this, tab->GetContents(), final_index);
    }
  }

  if (group_model_) {
    if (initial_group != tab->GetGroup()) {
      TabGroupStateChanged(final_index, tab, initial_group, tab->GetGroup());
    }
  }

  ValidateTabStripModel();
}

void TabStripModel::MoveTabsToIndexImpl(
    const std::vector<int>& tab_indices,
    int destination_index,
    const std::optional<tab_groups::TabGroupId> group) {
  if (tab_indices.empty()) {
    return;
  }

  const int pinned_tab_count = IndexOfFirstNonPinnedTab();
  const bool pin = IsTabPinned(tab_indices[0]);
  const bool all_tabs_pinned = std::all_of(
      tab_indices.begin(), tab_indices.end(),
      [pinned_tab_count](int index) { return index < pinned_tab_count; });
  const bool all_tabs_unpinned = std::all_of(
      tab_indices.begin(), tab_indices.end(),
      [pinned_tab_count](int index) { return index >= pinned_tab_count; });

  CHECK(all_tabs_pinned || all_tabs_unpinned);
  CHECK(std::ranges::is_sorted(tab_indices));

  const std::vector<MoveNotification> notifications =
      PrepareTabsToMoveToIndex(tab_indices, destination_index);

  // Update `contents_data`.
  contents_data_->MoveTabsRecursive(tab_indices, destination_index, group, pin);

  UpdateSelectionModelForMoves(tab_indices, destination_index);

  for (auto notification : notifications) {
    const int final_index = GetIndexOfTab(notification.tab);
    tabs::TabInterface* tab = GetTabAtIndex(final_index);
    if (notification.initial_index != final_index) {
      SendMoveNotificationForTab(notification.initial_index, final_index, tab,
                                 notification.selection_change);
    }

    if (group_model_) {
      if (notification.intial_group != tab->GetGroup()) {
        TabGroupStateChanged(final_index, tab, notification.intial_group,
                             tab->GetGroup());
      }
    }
  }

  ValidateTabStripModel();
}

void TabStripModel::TabGroupStateChanged(
    int index,
    tabs::TabInterface* tab,
    const std::optional<tab_groups::TabGroupId> initial_group,
    const std::optional<tab_groups::TabGroupId> new_group) {
  if (!group_model_) {
    return;
  }

  if (initial_group == new_group) {
    return;
  }

  if (initial_group.has_value()) {
    // Send the observation
    for (auto& observer : observers_) {
      observer.TabGroupedStateChanged(this, initial_group, std::nullopt, tab,
                                      index);
    }
    // Update the group model.
    RemoveTabFromGroupModel(initial_group.value());
  }

  if (new_group.has_value()) {
    // Update the group model.
    AddTabToGroupModel(new_group.value());

    // Send the observation
    for (auto& observer : observers_) {
      observer.TabGroupedStateChanged(this, std::nullopt, new_group, tab,
                                      index);
    }
  }
}

void TabStripModel::RemoveTabFromGroupModel(
    const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }

  TabGroup* tab_group = group_model_->GetTabGroup(group);
  tab_group->RemoveTab();

  if (tab_group->IsEmpty()) {
    group_model_->RemoveTabGroup(group, base::PassKey<TabStripModel>());
    contents_data_->CloseDetachedTabGroup(group);
  }
}

void TabStripModel::AddTabToGroupModel(const tab_groups::TabGroupId& group) {
  if (!group_model_) {
    return;
  }
  TabGroup* tab_group = group_model_->GetTabGroup(group);
  tab_group->AddTab();
}

void TabStripModel::ValidateTabStripModel() {
  if (empty()) {
    return;
  }

  CHECK(selection_model_.active().has_value() &&
        GetTabAtIndex(selection_model_.active().value()));

#if DCHECK_IS_ON()
  // Check if the selected tab indices are valid.
  const ui::ListSelectionModel::SelectedIndices& selected_indices =
      selection_model_.selected_indices();

  for (auto selection : selected_indices) {
    DCHECK(GetTabAtIndex(selection));
  }
#endif

  contents_data_->ValidateData();
}

void TabStripModel::SendMoveNotificationForTab(
    int index,
    int to_position,
    tabs::TabInterface* tab,
    TabStripSelectionChange& selection_change) {
  TabStripModelChange::Move move;
  move.tab = tab;
  move.contents = tab->GetContents();
  move.from_index = index;
  move.to_index = to_position;
  TabStripModelChange change(move);
  OnChange(change, selection_change);
}

void TabStripModel::UpdateSelectionModelForMove(int initial_index,
                                                int final_index,
                                                bool select_after_move) {
  if (initial_index == final_index) {
    return;
  }

  selection_model_.Move(initial_index, final_index, 1);
  if (!selection_model_.IsSelected(final_index) && select_after_move) {
    selection_model_.SetSelectedIndex(final_index);
  }
}

void TabStripModel::UpdateSelectionModelForMoves(
    const std::vector<int>& tab_indices,
    int destination_index) {
  const std::vector<std::pair<int, int>> moved_indices =
      CalculateIncrementalTabMoves(tab_indices, destination_index);

  for (std::pair<int, int> move : moved_indices) {
    if (move.first != move.second) {
      selection_model_.Move(move.first, move.second, 1);
    }
  }
}

std::vector<std::pair<int, int>> TabStripModel::CalculateIncrementalTabMoves(
    const std::vector<int>& tab_indices,
    int destination_index) const {
  std::vector<std::pair<int, int>> source_and_target_indices_to_move_left;
  std::vector<std::pair<int, int>> source_and_target_indices_to_move_right;

  // We want a sequence of moves that moves each tab directly from its
  // initial index to its final index. This is possible if and only if
  // every move maintains the same relative order of the moving tabs.
  // We do this by splitting the tabs based on which direction they're
  // moving, then moving them in the correct order within each group.
  int tab_destination_index = destination_index;
  for (int source_index : tab_indices) {
    if (source_index < tab_destination_index) {
      source_and_target_indices_to_move_right.emplace_back(
          source_index, tab_destination_index++);
    } else {
      source_and_target_indices_to_move_left.emplace_back(
          source_index, tab_destination_index++);
    }
  }

  std::vector<std::pair<int, int>> moved_indices;
  std::reverse(source_and_target_indices_to_move_right.begin(),
               source_and_target_indices_to_move_right.end());

  std::copy(source_and_target_indices_to_move_right.begin(),
            source_and_target_indices_to_move_right.end(),
            std::back_inserter(moved_indices));

  std::copy(source_and_target_indices_to_move_left.begin(),
            source_and_target_indices_to_move_left.end(),
            std::back_inserter(moved_indices));

  return moved_indices;
}

std::vector<TabStripModel::MoveNotification>
TabStripModel::PrepareTabsToMoveToIndex(const std::vector<int>& tab_indices,
                                        int destination_index) {
  const std::vector<std::pair<int, int>> moved_indices =
      CalculateIncrementalTabMoves(tab_indices, destination_index);
  std::vector<MoveNotification> notifications;

  ui::ListSelectionModel selection_model_copy = selection_model_;
  for (std::pair<int, int> move : moved_indices) {
    if (move.first != move.second) {
      FixOpeners(move.first);
    }

    // Update the `selection_model_copy_`
    TabStripSelectionChange selection;
    if (move.first == move.second) {
      selection = TabStripSelectionChange();
    } else {
      selection = TabStripSelectionChange(GetActiveTab(), selection_model_copy);
      selection_model_copy.Move(move.first, move.second, 1);
      selection.new_model = selection_model_copy;
    }

    const tabs::TabInterface* const tab = GetTabAtIndex(move.first);
    const MoveNotification notification = {move.first, tab->GetGroup(), tab,
                                           selection};
    notifications.push_back(notification);
  }

  return notifications;
}

void TabStripModel::SetTabsPinned(std::vector<int> indices, bool pinned) {
  if (!pinned) {
    std::reverse(indices.begin(), indices.end());
  }

  for (int index : indices) {
    if (IsTabPinned(index) == pinned) {
      continue;
    }

    const int non_pinned_tab_index = IndexOfFirstNonPinnedTab();
    MoveTabToIndexImpl(index,
                       pinned ? non_pinned_tab_index : non_pinned_tab_index - 1,
                       std::nullopt, pinned, false);
  }
}

// Sets the sound content setting for each site at the |indices|.
void TabStripModel::SetSitesMuted(const std::vector<int>& indices,
                                  bool mute) const {
  for (int tab_index : indices) {
    content::WebContents* web_contents = GetWebContentsAt(tab_index);
    GURL url = web_contents->GetLastCommittedURL();

    // `GetLastCommittedURL` could return an empty URL if no navigation has
    // occurred yet.
    if (url.is_empty()) {
      continue;
    }

    if (url.SchemeIs(content::kChromeUIScheme)) {
      // chrome:// URLs don't have content settings but can be muted, so just
      // mute the WebContents.
      SetTabAudioMuted(web_contents, mute,
                       TabMutedReason::CONTENT_SETTING_CHROME, std::string());
    } else {
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      HostContentSettingsMap* map =
          HostContentSettingsMapFactory::GetForProfile(profile);
      ContentSetting setting =
          mute ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;

      // The goal is to only add the site URL to the exception list if
      // the request behavior differs from the default value or if there is an
      // existing less specific rule (i.e. wildcards) in the exception list.
      if (!profile->IsIncognitoProfile()) {
        // Using default setting value below clears the setting from the
        // exception list for the site URL if it exists.
        map->SetContentSettingDefaultScope(url, url, ContentSettingsType::SOUND,
                                           CONTENT_SETTING_DEFAULT);

        // If the current setting matches the desired setting after clearing the
        // site URL from the exception list we can simply skip otherwise we
        // will add the site URL to the exception list.
        if (setting ==
            map->GetContentSetting(url, url, ContentSettingsType::SOUND)) {
          continue;
        }
      }
      // Adds the site URL to the exception list for the setting.
      map->SetContentSettingDefaultScope(url, url, ContentSettingsType::SOUND,
                                         setting);
    }
  }
}

void TabStripModel::FixOpeners(int index) {
  tabs::TabModel* old_tab = GetTabModelAtIndex(index);
  tabs::TabInterface* new_opener = old_tab ? old_tab->opener() : nullptr;

  for (int i = 0; i < GetTabCount(); i++) {
    tabs::TabModel* tab = GetTabModelAtIndex(i);
    if (tab->opener() != old_tab) {
      continue;
    }

    // Ensure a tab isn't its own opener.
    tab->set_opener(new_opener == tab ? nullptr : new_opener);
  }

  // Sanity check that none of the tabs' openers refer |old_tab| or
  // themselves.
  DCHECK([&]() {
    for (int i = 0; i < GetTabCount(); ++i) {
      const tabs::TabModel* const tab = GetTabModelAtIndex(i);
      if (tab->opener() == old_tab || tab->opener() == tab) {
        return false;
      }
    }
    return true;
  }());
}

std::optional<tab_groups::TabGroupId> TabStripModel::GetGroupToAssign(
    int index,
    int to_position) {
  CHECK(ContainsIndex(index), base::NotFatalUntil::M129);
  CHECK(ContainsIndex(to_position), base::NotFatalUntil::M129);

  tabs::TabInterface* tab_to_move = GetTabAtIndex(index);

  if (!group_model_) {
    return std::nullopt;
  }

  std::optional<tab_groups::TabGroupId> new_left_group;
  std::optional<tab_groups::TabGroupId> new_right_group;

  if (to_position > index) {
    new_left_group = GetTabGroupForTab(to_position);
    new_right_group = GetTabGroupForTab(to_position + 1);
  } else if (to_position < index) {
    new_left_group = GetTabGroupForTab(to_position - 1);
    new_right_group = GetTabGroupForTab(to_position);
  }

  if (tab_to_move->GetGroup() != new_left_group &&
      tab_to_move->GetGroup() != new_right_group) {
    if (new_left_group == new_right_group && new_left_group.has_value()) {
      // The tab is in the middle of an existing group, so add it to that group.
      return new_left_group;
    } else if (tab_to_move->GetGroup().has_value() &&
               group_model_->GetTabGroup(tab_to_move->GetGroup().value())
                       ->tab_count() > 1) {
      // The tab is between groups and its group is non-contiguous, so clear
      // this tab's group.
      return std::nullopt;
    }
  }

  return tab_to_move->GetGroup();
}

int TabStripModel::GetTabIndexAfterClosing(int index,
                                           int removing_index) const {
  if (removing_index < index) {
    index = std::max(0, index - 1);
  }
  return index;
}

void TabStripModel::OnActiveTabChanged(
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed() || empty()) {
    return;
  }

  const tabs::TabInterface* const old_tab = selection.old_tab;
  const tabs::TabInterface* const new_tab = selection.new_tab;
  const tabs::TabInterface* old_opener = nullptr;
  int reason = selection.reason;

  if (old_tab) {
    const int index = GetIndexOfTab(old_tab);
    if (index != TabStripModel::kNoTab) {
      // When switching away from a tab, the tab preview system may want to
      // capture an updated preview image. This must be done before any changes
      // are made to the old contents, and while the contents are still visible.
      //
      // It's possible this could be done with a separate TabStripModelObserver,
      // but then it would be possible for a different observer to jump in front
      // and modify the WebContents, so for now, do it here.
      auto* const thumbnail_helper =
          ThumbnailTabHelper::FromWebContents(old_tab->GetContents());
      if (thumbnail_helper) {
        thumbnail_helper->CaptureThumbnailOnTabBackgrounded();
      }

      old_opener = GetOpenerOfTabAt(index);

      // Forget the opener relationship if it needs to be reset whenever the
      // active tab changes (see comment in TabStripModel::AddWebContents, where
      // the flag is set).
      if (GetTabModelAtIndex(index)->reset_opener_on_active_tab_change()) {
        ForgetOpener(old_tab->GetContents());
      }
    }
  }
  DCHECK(selection.new_model.active().has_value());
  const tabs::TabInterface* const new_opener =
      GetOpenerOfTabAt(selection.new_model.active().value());

  if ((reason & TabStripModelObserver::CHANGE_REASON_USER_GESTURE) &&
      new_opener != old_opener &&
      ((old_tab == nullptr && new_opener == nullptr) ||
       new_opener != old_tab) &&
      ((new_tab == nullptr && old_opener == nullptr) ||
       old_opener != new_tab)) {
    ForgetAllOpeners();
  }
}

bool TabStripModel::PolicyAllowsTabClosing(
    content::WebContents* contents) const {
  if (!contents) {
    return true;
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebContents(contents);
  // Can be null if there is no tab helper or app id.
  const webapps::AppId* app_id = web_app::WebAppTabHelper::GetAppId(contents);
  if (!app_id) {
    return true;
  }

  return !delegate()->IsForWebApp() ||
         !provider->policy_manager().IsPreventCloseEnabled(*app_id);
}

int TabStripModel::DetermineInsertionIndex(ui::PageTransition transition,
                                           bool foreground) {
  int tab_count = count();
  if (!tab_count) {
    return 0;
  }

  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) &&
      active_index() != -1) {
    if (foreground) {
      // If the page was opened in the foreground by a link click in another
      // tab, insert it adjacent to the tab that opened that link.
      return active_index() + 1;
    }
    content::WebContents* const opener = GetActiveWebContents();
    // Figure out the last tab opened by the current tab.
    const int index = GetIndexOfLastWebContentsOpenedBy(opener, active_index());
    // If no such tab exists, simply place next to the current tab.
    if (index == TabStripModel::kNoTab) {
      return active_index() + 1;
    }

    // Normally we'd add the tab immediately after the most recent tab
    // associated with `opener`. However, if there is a group discontinuity
    // between the active tab and where we'd like to place the tab, we'll place
    // it just before the discontinuity instead (see crbug.com/1246421).
    const auto opener_group = GetTabGroupForTab(active_index());
    for (int i = active_index() + 1; i <= index; ++i) {
      // Insert before the first tab that differs in group.
      if (GetTabGroupForTab(i) != opener_group) {
        return i;
      }
    }
    // If there is no discontinuity, add after the last tab already associated
    // with the opener.
    return index + 1;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return count();
}

void TabStripModel::GroupCloseStopped(const tab_groups::TabGroupId& group) {
  delegate_->GroupCloseStopped(group);

  gfx::Range tabs_in_group = group_model_->GetTabGroup(group)->ListTabs();
  std::vector<int> ungrouping_tabs_indices;
  ungrouping_tabs_indices.reserve(tabs_in_group.length());
  for (uint32_t i = tabs_in_group.start(); i < tabs_in_group.end(); i++) {
    ungrouping_tabs_indices.push_back(i);
  }
  RemoveFromGroup(ungrouping_tabs_indices);
}

std::optional<int> TabStripModel::DetermineNewSelectedIndex(
    int removing_index) const {
  DCHECK(ContainsIndex(removing_index));

  if (removing_index != active_index()) {
    return std::nullopt;
  }

  if (selection_model().size() > 1) {
    return std::nullopt;
  }

  tabs::TabInterface* tab_to_remove_opener = GetOpenerOfTabAt(removing_index);
  // First see if the index being removed has any "child" tabs. If it does, we
  // want to select the first that child opened, not the next tab opened by the
  // removed tab.
  tabs::TabInterface* removed_tab = GetTabAtIndex(removing_index);
  // The parent opener should never be the same as the controller being removed.
  DCHECK(tab_to_remove_opener != removed_tab);
  int index = GetIndexOfNextWebContentsOpenedBy(removed_tab->GetContents(),
                                                removing_index);
  if (index != TabStripModel::kNoTab && !IsTabCollapsed(index)) {
    return GetTabIndexAfterClosing(index, removing_index);
  }

  if (tab_to_remove_opener && tab_to_remove_opener->GetContents()) {
    // If the tab has an opener, shift selection to the next tab with the same
    // opener.
    index = GetIndexOfNextWebContentsOpenedBy(
        tab_to_remove_opener->GetContents(), removing_index);
    if (index != TabStripModel::kNoTab && !IsTabCollapsed(index)) {
      return GetTabIndexAfterClosing(index, removing_index);
    }

    // If we can't find another tab with the same opener, fall back to the
    // opener itself.
    index = GetIndexOfTab(tab_to_remove_opener);
    if (index != TabStripModel::kNoTab && !IsTabCollapsed(index)) {
      return GetTabIndexAfterClosing(index, removing_index);
    }
  }

  // If closing a grouped tab, return a tab that is still in the group, if any.
  const std::optional<tab_groups::TabGroupId> current_group =
      GetTabGroupForTab(removing_index);
  if (current_group.has_value()) {
    // Match the default behavior below: prefer the tab to the right.
    const std::optional<tab_groups::TabGroupId> right_group =
        GetTabGroupForTab(removing_index + 1);
    if (current_group == right_group) {
      return removing_index;
    }

    const std::optional<tab_groups::TabGroupId> left_group =
        GetTabGroupForTab(removing_index - 1);
    if (current_group == left_group) {
      return removing_index - 1;
    }
  }

  // At this point, the tab detaching is either not inside a group, or the last
  // tab in the group. If there are any tabs in a not collapsed group,
  // |GetNextExpandedActiveTab()| will return the index of that tab.
  std::optional<int> next_available =
      GetNextExpandedActiveTab(removing_index, std::nullopt);
  if (next_available.has_value()) {
    return GetTabIndexAfterClosing(next_available.value(), removing_index);
  }

  // By default, return the tab on the right, unless this is the last tab.
  // Reaching this point means there are no other tabs in an uncollapsed group.
  // The tab at the specified index will become automatically expanded by the
  // caller.
  if (removing_index >= (count() - 1)) {
    return removing_index - 1;
  }

  return removing_index;
}

TabStripModel::ScopedTabStripModalUIImpl::ScopedTabStripModalUIImpl(
    TabStripModel* model)
    : model_(model) {
  CHECK(!model_->showing_modal_ui_);
  model_->showing_modal_ui_ = true;
}

TabStripModel::ScopedTabStripModalUIImpl::~ScopedTabStripModalUIImpl() {
  model_->showing_modal_ui_ = false;
}
