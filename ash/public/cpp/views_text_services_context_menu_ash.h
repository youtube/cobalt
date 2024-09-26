// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_VIEWS_TEXT_SERVICES_CONTEXT_MENU_ASH_H_
#define ASH_PUBLIC_CPP_VIEWS_TEXT_SERVICES_CONTEXT_MENU_ASH_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/views/controls/views_text_services_context_menu_base.h"

namespace views {
class Textfield;
}

namespace ash {

// This class implements support for adding and handling text service items in
// ChromeOS system UI textfields and ash-chrome browser native textfields (i.e.,
// the omnibox but not the WebUI embedded in the browser).
class ASH_PUBLIC_EXPORT ViewsTextServicesContextMenuAsh
    : public views::ViewsTextServicesContextMenuBase {
 public:
  ViewsTextServicesContextMenuAsh(ui::SimpleMenuModel* menu,
                                  views::Textfield* client);
  ViewsTextServicesContextMenuAsh(const ViewsTextServicesContextMenuAsh&) =
      delete;
  ViewsTextServicesContextMenuAsh& operator=(
      const ViewsTextServicesContextMenuAsh&) = delete;
  ~ViewsTextServicesContextMenuAsh() override;

  // ViewsTextServicesContextMenuBase:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool SupportsCommand(int command_id) const override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_VIEWS_TEXT_SERVICES_CONTEXT_MENU_ASH_H_
