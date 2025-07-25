// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}

namespace ash {

// Refer to `anchored_nudge_manager_impl.cc` to see the duration values.
// TODO(b/297619385): Move constants to a new constants file.
enum class NudgeDuration {
  // Default duration that is used for nudges that expire.
  kDefaultDuration = 0,

  // Used for nudges with a button or a body text that has
  // `AnchoredNudgeManagerImpl::kLongBodyTextLength` or more characters.
  kMediumDuration = 1,

  // Used for nudges that are meant to persist until user interacts with them.
  kLongDuration = 2,

  kMaxValue = kLongDuration
};

using HoverStateChangeCallback =
    base::RepeatingCallback<void(bool is_hovering)>;
using NudgeClickCallback = base::RepeatingCallback<void()>;
using NudgeDismissCallback = base::RepeatingCallback<void()>;

// Describes the contents of a System Nudge (AnchoredNudge), which is a notifier
// that informs users about something that might enhance their experience. See
// the "Educational Nudges" section in go/notifier-framework for example usages.
// Nudges may anchor to any `views::View` on screen and will follow it to set
// its bounds. Nudges with no `anchor_view` will show in the default location.
// Nudges `anchored_to_shelf` will set their arrow based on the shelf alignment.
// TODO(b/285988235): `AnchoredNudge` will replace the existing `SystemNudge`
// and take over its name.
struct ASH_PUBLIC_EXPORT AnchoredNudgeData {
  AnchoredNudgeData(const std::string& id,
                    NudgeCatalogName catalog_name,
                    const std::u16string& body_text,
                    views::View* anchor_view = nullptr);
  AnchoredNudgeData(AnchoredNudgeData&& other);
  AnchoredNudgeData& operator=(AnchoredNudgeData&& other);
  ~AnchoredNudgeData();

  views::View* GetAnchorView() { return anchor_view_tracker_->view(); }
  bool is_anchored() { return is_anchored_; }

  // Sets the anchor view, observes it with a view tracker to assign a nullptr
  // in case the view is deleted, and sets the `is_anchored_` member variable.
  void SetAnchorView(views::View* anchor_view);

  // Required system nudge elements.
  std::string id;
  NudgeCatalogName catalog_name;
  std::u16string body_text;

  // Optional system nudge view elements. If not empty, a leading image or nudge
  // title will be created.
  ui::ImageModel image_model;
  std::u16string title_text;

  // Callback for close button pressed.
  base::RepeatingClosure close_button_callback;

  // Optional system nudge buttons. If the text is not empty, the respective
  // button will be created. Pressing the button will execute its callback, if
  // any, followed by the nudge being closed. `second_button_text` should only
  // be set if `first_button_text` has also been set.
  // TODO(b/285023559): Add a `ChainedCancelCallback` class instead of a
  // `RepeatingClosure` so we don't have to manually modify the provided
  // callbacks in the manager.
  std::u16string first_button_text;
  base::RepeatingClosure first_button_callback = base::DoNothing();

  std::u16string second_button_text;
  base::RepeatingClosure second_button_callback = base::DoNothing();

  // Used to set the nudge's placement in relation to the anchor view, if any.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::BOTTOM_CENTER;

  // Nudges can set a default, medium or long duration for nudges that persist.
  // Refer to `anchored_nudge_manager_impl.cc` to see the duration values.
  // TODO(b/297619385): Move constants to a new constants file.
  NudgeDuration duration = NudgeDuration::kDefaultDuration;

  // If true, `arrow` will be set based on the current shelf alignment, and the
  // nudge will listen to shelf alignment changes to readjust its `arrow`.
  // It will maintain the shelf visible while a nudge is being shown.
  bool anchored_to_shelf = false;

  // Nudge action callbacks.
  HoverStateChangeCallback hover_state_change_callback;
  NudgeClickCallback click_callback;
  NudgeDismissCallback dismiss_callback;

 private:
  // True if an anchor view is provided when constructing the nudge data object
  // or through the `SetAnchorView` function.
  bool is_anchored_ = false;

  // View tracker that caches a pointer to the anchor view and sets it to
  // nullptr in case the view was deleted.
  std::unique_ptr<views::ViewTracker> anchor_view_tracker_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
