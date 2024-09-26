// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/views_text_services_context_menu_base.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

const char kViewsTextServicesContextMenuEmoji[] =
    "ContextMenu.ViewsTextServices.Emoji";

}  // namespace

ViewsTextServicesContextMenuBase::ViewsTextServicesContextMenuBase(
    ui::SimpleMenuModel* menu,
    Textfield* client)
    : client_(client) {
  DCHECK(client);
  DCHECK(menu);
  // Not inserted on read-only fields or if the OS/version doesn't support it.
  if (!client_->GetReadOnly() && ui::IsEmojiPanelSupported()) {
    menu->InsertSeparatorAt(0, ui::NORMAL_SEPARATOR);
    menu->InsertItemWithStringIdAt(0, IDS_CONTENT_CONTEXT_EMOJI,
                                   IDS_CONTENT_CONTEXT_EMOJI);
  }
}

ViewsTextServicesContextMenuBase::~ViewsTextServicesContextMenuBase() = default;

bool ViewsTextServicesContextMenuBase::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDS_CONTENT_CONTEXT_EMOJI) {
#if BUILDFLAG(IS_WIN)
    *accelerator = ui::Accelerator(ui::VKEY_OEM_PERIOD, ui::EF_COMMAND_DOWN);
    return true;
#elif BUILDFLAG(IS_MAC)
    *accelerator = ui::Accelerator(ui::VKEY_SPACE,
                                   ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);
    return true;
#elif BUILDFLAG(IS_CHROMEOS)
    *accelerator = ui::Accelerator(ui::VKEY_SPACE,
                                   ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
    return true;
#else
    return false;
#endif
  }

  return false;
}

bool ViewsTextServicesContextMenuBase::IsCommandIdChecked(
    int command_id) const {
  return false;
}

bool ViewsTextServicesContextMenuBase::IsCommandIdEnabled(
    int command_id) const {
  return command_id == IDS_CONTENT_CONTEXT_EMOJI;
}

void ViewsTextServicesContextMenuBase::ExecuteCommand(int command_id,
                                                      int event_flags) {
  if (command_id == IDS_CONTENT_CONTEXT_EMOJI) {
    client_->GetWidget()->ShowEmojiPanel();
    UMA_HISTOGRAM_BOOLEAN(kViewsTextServicesContextMenuEmoji, true);
  }
}

bool ViewsTextServicesContextMenuBase::SupportsCommand(int command_id) const {
  return command_id == IDS_CONTENT_CONTEXT_EMOJI;
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
// static
std::unique_ptr<ViewsTextServicesContextMenu>
ViewsTextServicesContextMenu::Create(ui::SimpleMenuModel* menu,
                                     Textfield* client) {
  return std::make_unique<ViewsTextServicesContextMenuBase>(menu, client);
}
#endif

}  // namespace views
