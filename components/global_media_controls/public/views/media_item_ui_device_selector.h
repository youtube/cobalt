// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_

#include "ui/views/view.h"

namespace global_media_controls {

class MediaItemUIView;

// A MediaItemUIDeviceSelector is a views::View that should be inserted into the
// bottom of the MediaItemUI which contains and expandable list of devices to
// connect to (audio/Cast/etc).
class MediaItemUIDeviceSelector : public views::View {
 public:
  // Gives the device selector a pointer to the MediaItemUIView so that it can
  // inform it of size changes.
  virtual void SetMediaItemUIView(MediaItemUIView* view) = 0;

  // Called when the color theme has changed.
  virtual void OnColorsChanged(SkColor foreground, SkColor background) = 0;

  // Called when an audio device switch has occurred
  virtual void UpdateCurrentAudioDevice(
      const std::string& current_device_id) = 0;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_DEVICE_SELECTOR_H_
