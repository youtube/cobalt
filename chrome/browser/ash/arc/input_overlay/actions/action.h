// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace arc::input_overlay {

constexpr char kKeyboard[] = "keyboard";
constexpr char kMouse[] = "mouse";

class ActionView;
class DisplayOverlayController;
class TouchInjector;

// Parse position from Json.
std::unique_ptr<Position> ParsePosition(const base::Value::Dict& dict);
// Log events for debugging.
void LogEvent(const ui::Event& event);
void LogTouchEvents(const std::list<ui::TouchEvent>& events);
// Json format:
// {
//    "key": "KeyA",
//    "modifiers": [""] // optional: "ctrl", "shift", "alt".
// }
absl::optional<std::pair<ui::DomCode, int>> ParseKeyboardKey(
    const base::Value& value,
    const base::StringPiece key_name);

// Return true if the |input_element| is bound.
bool IsInputBound(const InputElement& input_element);
// Return true if the |input_element| is bound to keyboard key.
bool IsKeyboardBound(const InputElement& input_element);
// Return true if the |input_element| is bound to mouse.
bool IsMouseBound(const InputElement& input_element);

// This is the base touch action which converts other events to touch
// events for input overlay.
class Action {
 public:
  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  virtual ~Action();

  virtual bool ParseFromJson(const base::Value& value);
  // Used to create an action from UI.
  virtual bool InitFromEditor();
  bool ParseFromProto(const ActionProto& proto);
  void OverwriteFromProto(const ActionProto& proto);
  // 1. Return true & non-empty touch_events:
  //    Call SendEventFinally to send simulated touch event.
  // 2. Return true & empty touch_events:
  //    Call DiscardEvent to discard event such as repeated key event.
  // 3. Return false:
  //    No need to rewrite the event, so call SendEvent with original event.
  virtual bool RewriteEvent(const ui::Event& origin,
                            const bool is_mouse_locked,
                            const gfx::Transform* rotation_transform,
                            std::list<ui::TouchEvent>& touch_events,
                            bool& keep_original_event) = 0;
  // Get the UI location in the content view.
  virtual gfx::PointF GetUICenterPosition() = 0;
  virtual std::unique_ptr<ActionView> CreateView(
      DisplayOverlayController* display_overlay_controller) = 0;
  // This is called if other actions take the input binding from this action.
  // |input_element| should overlap the current displayed binding. If it is
  // partially overlapped, then we only unbind the overlapped input.
  virtual void UnbindInput(const InputElement& input_element) = 0;

  // This is called for editing the actions before change is saved. Or for
  // loading the customized data to override the default input mapping.
  void PrepareToBindInput(std::unique_ptr<InputElement> input_element);
  // Save |pending_input_| as |current_input_|.
  void BindPending();
  // Cancel |pending_input_| and |pending_position_|.
  void CancelPendingBind();
  void ResetPendingBind();

  void PrepareToBindPosition(const gfx::Point& new_touch_center);
  void PrepareToBindPosition(std::unique_ptr<Position> position);

  // Restore the input binding back to the original binding.
  void RestoreToDefault();
  // Return currently displayed input binding.
  const InputElement& GetCurrentDisplayedInput();
  // Check if there is any overlap between |input_element| and current
  // displayed binding.
  bool IsOverlapped(const InputElement& input_element);
  // Make sure |original_positions_| is not empty before calling this.
  const Position& GetCurrentDisplayedPosition();

  // Return the proto object if the action is customized.
  virtual std::unique_ptr<ActionProto> ConvertToProtoIfCustomized() const;
  // Update |touch_down_positions_| after reposition, rotation change or window
  // bounds change.
  void UpdateTouchDownPositions();

  // Cancel event when the focus is lost or window is destroyed and the touch
  // event is still not released.
  absl::optional<ui::TouchEvent> GetTouchCanceledEvent();
  absl::optional<ui::TouchEvent> GetTouchReleasedEvent();
  int GetUIRadius();

  bool IsDefaultAction() const;

