// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_RADIO_BUTTON_H_
#define ASH_STYLE_RADIO_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/style/option_button_base.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A rectangular label button with four different icon types, circle icon on
// left or right side, check icon on left or right side. It's usually used in
// the group of radio buttons. Please refer `RadioButtonGroup` for more details.
class ASH_EXPORT RadioButton : public OptionButtonBase {
 public:
  METADATA_HEADER(RadioButton);

  enum class IconDirection {
    kLeading,
    kFollowing,
  };

  enum class IconType {
    kCircle,
    kCheck,
  };

  explicit RadioButton(int button_width,
                       PressedCallback callback,
                       const std::u16string& label = std::u16string(),
                       IconDirection icon_direction = IconDirection::kLeading,
                       IconType icon_type = IconType::kCircle,
                       const gfx::Insets& insets = kDefaultPadding);
  RadioButton(const RadioButton&) = delete;
  RadioButton& operator=(const RadioButton&) = delete;
  ~RadioButton() override;

  // views::LabelButton:
  gfx::ImageSkia GetImage(ButtonState for_state) const override;

  // OptionButtonBase::
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool IsIconOnTheLeftSide() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  const IconDirection icon_direction_;
  const IconType icon_type_;
};

}  // namespace ash

#endif  // ASH_STYLE_RADIO_BUTTON_H_