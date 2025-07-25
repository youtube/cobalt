// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_EXPAND_BUTTON_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_EXPAND_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace sharesheet {

class SharesheetExpandButton : public views::Button {
 public:
  METADATA_HEADER(SharesheetExpandButton);

  explicit SharesheetExpandButton(PressedCallback callback);
  SharesheetExpandButton(const SharesheetExpandButton&) = delete;
  SharesheetExpandButton& operator=(const SharesheetExpandButton&) = delete;

  void SetToDefaultState();
  void SetToExpandedState();

 private:
  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> label_ = nullptr;
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_EXPAND_BUTTON_H_
