// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

using content::DesktopMediaID;

DesktopMediaSourceViewStyle::DesktopMediaSourceViewStyle(
    const DesktopMediaSourceViewStyle& style) = default;

DesktopMediaSourceViewStyle::DesktopMediaSourceViewStyle(
    int columns,
    const gfx::Size& item_size,
    const gfx::Rect& icon_rect,
    const gfx::Rect& label_rect,
    gfx::HorizontalAlignment text_alignment,
    const gfx::Rect& image_rect,
    int focus_rectangle_inset)
    : columns(columns),
      item_size(item_size),
      icon_rect(icon_rect),
      label_rect(label_rect),
      text_alignment(text_alignment),
      image_rect(image_rect),
      focus_rectangle_inset(focus_rectangle_inset) {}

DesktopMediaSourceView::DesktopMediaSourceView(
    DesktopMediaListView* parent,
    DesktopMediaID source_id,
    DesktopMediaSourceViewStyle style)
    : parent_(parent),
      source_id_(source_id),
      selected_(false) {
  AddChildView(icon_view_.get());
  AddChildView(image_view_.get());
  AddChildView(label_.get());
  icon_view_->SetCanProcessEventsWithinSubtree(false);
  image_view_->SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetStyle(style);
  views::FocusRing::Install(this);
}

DesktopMediaSourceView::~DesktopMediaSourceView() {}

void DesktopMediaSourceView::SetName(const std::u16string& name) {
  label_->SetText(name);
}

void DesktopMediaSourceView::SetThumbnail(const gfx::ImageSkia& thumbnail) {
  image_view_->SetImage(thumbnail);
}

void DesktopMediaSourceView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
}

void DesktopMediaSourceView::SetSelected(bool selected) {
  if (selected == selected_)
    return;
  selected_ = selected;

  if (selected) {
    // Unselect all other sources.
    Views neighbours;
    parent()->GetViewsInGroup(GetGroup(), &neighbours);
    for (auto i(neighbours.begin()); i != neighbours.end(); ++i) {
      if (*i != this) {
        DCHECK(views::IsViewClass<DesktopMediaSourceView>(*i));
        DesktopMediaSourceView* source_view =
            static_cast<DesktopMediaSourceView*>(*i);
        source_view->SetSelected(false);
      }
    }

    image_view_->SetBackground(views::CreateSolidBackground(
        GetColorProvider()->GetColor(ui::kColorMenuItemBackgroundSelected)));
    label_->SetFontList(label_->font_list().Derive(0, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::BOLD));
    parent_->OnSelectionChanged();
  } else {
    image_view_->SetBackground(nullptr);
    label_->SetFontList(label_->font_list().Derive(0, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::NORMAL));
  }

  OnPropertyChanged(&selected_, views::kPropertyEffectsPaint);
}

void DesktopMediaSourceView::SetStyle(DesktopMediaSourceViewStyle style) {
  image_view_->SetBoundsRect(style.image_rect);
  icon_view_->SetBoundsRect(style.icon_rect);
  icon_view_->SetImageSize(style.icon_rect.size());
  label_->SetBoundsRect(style.label_rect);
  label_->SetHorizontalAlignment(style.text_alignment);
}

bool DesktopMediaSourceView::GetSelected() const {
  return selected_;
}

void DesktopMediaSourceView::ClearSelection() {
  if (!GetSelected())
    return;
  SetSelected(false);
  parent_->OnSelectionChanged();
}

views::View* DesktopMediaSourceView::GetSelectedViewForGroup(int group) {
  Views neighbours;
  parent()->GetViewsInGroup(group, &neighbours);
  if (neighbours.empty())
    return nullptr;

  for (auto i(neighbours.begin()); i != neighbours.end(); ++i) {
    DCHECK(views::IsViewClass<DesktopMediaSourceView>(*i));
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(*i);
    if (source_view->selected_)
      return source_view;
  }
  return nullptr;
}

bool DesktopMediaSourceView::IsGroupFocusTraversable() const {
  return false;
}

void DesktopMediaSourceView::OnFocus() {
  View::OnFocus();
  SetSelected(true);
  ScrollRectToVisible(gfx::Rect(size()));
}

bool DesktopMediaSourceView::OnMousePressed(const ui::MouseEvent& event) {
  RequestFocus();
  return true;
}

void DesktopMediaSourceView::OnGestureEvent(ui::GestureEvent* event) {
  // Detect tap gesture using ET_GESTURE_TAP_DOWN so the view also gets focused
  // on the long tap (when the tap gesture starts).
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    RequestFocus();
    event->SetHandled();
  }
}

void DesktopMediaSourceView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetNameChecked(
      label_->GetText().empty()
          ? l10n_util::GetStringUTF16(
                IDS_DESKTOP_MEDIA_SOURCE_EMPTY_ACCESSIBLE_NAME)
          : label_->GetText());
}

BEGIN_METADATA(DesktopMediaSourceView, views::View)
ADD_PROPERTY_METADATA(bool, Selected)
END_METADATA
