// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/placeholder_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

PlaceholderScreenHandler::PlaceholderScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PlaceholderScreenHandler::~PlaceholderScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void PlaceholderScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void PlaceholderScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace ash
