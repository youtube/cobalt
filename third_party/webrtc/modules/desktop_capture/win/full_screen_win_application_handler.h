/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_FULL_SCREEN_WIN_APPLICATION_HANDLER_H_
#define MODULES_DESKTOP_CAPTURE_WIN_FULL_SCREEN_WIN_APPLICATION_HANDLER_H_

#include <memory>

#include "modules/desktop_capture/full_screen_application_handler.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

class FullScreenPowerPointHandler : public FullScreenApplicationHandler {
 public:
  enum class WindowType { kEditor, kSlideShow, kOther };

  explicit FullScreenPowerPointHandler(DesktopCapturer::SourceId sourceId);

  ~FullScreenPowerPointHandler() override {}

  DesktopCapturer::SourceId FindFullScreenWindow(
      const DesktopCapturer::SourceList& window_list,
      int64_t timestamp) const override;

 private:
  WindowType GetWindowType(HWND window) const;

  // This function extracts the title from the editor. It needs to be
  // updated every time PowerPoint changes its editor title format. Currently,
  // it supports editor title in the format "Window - Title - PowerPoint".
  std::string GetDocumentTitleFromEditor(HWND window) const;

  // This function extracts the title from the slideshow when PowerPoint goes
  // fullscreen. This function needs to be updated whenever PowerPoint changes
  // its title format. Currently, it supports Fullscreen titles of the format
  // "PowerPoint Slide Show - [Window - Title]" or "PowerPoint Slide Show -
  // Window - Title".
  std::string GetDocumentTitleFromSlideShow(HWND window) const;

  bool IsEditorWindow(HWND window) const;

  bool IsSlideShowWindow(HWND window) const;
};

std::unique_ptr<FullScreenApplicationHandler>
CreateFullScreenWinApplicationHandler(DesktopCapturer::SourceId sourceId);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_FULL_SCREEN_WIN_APPLICATION_HANDLER_H_