  InputElement* current_input() const { return current_input_.get(); }
  InputElement* original_input() const { return original_input_.get(); }
  InputElement* pending_input() const { return pending_input_.get(); }
  void set_pending_input(std::unique_ptr<InputElement> input) {
    if (pending_input_)
      pending_input_.reset();
    pending_input_ = std::move(input);
  }
  int id() { return id_; }
  const std::string& name() { return name_; }
  const std::vector<Position>& original_positions() const {
    return original_positions_;
  }
  const std::vector<Position>& current_positions() const {
    return current_positions_;
  }
  const std::vector<gfx::PointF>& touch_down_positions() const {
    return touch_down_positions_;
  }
  bool require_mouse_locked() const { return require_mouse_locked_; }
  TouchInjector* touch_injector() const { return touch_injector_; }
  int current_position_idx() const { return current_position_idx_; }
  const absl::optional<int> touch_id() const { return touch_id_; }
  bool on_left_or_middle_side() const { return on_left_or_middle_side_; }
  bool support_modifier_key() const { return support_modifier_key_; }
  ActionView* action_view() const { return action_view_; }
  void set_action_view(ActionView* action_view) { action_view_ = action_view; }

  bool deleted() const { return deleted_; }
  void set_deleted(bool deleted) { deleted_ = deleted; }

 protected:
  // |touch_injector| must be non-NULL and own this Action.
  explicit Action(TouchInjector* touch_injector);

  // Create a touch pressed/moved/released event with |time_stamp| and save it
  // in |touch_events|.
  bool CreateTouchPressedEvent(const base::TimeTicks& time_stamp,
                               std::list<ui::TouchEvent>& touch_events);
  void CreateTouchMovedEvent(const base::TimeTicks& time_stamp,
                             std::list<ui::TouchEvent>& touch_events);
  void CreateTouchReleasedEvent(const base::TimeTicks& time_stamp,
                                std::list<ui::TouchEvent>& touch_events);

  bool IsRepeatedKeyEvent(const ui::KeyEvent& key_event);
  // Verify the key release event. If it is verified, it continues to simulate
  // the touch event. Otherwise, consider it as discard.
  bool VerifyOnKeyRelease(ui::DomCode code);
  // Process after unbinding the input mapping.
  void PostUnbindInputProcess();

  // Original input binding.
  std::unique_ptr<InputElement> original_input_;
  // Current input binding.
  std::unique_ptr<InputElement> current_input_;
  // Pending input binding. It is used during the editing before it is saved.
  std::unique_ptr<InputElement> pending_input_;

  // Unique ID for each action.
  int id_ = 0;
  // name_ is basically for debugging and not visible to users.
  std::string name_;
  // Position take turns for each key press if there are more than
  // one positions. This is for original default positions.
  std::vector<Position> original_positions_;
  // The first element of |current_positions_| is different from
  // |original_positions_| if the position is customized.
  std::vector<Position> current_positions_;
  // Only support the reposition of the first touch position if there are more
  // than one touch position.
  std::unique_ptr<Position> pending_position_;
  // Root locations of touch point.
  std::vector<gfx::PointF> touch_down_positions_;
  // If |require_mouse_locked_| == true, the action takes effect when the mouse
  // is locked. Once the mouse is unlocked, the active actions which need mouse
  // lock will be released.
  bool require_mouse_locked_ = false;
  int parsed_input_sources_ = 0;
  absl::optional<int> touch_id_;
  size_t current_position_idx_ = 0;
  raw_ptr<TouchInjector> touch_injector_;

  gfx::PointF last_touch_root_location_;
  base::flat_set<ui::DomCode> keys_pressed_;
  // This is used for marking the position of the UI view for the action.
  // According to the design spec, the label position depends
  // on whether the action position is on left or right.
  bool on_left_or_middle_side_ = false;
  absl::optional<float> radius_;
  // By default, it doesn't support modifier key.
  bool support_modifier_key_ = false;
  raw_ptr<ActionView> action_view_ = nullptr;

 private:
  void OnTouchReleased();
  void OnTouchCancelled();
  // Create a touch event of |type| with |time_stamp| and save it
  // in |touch_events|.
  void CreateTouchEvent(ui::EventType type,
                        const base::TimeTicks& time_stamp,
                        std::list<ui::TouchEvent>& touch_events);

  // Mainly for default action to mark if it is deleted.
  bool deleted_ = false;

  // TODO(b/260937747): Update or remove when removing flags
  // |kArcInputOverlayAlphaV2| or |kArcInputOverlayBeta|.
  bool allow_reposition_;
  // Corresponds to |kArcInputOverlayBeta| flag to turn on/off the editor
  // feature of adding or removing actions.
  bool beta_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
