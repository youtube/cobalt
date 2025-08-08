// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/feature_constants.h"

namespace chromecast {
namespace feature {

const char kEnableTrackControlAppRendererFeatureUse[] =
    "track_control_renderer_feature_use";
const char kEnablePlayready[] = "playready";
const char kEnableDevMode[] = "dev_mode";
const char kDevModeOrigin[] = "dev_mode_origin";
const char kEnableSystemGestures[] = "system_gestures";
const char kEnableWindowControls[] = "enable_window_controls";
const char kEnableSettingsUiMojo[] = "enable_settings_ui_mojo";
const char kDisableBackgroundTabTimerThrottle[] =
    "disable_background_tab_timer_throttle";
const char kDisableBackgroundSuspend[] = "disable_background_suspend";
const char kEnableAssistantMessagePipe[] = "enable_assistant_message_pipe";
const char kEnableDemoStandaloneMode[] = "enable_demo_standalone_mode";

const char kKeyAppId[] = "app_id";
const char kKeyAllowInsecureContent[] = "allow_insecure_content";

const char kCastCoreRendererFeatures[] = "cast_core_renderer_features";
const char kCastCoreEnforceFeaturePermissions[] =
    "cast_core_enforce_feature_permissions";
const char kCastCoreFeaturePermissions[] = "cast_core_feature_permissions";
const char kCastCoreFeaturePermissionOrigins[] =
    "cast_core_feature_permission_origins";
const char kCastCoreIsAudioOnly[] = "cast_core_is_audio_only";
const char kCastCoreIsRemoteControlMode[] = "cast_core_is_remote_control_mode";

}  // namespace feature
}  // namespace chromecast
