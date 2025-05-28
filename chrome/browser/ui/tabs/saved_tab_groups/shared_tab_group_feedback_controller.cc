
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/shared_tab_group_feedback_controller.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "ui/views/view_class_properties.h"

namespace tab_groups {

SharedTabGroupFeedbackController::SharedTabGroupFeedbackController(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      tab_group_sync_service_(SavedTabGroupUtils::GetServiceForProfile(
          browser_view_->GetProfile())) {
  CHECK(tab_group_sync_service_);
  tab_group_sync_observer_.Observe(tab_group_sync_service_);
  browser_view_->browser()->GetTabStripModel()->AddObserver(this);

  active_tab_change_subscriptions_.push_back(
      browser_view_->browser()->RegisterActiveTabDidChange(
          base::BindRepeating(&SharedTabGroupFeedbackController::MaybeShowIPH,
                              base::Unretained(this))));
}

SharedTabGroupFeedbackController::~SharedTabGroupFeedbackController() = default;

void SharedTabGroupFeedbackController::Init() {
  MaybeShowFeedbackActionInToolbar();
}

void SharedTabGroupFeedbackController::TearDown() {
  browser_view_->browser()->GetTabStripModel()->RemoveObserver(this);
}

void SharedTabGroupFeedbackController::UpdateFeedbackButtonVisibility(
    bool should_show_button) {
  PinnedToolbarActionsContainer* container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();
  if (!container) {
    // Can be null when dragging a tab / group into a new window.
    return;
  }

  if (should_show_button ==
      container->IsActionPoppedOut(kActionSendSharedTabGroupFeedback)) {
    // Do nothing if the button is already in the correct state.
    return;
  }

  container->ShowActionEphemerallyInToolbar(kActionSendSharedTabGroupFeedback,
                                            should_show_button);

  if (should_show_button) {
    PinnedActionToolbarButton* button =
        container->GetButtonFor(kActionSendSharedTabGroupFeedback);
    CHECK(button);

    // Add the ElementIdentifier so the IPH system can find the button.
    button->SetProperty(views::kElementIdentifierKey,
                        kSharedTabGroupFeedbackElementId);
  }
}

void SharedTabGroupFeedbackController::MaybeShowFeedbackActionInToolbar() {
  // Show the feedback button if there are any shared tab groups open.
  bool should_show_button = false;
  std::vector<TabGroupId> local_group_ids = browser_view_->browser()
                                                ->tab_strip_model()
                                                ->group_model()
                                                ->ListTabGroups();
  for (const TabGroupId& local_group_id : local_group_ids) {
    std::optional<SavedTabGroup> saved_group =
        tab_group_sync_service_->GetGroup(local_group_id);
    if (saved_group && saved_group->is_shared_tab_group()) {
      should_show_button = true;
      break;
    }
  }

  UpdateFeedbackButtonVisibility(should_show_button);
}

void SharedTabGroupFeedbackController::MaybeShowIPH(
    BrowserWindowInterface* browser_window_interface) {
  tabs::TabInterface* tab_interface =
      browser_window_interface->GetActiveTabInterface();
  std::optional<TabGroupId> group_id = tab_interface->GetGroup();
  if (!group_id) {
    return;
  }

  std::optional<SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group_id.value());
  if (!saved_group) {
    return;
  }

  if (!saved_group->is_shared_tab_group()) {
    return;
  }

  BrowserUserEducationInterface* browser_user_education_interface =
      browser_window_interface->GetUserEducationInterface();
  CHECK(browser_user_education_interface);

  browser_user_education_interface->MaybeShowFeaturePromo(
      feature_engagement::kIPHTabGroupsSharedTabFeedbackFeature);
}

void SharedTabGroupFeedbackController::OnInitialized() {
  Init();
}

void SharedTabGroupFeedbackController::OnTabGroupUpdated(
    const SavedTabGroup& group,
    TriggerSource source) {
  MaybeShowFeedbackActionInToolbar();
}

void SharedTabGroupFeedbackController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  MaybeShowFeedbackActionInToolbar();
}

}  // namespace tab_groups
