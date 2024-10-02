// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TRACKER_H_
#define ASH_ACCELERATORS_ACCELERATOR_TRACKER_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_handler.h"

namespace ash {

using KeyState = ui::Accelerator::KeyState;

// A data structure to identify a tracking key event.
struct TrackerData {
  constexpr TrackerData(KeyState key_state,
                        ui::KeyboardCode key_code,
                        int flags)
      : key_state(key_state), key_code(key_code), flags(flags) {}

  KeyState key_state;
  ui::KeyboardCode key_code;
  int flags;

  // Comparator in the base::flat_map where TrackerData is used as the
  // key. The order of comparison follows ui::Accelerator.
  constexpr bool operator<(const TrackerData& rhs) const {
    if (key_code != rhs.key_code)
      return key_code < rhs.key_code;
    if (key_state != rhs.key_state)
      return key_state < rhs.key_state;
    return flags < rhs.flags;
  }
};

using TrackerDataActionPair = std::pair<TrackerData, base::StringPiece>;

// A list of pair between TrackerData and user action string.
// Add the key combination you want to track here.
// Make sure the user action string starts with "AccelTracker_"
// to differentiate with the real accelerators.
// Also document the user action in tools/metrics/actions/actions.xml.
constexpr TrackerDataActionPair kAcceleratorTrackerList[] = {
    {{KeyState::PRESSED, ui::VKEY_3, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screenshot_Full_Mac_Launcher"},
    {{KeyState::PRESSED, ui::VKEY_3, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screenshot_Full_Mac_Ctrl"},
    {{KeyState::PRESSED, ui::VKEY_3, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screenshot_Full_Mac_Alt"},
    {{KeyState::PRESSED, ui::VKEY_4, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screenshot_Partial_Mac_Launcher"},
    {{KeyState::PRESSED, ui::VKEY_4, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screenshot_Partial_Mac_Ctrl"},
    {{KeyState::PRESSED, ui::VKEY_4, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screenshot_Partial_Mac_Alt"},
    {{KeyState::PRESSED, ui::VKEY_5, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screen_Capture_Bar_Mac_Launcher"},
    {{KeyState::PRESSED, ui::VKEY_5, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screen_Capture_Bar_Mac_Ctrl"},
    {{KeyState::PRESSED, ui::VKEY_5, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screen_Capture_Bar_Mac_Alt"},
    {{KeyState::PRESSED, ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
     "AccelTracker_Screen_Capture_Bar_Windows"},
};

constexpr size_t kAcceleratorTrackerListLength =
    std::size(kAcceleratorTrackerList);

// AcceleratorTracker is a Shell pretarget EventHandler that only records user
// action metrics for any key combinations and does not consume the inputted
// events or block them from propagating.
class ASH_EXPORT AcceleratorTracker : public ui::EventHandler {
 public:
  explicit AcceleratorTracker(
      base::span<const TrackerDataActionPair> tracker_data_list);
  AcceleratorTracker(const AcceleratorTracker&) = delete;
  AcceleratorTracker& operator=(const AcceleratorTracker&) = delete;
  ~AcceleratorTracker() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  // Used to lookup for the user action string given a key event.
  base::flat_map<TrackerData, base::StringPiece> accelerator_tracker_map_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TRACKER_H_
