// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_focus_cycler.h"

#include "ash/focus_cycler.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"

namespace ash {

ShelfFocusCycler::ShelfFocusCycler(Shelf* shelf) : shelf_(shelf) {}

void ShelfFocusCycler::FocusOut(bool reverse, SourceView source_view) {
  // TODO(manucornet): Once the non-views-based shelf is gone, make this a
  // simple cycling logic instead of a long switch.
  switch (source_view) {
    case SourceView::kShelfNavigationView:
      if (reverse)
        FocusStatusArea(reverse);
      else
        FocusShelf(reverse);
      break;
    case SourceView::kShelfView:
      if (reverse)
        FocusNavigation(reverse);
      else
        FocusStatusArea(reverse);
      break;
    case SourceView::kStatusAreaView: {
      // If we are using a views-based shelf:
      // * If we're in an active session, either focus the navigation widget
      //   (going forward) or the shelf (reverse).
      // * Otherwise (login/lock screen, OOBE), bring focus to the shelf only
      //   if we're going in reverse; if we're going forward, let the system
      //   tray focus observers focus the lock/login view.
      if (shelf_->shelf_widget()->GetLoginShelfView()->GetVisible() &&
          (!reverse ||
           (!shelf_->shelf_widget()->GetLoginShelfView()->IsFocusable() &&
            reverse))) {
        // Login/lock screen or OOBE.
        Shell::Get()->system_tray_notifier()->NotifyFocusOut(reverse);
      } else if (reverse) {
        FocusShelf(reverse);
      } else {
        FocusNavigation(reverse);
      }
      break;
    }
  }
}

void ShelfFocusCycler::FocusNavigation(bool last_element) {
  ShelfNavigationWidget* navigation_widget = shelf_->navigation_widget();
  if (!navigation_widget->GetHomeButton() &&
      !navigation_widget->GetBackButton()) {
    FocusOut(last_element, SourceView::kShelfNavigationView);
    return;
  }
  navigation_widget->PrepareForGettingFocus(last_element);
  Shell::Get()->focus_cycler()->FocusWidget(navigation_widget);
}

void ShelfFocusCycler::FocusShelf(bool last_element) {
  if (shelf_->shelf_widget()->GetLoginShelfView()->GetVisible()) {
    // TODO(https://crbug.com/1343114): refactor the code below after the login
    // shelf widget is ready.

    if (features::IsUseLoginShelfWidgetEnabled()) {
      LoginShelfWidget* login_shelf_widget = shelf_->login_shelf_widget();
      login_shelf_widget->SetDefaultLastFocusableChild(last_element);
      Shell::Get()->focus_cycler()->FocusWidget(login_shelf_widget);
    } else {
      ShelfWidget* shelf_widget = shelf_->shelf_widget();
      shelf_widget->set_default_last_focusable_child(last_element);
      Shell::Get()->focus_cycler()->FocusWidget(shelf_widget);
    }
  } else {
    HotseatWidget* hotseat_widget = shelf_->hotseat_widget();
    hotseat_widget->scrollable_shelf_view()->set_default_last_focusable_child(
        last_element);
    Shell::Get()->focus_cycler()->FocusWidget(hotseat_widget);
  }
}

void ShelfFocusCycler::FocusStatusArea(bool last_element) {
  StatusAreaWidget* status_area_widget = shelf_->GetStatusAreaWidget();
  status_area_widget->status_area_widget_delegate()
      ->set_default_last_focusable_child(last_element);
  Shell::Get()->focus_cycler()->FocusWidget(status_area_widget);
}

}  // namespace ash
