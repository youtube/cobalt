// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_model.h"

#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace tabs {

namespace {

// This class exists to allow consumers to look up a TabInterface from an
// instance of WebContents. This is necessary while transitioning features to
// use TabInterface and TabModel instead of WebContents.
class TabLookupFromWebContents
    : public content::WebContentsUserData<TabLookupFromWebContents> {
 public:
  ~TabLookupFromWebContents() override = default;

  TabModel* model() { return model_; }

 private:
  friend WebContentsUserData;
  TabLookupFromWebContents(content::WebContents* contents, TabModel* model)
      : content::WebContentsUserData<TabLookupFromWebContents>(*contents),
        model_(model) {}

  // Semantically owns this class.
  raw_ptr<TabModel> model_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabLookupFromWebContents);

}  // namespace

TabModel::TabModel(std::unique_ptr<content::WebContents> contents,
                   TabStripModel* soon_to_be_owning_model)
    : contents_owned_(std::move(contents)),
      contents_(contents_owned_.get()),
      soon_to_be_owning_model_(soon_to_be_owning_model) {
  TabLookupFromWebContents::CreateForWebContents(contents_, this);

  // TODO(https://crbug.com/362038317): Tab-helpers should be created in exactly
  // one place, which is here.
  TabHelpers::AttachTabHelpers(contents_);
  tab_features_ = TabFeatures::CreateTabFeatures();

  // Once tabs are pulled into a standalone module, TabFeatures and its
  // initialization will need to be delegated back to the main module.
  tab_features_->Init(
      *this, Profile::FromBrowserContext(contents_->GetBrowserContext()));
}

TabModel::~TabModel() {
  contents_->RemoveUserData(TabLookupFromWebContents::UserDataKey());
}

void TabModel::OnAddedToModel(TabStripModel* owning_model) {
  soon_to_be_owning_model_ = nullptr;
  CHECK(!owning_model_);
  CHECK(owning_model);
  owning_model_ = owning_model;
  owning_model_->AddObserver(this);

  // Being detached is equivalent to being in the background. So after
  // detachment, if the tab is in the foreground, we must send a notification.
  if (IsActivated()) {
    did_enter_foreground_callback_list_.Notify(this);
  }

  // Being detached is equivalent to being in the background. So after
  // detachment, if the tab is in the foreground, we must send a notification.
  if (IsVisible()) {
    did_become_visible_callback_list_.Notify(this);
  }
}

void TabModel::OnRemovedFromModel() {
  // Going through each field here:
  // Keep `contents_`, obviously.

  owning_model_->RemoveObserver(this);
  owning_model_ = nullptr;

  // Opener stuff doesn't make sense to transfer between browsers.
  opener_ = nullptr;
  reset_opener_on_active_tab_change_ = false;

  // Pinned state, blocked state, and group membership are all preserved, at
  // least in some cases, but for now let's leave that to the existing
  // mechanisms that were handling that.
  // TODO(tbergquist): Decide whether to stick with this approach or not.
  pinned_ = false;
  blocked_ = false;
  group_ = std::nullopt;
}

TabCollection* TabModel::GetParentCollection(
    base::PassKey<TabCollection>) const {
  return parent_collection_;
}

void TabModel::OnReparented(TabCollection* parent,
                            base::PassKey<TabCollection>) {
  parent_collection_ = parent;
}

void TabModel::SetPinned(bool pinned) {
  if (pinned_ == pinned) {
    return;
  }

  pinned_ = pinned;
  pinned_state_changed_callback_list_.Notify(this, pinned_);
}

void TabModel::SetGroup(std::optional<tab_groups::TabGroupId> group) {
  if (group_ == group) {
    return;
  }

  group_ = group;
  group_changed_callback_list_.Notify(this, group_);
}

void TabModel::WillEnterBackground(base::PassKey<TabStripModel>) {
  will_enter_background_callback_list_.Notify(this);
  will_become_hidden_callback_list_.Notify(this);
}

void TabModel::WillDetach(base::PassKey<TabStripModel>,
                          tabs::TabInterface::DetachReason reason) {
  will_detach_callback_list_.Notify(this, reason);
}

void TabModel::DidInsert(base::PassKey<TabStripModel>) {
  did_insert_callback_list_.Notify(this);
}

content::WebContents* TabModel::GetContents() const {
  return contents_;
}

base::CallbackListSubscription TabModel::RegisterWillDiscardContents(
    TabInterface::WillDiscardContentsCallback callback) {
  return will_discard_contents_callback_list_.Add(std::move(callback));
}

bool TabModel::IsActivated() const {
  return GetModelForTabInterface()->GetActiveTab() == this;
}

