// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_PADDED_BUTTON_H_
#define UI_MESSAGE_CENTER_VIEWS_PADDED_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"

namespace message_center {

// PaddedButtons are ImageButtons whose image can be padded within the button.
// This allows the creation of buttons like the notification close and expand
// buttons whose clickable areas extends beyond their image areas
// (<http://crbug.com/168822>) without the need to create and maintain
// corresponding resource images with alpha padding. In the future, this class
// will also allow for buttons whose touch areas extend beyond their clickable
// area (<http://crbug.com/168856>).
class MESSAGE_CENTER_EXPORT PaddedButton : public views::ImageButton {
 public:
  METADATA_HEADER(PaddedButton);

  explicit PaddedButton(PressedCallback callback);
  PaddedButton(const PaddedButton&) = delete;
  PaddedButton& operator=(const PaddedButton&) = delete;
  ~PaddedButton() override = default;

  // views::ImageButton:
  void OnThemeChanged() override;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_PADDED_BUTTON_H_
