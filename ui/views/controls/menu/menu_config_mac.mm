// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/platform_font_mac.h"

namespace {

void InitMaterialMenuConfig(views::MenuConfig* config) {
  // These config parameters are from https://crbug.com/829347 and the spec
  // images linked from that bug.
  config->submenu_horizontal_overlap = 0;
  config->minimum_text_item_height = 28;
  config->minimum_container_item_height = 40;
  config->arrow_to_edge_padding = 16;
  config->separator_height = 9;
  config->separator_lower_height = 4;
  config->separator_upper_height = 4;
  config->separator_spacing_height = 5;
  config->separator_thickness = 1;
  config->reserve_dedicated_arrow_column = false;
  config->icons_in_label = true;
  config->icon_label_spacing = 8;
  config->corner_radius = 8;
  config->auxiliary_corner_radius = 4;
  config->item_horizontal_border_padding = 0;
}

}  // namespace

namespace views {

void MenuConfig::Init() {
  context_menu_font_list = font_list = gfx::FontList(gfx::Font(
      new gfx::PlatformFontMac(gfx::PlatformFontMac::SystemFontType::kMenu)));
  check_selected_combobox_item = true;
  arrow_key_selection_wraps = false;
  use_mnemonics = false;
  show_context_menu_accelerators = false;
  all_menus_use_prefix_selection = true;
  menu_horizontal_border_size = 0;
  use_outer_border = false;
  if (!features::IsChromeRefresh2023()) {
    InitMaterialMenuConfig(this);
  }
}

void MenuConfig::InitPlatformCR2023() {
  context_menu_font_list = font_list;
}

}  // namespace views
