// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#include <dispatch/dispatch.h>

#include "base/i18n/rtl.h"
#include "base/mac/mac_util.h"
#import "base/message_loop/message_pump_mac.h"
#include "base/numerics/safe_conversions.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_config.h"
#import "ui/views/controls/menu/menu_controller_cocoa_delegate_impl.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr CGFloat kNativeCheckmarkWidth = 18;
constexpr CGFloat kNativeMenuItemHeight = 18;

}  // namespace

namespace views::internal {
namespace {

// Returns the first item in |menu_controller|'s menu that will be checked.
NSMenuItem* FirstCheckedItem(MenuControllerCocoa* menu_controller) {
  for (NSMenuItem* item in [[menu_controller menu] itemArray]) {
    if ([menu_controller model]->IsItemCheckedAt(
            base::checked_cast<size_t>([item tag]))) {
      return item;
    }
  }
  return nil;
}

// Places a temporary, hidden NSView at |screen_bounds| within |window|. Used
// with -[NSMenu popUpMenuPositioningItem:atLocation:inView:] to position the
// menu for a combobox. The caller must remove the returned NSView from its
// superview when the menu is closed.
base::scoped_nsobject<NSView> CreateMenuAnchorView(
    NSWindow* window,
    const gfx::Rect& screen_bounds,
    NSMenuItem* checked_item,
    CGFloat actual_menu_width,
    MenuAnchorPosition position) {
  NSRect rect = gfx::ScreenRectToNSRect(screen_bounds);
  rect = [window convertRectFromScreen:rect];
  rect = [[window contentView] convertRect:rect fromView:nil];

  // If there's no checked item (e.g. Combobox::STYLE_ACTION), NSMenu will
  // anchor at the top left of the frame. Action buttons should anchor below.
  if (!checked_item) {
    rect.size.height = 0;
    if (base::i18n::IsRTL())
      rect.origin.x += rect.size.width;
  } else {
    // To ensure a consistent anchoring that's vertically centered in the
    // bounds, fix the height to be the same as a menu item.
    rect.origin.y = NSMidY(rect) - kNativeMenuItemHeight / 2;
    rect.size.height = kNativeMenuItemHeight;
    if (base::i18n::IsRTL()) {
      // The Views menu controller flips the MenuAnchorPosition value from left
      // to right in RTL. NSMenu does this automatically: the menu opens to the
      // left of the anchor, but AppKit doesn't account for the anchor width.
      // So the width needs to be added to anchor at the right of the view.
      // Note the checkmark width is not also added - it doesn't quite line up
      // the text. A Yosemite NSComboBox doesn't line up in RTL either: just
      // adding the width is a good match for the native behavior.
      rect.origin.x += rect.size.width;
    } else {
      rect.origin.x -= kNativeCheckmarkWidth;
    }
  }

  // When the actual menu width is larger than the anchor, right alignment
  // should be respected.
  if (actual_menu_width > rect.size.width &&
      position == views::MenuAnchorPosition::kTopRight &&
      !base::i18n::IsRTL()) {
    int width_diff = actual_menu_width - rect.size.width;
    rect.origin.x -= width_diff;
  }
  // A plain NSView will anchor below rather than "over", so use an NSButton.
  base::scoped_nsobject<NSView> anchor_view(
      [[NSButton alloc] initWithFrame:rect]);
  [anchor_view setHidden:YES];
  [[window contentView] addSubview:anchor_view];
  return anchor_view;
}

// Returns an appropriate event (with a location) suitable for showing a context
// menu. Uses [NSApp currentEvent] if it's a non-nil mouse click event,
// otherwise creates an autoreleased dummy event located at |anchor|.
NSEvent* EventForPositioningContextMenu(const gfx::Rect& anchor,
                                        NSWindow* window) {
  NSEvent* event = [NSApp currentEvent];
  switch ([event type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
      return event;
    default:
      break;
  }
  NSPoint location_in_window = [window
      convertPointFromScreen:gfx::ScreenPointToNSPoint(anchor.CenterPoint())];
  return [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                            location:location_in_window
                       modifierFlags:0
                           timestamp:0
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:0];
}

}  // namespace

// static
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32_t run_types,
    base::RepeatingClosure on_menu_closed_callback) {
  if ((run_types & MenuRunner::CONTEXT_MENU) &&
      !(run_types & (MenuRunner::IS_NESTED))) {
    return new MenuRunnerImplCocoa(menu_model,
                                   std::move(on_menu_closed_callback));
  }
  return new MenuRunnerImplAdapter(menu_model,
                                   std::move(on_menu_closed_callback));
}

MenuRunnerImplCocoa::MenuRunnerImplCocoa(
    ui::MenuModel* menu,
    base::RepeatingClosure on_menu_closed_callback)
    : on_menu_closed_callback_(std::move(on_menu_closed_callback)) {
  menu_delegate_.reset([[MenuControllerCocoaDelegateImpl alloc] init]);
  menu_controller_.reset([[MenuControllerCocoa alloc]
               initWithModel:menu
                    delegate:menu_delegate_.get()
      useWithPopUpButtonCell:NO]);
}

bool MenuRunnerImplCocoa::IsRunning() const {
  return running_;
}

void MenuRunnerImplCocoa::Release() {
  if (IsRunning()) {
    if (delete_after_run_)
      return;  // We already canceled.

    delete_after_run_ = true;

    // Reset |menu_controller_| to ensure it clears itself as a delegate to
    // prevent NSMenu attempting to access the weak pointer to the ui::MenuModel
    // it holds (which is not owned by |this|). Toolkit-views menus use
    // MenuRunnerImpl::empty_delegate_ to handle this case.
    [menu_controller_ cancel];
    menu_controller_.reset();
  } else {
    delete this;
  }
}

void MenuRunnerImplCocoa::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    int32_t run_types,
    gfx::NativeView native_view_for_gestures,
    absl::optional<gfx::RoundedCornersF> corners) {
  DCHECK(!IsRunning());
  DCHECK(parent);
  closing_event_time_ = base::TimeTicks();
  running_ = true;
  [menu_delegate_ setAnchorRect:bounds];

  // Ensure the UI can update while the menu is fading out.
  base::ScopedPumpMessagesInPrivateModes pump_private;

  NSWindow* window = parent->GetNativeWindow().GetNativeNSWindow();
  NSView* view = parent->GetNativeView().GetNativeNSView();
  [menu_controller_ maybeBuildWithColorProvider:parent->GetColorProvider()];
  NSMenu* const menu = [menu_controller_ menu];
  if (run_types & MenuRunner::CONTEXT_MENU) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuWillShow(
        menu, views::ElementTrackerViews::GetContextForWidget(parent));

    [NSMenu popUpContextMenu:menu
                   withEvent:EventForPositioningContextMenu(bounds, window)
                     forView:view];

    ui::ElementTrackerMac::GetInstance()->NotifyMenuDoneShowing(menu);

  } else {
    CHECK(run_types & MenuRunner::COMBOBOX);
    NSMenuItem* const checked_item = FirstCheckedItem(menu_controller_);
    base::scoped_nsobject<NSView> anchor_view(CreateMenuAnchorView(
        window, bounds, checked_item, menu.size.width, anchor));
    [menu setMinimumWidth:bounds.width() + kNativeCheckmarkWidth];
    [menu popUpMenuPositioningItem:checked_item
                        atLocation:NSZeroPoint
                            inView:anchor_view];

    [anchor_view removeFromSuperview];
  }

  closing_event_time_ = ui::EventTimeForNow();
  running_ = false;

  if (delete_after_run_) {
    delete this;
    return;
  }

  // Don't invoke the callback if Release() was called, since that usually means
  // the owning instance is being destroyed.
  if (!on_menu_closed_callback_.is_null())
    on_menu_closed_callback_.Run();
}

void MenuRunnerImplCocoa::Cancel() {
  [menu_controller_ cancel];
}

base::TimeTicks MenuRunnerImplCocoa::GetClosingEventTime() const {
  return closing_event_time_;
}

MenuRunnerImplCocoa::~MenuRunnerImplCocoa() = default;

}  // namespace views::internal
