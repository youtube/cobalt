// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/themes_and_customization_handler.h"

#include <utility>

ThemesAndCustomizationHandler::ThemesAndCustomizationHandler(
    mojo::PendingReceiver<
        feature_showcase::mojom::ThemesAndCustomizationPageHandler> receiver,
    ThemeService* theme_service)
    : receiver_(this, std::move(receiver)), theme_service_(theme_service) {}

ThemesAndCustomizationHandler::~ThemesAndCustomizationHandler() {
  if (revert_theme_on_destruction_) {
    RevertTheme();
  }
}

void ThemesAndCustomizationHandler::SnapshotTheme() {
  if (theme_service_) {
    original_color_scheme_ = theme_service_->GetBrowserColorScheme();
    theme_reinstaller_ = theme_service_->BuildReinstallerForCurrentTheme();
  }
}

void ThemesAndCustomizationHandler::AcceptTheme() {
  revert_theme_on_destruction_ = false;
}

void ThemesAndCustomizationHandler::RevertTheme() {
  CHECK(revert_theme_on_destruction_);
  revert_theme_on_destruction_ = false;

  if (!theme_service_) {
    return;
  }
  theme_service_->SetBrowserColorScheme(original_color_scheme_);
  theme_reinstaller_->Reinstall();
}
