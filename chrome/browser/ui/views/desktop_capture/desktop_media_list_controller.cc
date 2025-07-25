// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/metadata/metadata_impl_macros.h"

BEGIN_METADATA(DesktopMediaListController, ListView, views::View)
END_METADATA

DesktopMediaListController::DesktopMediaListController(
    DesktopMediaPickerDialogView* parent,
    std::unique_ptr<DesktopMediaList> media_list)
    : dialog_(parent),
      media_list_(std::move(media_list)),
      auto_select_tab_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutoSelectTabCaptureSourceByTitle)),
      auto_select_source_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutoSelectDesktopCaptureSource)),
      auto_accept_this_tab_capture_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoAccept)),
      auto_reject_this_tab_capture_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoReject)) {
  DCHECK(dialog_);
  DCHECK(media_list_);
}

DesktopMediaListController::~DesktopMediaListController() = default;

std::unique_ptr<views::View> DesktopMediaListController::CreateView(
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style,
    const std::u16string& accessible_name) {
  DCHECK(!view_);

  auto view = std::make_unique<DesktopMediaListView>(
      this, generic_style, single_style, accessible_name);
  view_ = view.get();
  view_observations_.AddObservation(view_.get());
  return view;
}

std::unique_ptr<views::View> DesktopMediaListController::CreateTabListView(
    const std::u16string& accessible_name) {
  DCHECK(!view_);

  auto view = std::make_unique<DesktopMediaTabList>(this, accessible_name);
  view_ = view.get();
  view_observations_.AddObservation(view_.get());
  return view;
}

void DesktopMediaListController::StartUpdating(
    content::DesktopMediaID dialog_window_id) {
  dialog_window_id_ = dialog_window_id;
  // Defer calling StartUpdating on media lists with a delegated source list
  // until the first time they are focused.
  if (!media_list_->IsSourceListDelegated())
    StartUpdatingInternal();
}

void DesktopMediaListController::StartUpdatingInternal() {
  is_updating_ = true;
  media_list_->SetViewDialogWindowId(dialog_window_id_);
  media_list_->StartUpdating(this);
}

void DesktopMediaListController::FocusView() {
  if (view_)
    view_->RequestFocus();

  if (media_list_->IsSourceListDelegated() && !is_updating_)
    StartUpdatingInternal();

  media_list_->FocusList();
}

void DesktopMediaListController::HideView() {
  media_list_->HideList();
}

bool DesktopMediaListController::SupportsReselectButton() const {
  // Only DelegatedSourceLists support the notion of reslecting.
  return media_list_->IsSourceListDelegated();
}

void DesktopMediaListController::SetCanReselect(bool can_reselect) {
  if (can_reselect_ == can_reselect)
    return;
  can_reselect_ = can_reselect;
  dialog_->OnCanReselectChanged(this);
}

void DesktopMediaListController::OnReselectRequested() {
  // Before we clear the delegated source list selection (which may be async),
  // clear our own selection.
  ClearSelection();

  // Clearing the selection is enough to force the list to reappear the next
  // time that it is focused (or now if it is currently focused).
  media_list_->ClearDelegatedSourceListSelection();

  // Once we've called Reselect, we don't want to call it again until we get a
  // new selection.
  SetCanReselect(false);
}

absl::optional<content::DesktopMediaID>
DesktopMediaListController::GetSelection() const {
  return view_ ? view_->GetSelection() : absl::nullopt;
}

void DesktopMediaListController::ClearSelection() {
  if (view_)
    view_->ClearSelection();
}

void DesktopMediaListController::OnSourceListLayoutChanged() {
  dialog_->OnSourceListLayoutChanged();
}

void DesktopMediaListController::OnSourceSelectionChanged() {
  dialog_->OnSelectionChanged();
}

void DesktopMediaListController::AcceptSource() {
  if (GetSelection())
    dialog_->AcceptSource();
}

void DesktopMediaListController::AcceptSpecificSource(
    content::DesktopMediaID source) {
  dialog_->AcceptSpecificSource(source);
}

void DesktopMediaListController::Reject() {
  dialog_->Reject();
}

size_t DesktopMediaListController::GetSourceCount() const {
  return base::checked_cast<size_t>(media_list_->GetSourceCount());
}

const DesktopMediaList::Source& DesktopMediaListController::GetSource(
    size_t index) const {
  return media_list_->GetSource(index);
}

void DesktopMediaListController::SetThumbnailSize(const gfx::Size& size) {
  media_list_->SetThumbnailSize(size);
}

void DesktopMediaListController::SetPreviewedSource(
    const absl::optional<content::DesktopMediaID>& id) {
  media_list_->SetPreviewedSource(id);
}

base::WeakPtr<DesktopMediaListController>
DesktopMediaListController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DesktopMediaListController::OnSourceAdded(int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceAdded(
        base::checked_cast<size_t>(index));
  }

  const DesktopMediaList::Source& source = GetSource(index);

  if (ShouldAutoAccept(source)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopMediaListController::AcceptSpecificSource,
                       weak_factory_.GetWeakPtr(), source.id));
  } else if (ShouldAutoReject(source)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DesktopMediaListController::Reject,
                                  weak_factory_.GetWeakPtr()));
  }
}

void DesktopMediaListController::OnSourceRemoved(int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceRemoved(
        base::checked_cast<size_t>(index));
  }
}

void DesktopMediaListController::OnSourceMoved(int old_index, int new_index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceMoved(
        base::checked_cast<size_t>(old_index),
        base::checked_cast<size_t>(new_index));
  }
}
void DesktopMediaListController::OnSourceNameChanged(int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceNameChanged(
        base::checked_cast<size_t>(index));
  }
}
void DesktopMediaListController::OnSourceThumbnailChanged(int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceThumbnailChanged(
        base::checked_cast<size_t>(index));
  }
}

void DesktopMediaListController::OnSourcePreviewChanged(size_t index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourcePreviewChanged(index);
  }
}

void DesktopMediaListController::OnDelegatedSourceListSelection() {
  DCHECK(media_list_->IsSourceListDelegated());
  if (view_) {
    view_->GetSourceListListener()->OnDelegatedSourceListSelection();
  }

  SetCanReselect(true);
}

void DesktopMediaListController::OnDelegatedSourceListDismissed() {
  DCHECK(media_list_->IsSourceListDelegated());
  dialog_->OnDelegatedSourceListDismissed();
}

void DesktopMediaListController::OnViewIsDeleting(views::View* view) {
  view_observations_.RemoveObservation(view);
  view_ = nullptr;
}

bool DesktopMediaListController::ShouldAutoAccept(
    const DesktopMediaList::Source& source) const {
  if (media_list_->GetMediaListType() == DesktopMediaList::Type::kCurrentTab) {
    return auto_accept_this_tab_capture_;
  } else if (media_list_->GetMediaListType() ==
                 DesktopMediaList::Type::kWebContents &&
             !auto_select_tab_.empty() &&
             source.name.find(base::ASCIIToUTF16(auto_select_tab_)) !=
                 std::u16string::npos) {
    return true;
  }

  return (!auto_select_source_.empty() &&
          source.name.find(base::ASCIIToUTF16(auto_select_source_)) !=
              std::u16string::npos);
}

bool DesktopMediaListController::ShouldAutoReject(
    const DesktopMediaList::Source& source) const {
  if (media_list_->GetMediaListType() == DesktopMediaList::Type::kCurrentTab) {
    return auto_reject_this_tab_capture_;
  }
  return false;
}
