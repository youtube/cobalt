// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_ADMIN_TEMPLATE_LAUNCH_TRACKER_H_
#define ASH_WM_DESKS_TEMPLATES_ADMIN_TEMPLATE_LAUNCH_TRACKER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class DeskTemplate;
class SavedDeskDelegate;

// Holds updates to apply to a window in an admin template.
struct AdminTemplateWindowUpdate {
  // The restore window id (RWID) of the window that is being updated. This is
  // the RWID as it appears in the originating template, not what the launched
  // window has since the latter has been made unique for each launch.
  int32_t template_rwid;

  // Optional new bounds for the window.
  absl::optional<gfx::Rect> bounds;

  // Optional new display ID for the window.
  absl::optional<int64_t> display_id;
};

// Apply changes in `update` to `admin_template`. If `update` has specified a
// window that doesn't exist in the template, false is returned and the template
// is left unchanged.
ASH_EXPORT bool MergeAdminTemplateWindowUpdate(
    DeskTemplate& admin_template,
    const AdminTemplateWindowUpdate& update);

// Figures out where to place an admin template window. The passed `bounds` is
// first adjusted to fit inside `work_area` (note that this may change the
// size). It is then placed, if possible, so that it does not overlap exactly
// with any of the bounds in `existing_bounds`.
ASH_EXPORT void AdjustAdminTemplateWindowBounds(
    const gfx::Rect& work_area,
    const std::vector<gfx::Rect>& existing_bounds,
    gfx::Rect& bounds);

// Returns the number of `window_count` bounds for windows on the
// `available_bounds` screen.
ASH_EXPORT std::vector<gfx::Rect> GetInitialWindowLayout(
    const gfx::Rect& work_area,
    const int window_count);

// This class is used to launch an admin template and track the windows that
// have been launched from it.
class ASH_EXPORT AdminTemplateLaunchTracker {
 public:
  // Construct an admin template launch tracker. The passed `admin_template`
  // (which must not be null) will be updated as the user interacts with
  // launched windows. Updates to the template are sent to `template_update_cb`.
  AdminTemplateLaunchTracker(
      std::unique_ptr<DeskTemplate> admin_template,
      base::RepeatingCallback<void(const DeskTemplate&)> template_update_cb);

  AdminTemplateLaunchTracker(const AdminTemplateLaunchTracker&) = delete;
  AdminTemplateLaunchTracker& operator=(const AdminTemplateLaunchTracker&) =
      delete;

  // Destroys the launch tracker, as well as all internal window observers. The
  // update callback will not be invoked after the tracker has been destroyed.
  ~AdminTemplateLaunchTracker();

  // Sets up window observers for windows that are expected to be launched. It
  // then launches the template using `delegate`.
  void LaunchTemplate(SavedDeskDelegate* delegate, int64_t default_display_id);

 private:
  void OnObserverCreated(std::unique_ptr<base::CheckedObserver> observer);

  void OnObserverDone(base::CheckedObserver* observer);

  void OnUpdate(const AdminTemplateWindowUpdate& update);

  // The template that will be updated based on received events. Changes are
  // eventually saved to the storage model.
  std::unique_ptr<DeskTemplate> admin_template_;

  // Callback that will be invoked when the template has been updated.
  base::RepeatingCallback<void(const DeskTemplate&)> template_update_cb_;

  // Window observers launched by the tracker. They are tracked so that they can
  // be destroyed, if the launchtracker itself is destroyed.
  std::vector<std::unique_ptr<base::CheckedObserver>> window_observers_;

  base::WeakPtrFactory<AdminTemplateLaunchTracker> weak_ptr_factory_{this};
};

}  // namespace ash

#endif
