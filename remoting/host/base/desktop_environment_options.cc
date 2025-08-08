// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/desktop_environment_options.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

using DesktopCaptureOptions = webrtc::DesktopCaptureOptions;

// static
DesktopEnvironmentOptions DesktopEnvironmentOptions::CreateDefault() {
  DesktopEnvironmentOptions options;
  options.desktop_capture_options_ = DesktopCaptureOptions::CreateDefault();
  options.Initialize();
  return options;
}

DesktopEnvironmentOptions::DesktopEnvironmentOptions() {
  Initialize();
}

DesktopEnvironmentOptions::DesktopEnvironmentOptions(
    DesktopEnvironmentOptions&& other) = default;
DesktopEnvironmentOptions::DesktopEnvironmentOptions(
    const DesktopEnvironmentOptions& other) = default;
DesktopEnvironmentOptions::~DesktopEnvironmentOptions() = default;
DesktopEnvironmentOptions& DesktopEnvironmentOptions::operator=(
    DesktopEnvironmentOptions&& other) = default;
DesktopEnvironmentOptions& DesktopEnvironmentOptions::operator=(
    const DesktopEnvironmentOptions& other) = default;

void DesktopEnvironmentOptions::Initialize() {
  desktop_capture_options_.set_detect_updated_region(true);
}

const DesktopCaptureOptions*
DesktopEnvironmentOptions::desktop_capture_options() const {
  return &desktop_capture_options_;
}

DesktopCaptureOptions* DesktopEnvironmentOptions::desktop_capture_options() {
  return &desktop_capture_options_;
}

bool DesktopEnvironmentOptions::enable_curtaining() const {
  return enable_curtaining_;
}

void DesktopEnvironmentOptions::set_enable_curtaining(bool enabled) {
  enable_curtaining_ = enabled;
}

bool DesktopEnvironmentOptions::enable_user_interface() const {
  return enable_user_interface_;
}

void DesktopEnvironmentOptions::set_enable_user_interface(bool enabled) {
  enable_user_interface_ = enabled;
}

bool DesktopEnvironmentOptions::enable_notifications() const {
  return enable_notifications_;
}

void DesktopEnvironmentOptions::set_enable_notifications(bool enabled) {
  enable_notifications_ = enabled;
}

bool DesktopEnvironmentOptions::terminate_upon_input() const {
  return terminate_upon_input_;
}

void DesktopEnvironmentOptions::set_terminate_upon_input(bool enabled) {
  terminate_upon_input_ = enabled;
}

bool DesktopEnvironmentOptions::enable_file_transfer() const {
  return enable_file_transfer_;
}

void DesktopEnvironmentOptions::set_enable_file_transfer(bool enabled) {
  enable_file_transfer_ = enabled;
}

bool DesktopEnvironmentOptions::enable_remote_open_url() const {
  return enable_remote_open_url_;
}

void DesktopEnvironmentOptions::set_enable_remote_open_url(bool enabled) {
  enable_remote_open_url_ = enabled;
}

bool DesktopEnvironmentOptions::enable_remote_webauthn() const {
  return enable_remote_webauthn_;
}

void DesktopEnvironmentOptions::set_enable_remote_webauthn(bool enabled) {
  enable_remote_webauthn_ = enabled;
}

const absl::optional<size_t>& DesktopEnvironmentOptions::clipboard_size()
    const {
  return clipboard_size_;
}

void DesktopEnvironmentOptions::set_clipboard_size(
    absl::optional<size_t> clipboard_size) {
  clipboard_size_ = std::move(clipboard_size);
}

bool DesktopEnvironmentOptions::capture_video_on_dedicated_thread() const {
  // TODO(joedow): Determine whether we can migrate additional platforms to
  // using the DesktopCaptureWrapper instead of the DesktopCaptureProxy. Then
  // clean up DesktopCapturerProxy::Core::CreateCapturer().
#if BUILDFLAG(IS_LINUX)
  return capture_video_on_dedicated_thread_;
#else
  return false;
#endif
}

void DesktopEnvironmentOptions::set_capture_video_on_dedicated_thread(
    bool use_dedicated_thread) {
  capture_video_on_dedicated_thread_ = use_dedicated_thread;
}

void DesktopEnvironmentOptions::ApplySessionOptions(
    const SessionOptions& options) {
  // This field is for test purpose. Usually it should not be set to false.
  absl::optional<bool> detect_updated_region =
      options.GetBool("Detect-Updated-Region");
  if (detect_updated_region.has_value()) {
    desktop_capture_options_.set_detect_updated_region(*detect_updated_region);
  }
  absl::optional<bool> capture_video_on_dedicated_thread =
      options.GetBool("Capture-Video-On-Dedicated-Thread");
  if (capture_video_on_dedicated_thread.has_value()) {
    set_capture_video_on_dedicated_thread(*capture_video_on_dedicated_thread);
  }
#if defined(WEBRTC_USE_PIPEWIRE)
  desktop_capture_options_.set_allow_pipewire(true);
  desktop_capture_options_.set_pipewire_use_damage_region(true);
#endif  // defined(WEBRTC_USE_PIPEWIRE)
}

}  // namespace remoting
