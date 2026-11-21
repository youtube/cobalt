// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_menu_model.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"

namespace indigo {

namespace {

static constexpr int kIndigoDismissCommand = 1;
static constexpr int kIndigoOpenSettingsCommand = 2;

}  // namespace

IndigoMenuModel::IndigoMenuModel(
    Profile* profile,
    base::WeakPtr<IndigoPageActionController> controller)
    : ui::SimpleMenuModel(this), profile_(profile), controller_(controller) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  // Add menu items.
  AddItemWithStringIdAndIcon(
      kIndigoDismissCommand, IDS_INDIGO_ANCHORED_MESSAGE_MENU_DISMISS,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? vector_icons::kCloseIcon
                                         : vector_icons::kCloseOldIcon,
                                     ui::kColorMenuIcon, 16));
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(
      kIndigoOpenSettingsCommand, IDS_INDIGO_ANCHORED_MESSAGE_MENU_SETTINGS,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? vector_icons::kSettingsFilledIcon
                                         : vector_icons::kSettingsOldIcon,
                                     ui::kColorMenuIcon, 16));
}

IndigoMenuModel::~IndigoMenuModel() = default;

void IndigoMenuModel::ExecuteCommand(int command_id, int event_flags) {
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));

  if (!controller_) {
    return;
  }

  switch (command_id) {
    case kIndigoDismissCommand:
      controller_->DismissAnchoredMessage();
      break;
    case kIndigoOpenSettingsCommand:
      if (controller_) {
        controller_->OpenSettings();
      }
      break;
  }
}

}  // namespace indigo