base::CallbackListSubscription TabModel::RegisterDidActivate(
    TabInterface::DidActivateCallback callback) {
  return did_enter_foreground_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillDeactivate(
    TabInterface::WillDeactivateCallback callback) {
  return will_enter_background_callback_list_.Add(std::move(callback));
}

bool TabModel::IsVisible() const {
  return GetModelForTabInterface()->GetActiveTab() == this;
}

base::CallbackListSubscription TabModel::RegisterDidBecomeVisible(
    TabInterface::DidBecomeVisibleCallback callback) {
  return did_become_visible_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillBecomeHidden(
    TabInterface::WillBecomeHiddenCallback callback) {
  return will_become_hidden_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterWillDetach(
    TabInterface::WillDetach callback) {
  return will_detach_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterDidInsert(
    TabInterface::DidInsertCallback callback) {
  return did_insert_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterPinnedStateChanged(
    TabInterface::PinnedStateChangedCallback callback) {
  return pinned_state_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabModel::RegisterGroupChanged(
    TabInterface::GroupChangedCallback callback) {
  return group_changed_callback_list_.Add(std::move(callback));
}

bool TabModel::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<ScopedTabModalUI> TabModel::ShowModalUI() {
  return std::make_unique<ScopedTabModalUIImpl>(this);
}

base::CallbackListSubscription TabModel::RegisterModalUIChanged(
    TabInterface::TabInterfaceCallback callback) {
  return modal_ui_changed_callback_list_.Add(std::move(callback));
}

bool TabModel::IsInNormalWindow() const {
  return GetModelForTabInterface()->delegate()->IsNormalWindow();
}

BrowserWindowInterface* TabModel::GetBrowserWindowInterface() {
  return GetModelForTabInterface()->delegate()->GetBrowserWindowInterface();
}

tabs::TabFeatures* TabModel::GetTabFeatures() {
  return tab_features_.get();
}

bool TabModel::IsPinned() const {
  return pinned_;
}

bool TabModel::IsSplit() const {
  return split_;
}

std::optional<tab_groups::TabGroupId> TabModel::GetGroup() const {
  return group_;
}

bool TabModel::ShouldAcceptMouseEventsWhileWindowInactive() const {
  return accept_input_when_window_inactive_ > 0;
}

std::unique_ptr<ScopedAcceptMouseEventsWhileWindowInactive>
TabModel::AcceptMouseEventsWhileWindowInactive() {
  return std::make_unique<ScopedAcceptMouseEventsWhileWindowInactiveImpl>(this);
}

void TabModel::Close() {
  auto* window_interface = GetBrowserWindowInterface();
  auto* tab_strip = window_interface->GetTabStripModel();
  CHECK(tab_strip);
  const int tab_idx = tab_strip->GetIndexOfTab(this);
  CHECK(tab_idx != TabStripModel::kNoTab);
  tab_strip->CloseWebContentsAt(tab_idx, TabCloseTypes::CLOSE_NONE);
}

void TabModel::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.new_contents == GetContents()) {
    did_enter_foreground_callback_list_.Notify(this);
    did_become_visible_callback_list_.Notify(this);
    return;
  }
}

TabStripModel* TabModel::GetModelForTabInterface() const {
  CHECK(soon_to_be_owning_model_ || owning_model_);
  return soon_to_be_owning_model_ ? soon_to_be_owning_model_ : owning_model_;
}

TabModel::ScopedTabModalUIImpl::ScopedTabModalUIImpl(TabModel* tab)
    : tab_(tab->weak_factory_.GetWeakPtr()) {
  CHECK(!tab_->showing_modal_ui_);
  tab_->showing_modal_ui_ = true;
  tab_->modal_ui_changed_callback_list_.Notify(tab_.get());
}

TabModel::ScopedTabModalUIImpl::~ScopedTabModalUIImpl() {
  if (tab_) {
    tab_->showing_modal_ui_ = false;
    tab_->modal_ui_changed_callback_list_.Notify(tab_.get());
  }
}

TabModel::ScopedAcceptMouseEventsWhileWindowInactiveImpl::
    ScopedAcceptMouseEventsWhileWindowInactiveImpl(TabModel* tab)
    : tab_(tab->weak_factory_.GetWeakPtr()) {
  ++tab_->accept_input_when_window_inactive_;
}

TabModel::ScopedAcceptMouseEventsWhileWindowInactiveImpl::
    ~ScopedAcceptMouseEventsWhileWindowInactiveImpl() {
  if (tab_) {
    --tab_->accept_input_when_window_inactive_;
  }
}

void TabModel::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("web_contents", GetContents());
  dict.Add("pinned", IsPinned());
  dict.Add("split", IsSplit());
  dict.Add("blocked", blocked());
}

std::unique_ptr<content::WebContents> TabModel::DiscardContents(
    std::unique_ptr<content::WebContents> contents) {
  will_discard_contents_callback_list_.Notify(this, contents_, contents.get());
  contents_->RemoveUserData(TabLookupFromWebContents::UserDataKey());
  std::unique_ptr<content::WebContents> old_contents =
      std::move(contents_owned_);
  contents_owned_ = std::move(contents);
  contents_ = contents_owned_.get();
  TabLookupFromWebContents::CreateForWebContents(contents_, this);
  return old_contents;
}

// static
std::unique_ptr<content::WebContents> TabModel::DestroyAndTakeWebContents(
    std::unique_ptr<TabModel> tab_model) {
  std::unique_ptr<content::WebContents> contents =
      std::move(tab_model->contents_owned_);
  return contents;
}

void TabModel::DestroyTabFeatures() {
  tab_features_.reset();
}

// static
TabInterface* TabInterface::GetFromContents(
    content::WebContents* web_contents) {
  return TabLookupFromWebContents::FromWebContents(web_contents)->model();
}

// static
TabInterface* TabInterface::MaybeGetFromContents(
    content::WebContents* web_contents) {
  TabLookupFromWebContents* lookup =
      TabLookupFromWebContents::FromWebContents(web_contents);
  if (!lookup) {
    return nullptr;
  }
  return lookup->model();
}

}  // namespace tabs
