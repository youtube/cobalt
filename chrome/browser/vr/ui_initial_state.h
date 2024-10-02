// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INITIAL_STATE_H_
#define CHROME_BROWSER_VR_UI_INITIAL_STATE_H_

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

// This class describes the initial state of a UI, and may be used by a UI
// instances owner to specify a custom state on startup.
struct VR_BASE_EXPORT UiInitialState {
  UiInitialState();
  UiInitialState(const UiInitialState& other);
  bool in_web_vr = true;
  bool browsing_disabled = true;
  bool has_or_can_request_record_audio_permission = false;
  bool assets_supported = false;
  bool supports_selection = false;
  bool needs_keyboard_update = false;
  bool is_standalone_vr_device = false;
  bool create_tabs_view = false;
  bool gvr_input_support = false;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INITIAL_STATE_H_
