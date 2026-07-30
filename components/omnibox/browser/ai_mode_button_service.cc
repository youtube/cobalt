// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/ai_mode_button_service.h"

#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

AiModeButtonService::AiModeButtonService(
    TemplateURLService* template_url_service,
    GoogleStrings google_strings)
    : template_url_service_(template_url_service),
      google_strings_(std::move(google_strings)) {
  if (template_url_service_) {
    template_url_service_observer.Observe(template_url_service_);
  }
  current_config_ = ComputeCurrentConfig();
}

AiModeButtonService::~AiModeButtonService() = default;

base::CallbackListSubscription AiModeButtonService::RegisterOnConfigChanged(
    Callback callback) {
  callback.Run(GetCurrentConfig());
  return callbacks_.Add(callback);
}

void AiModeButtonService::OnTemplateURLServiceChanged() {
  auto new_config = ComputeCurrentConfig();

  // Early exit if config did not change.
  if (!new_config && !current_config_) {
    return;
  }
  if (new_config && current_config_ && new_config->id == current_config_->id) {
    return;
  }

  current_config_ = std::move(new_config);
  callbacks_.Notify(GetCurrentConfig());
}

void AiModeButtonService::OnTemplateURLServiceShuttingDown() {
  template_url_service_observer.Reset();
  template_url_service_ = nullptr;
}

std::optional<ai_mode_button_config::AiModeButtonConfig>
AiModeButtonService::ComputeCurrentConfig() const {
  if (!template_url_service_) {
    return std::nullopt;
  }
  const TemplateURL* dse = template_url_service_->GetDefaultSearchProvider();
  if (!dse) {
    return std::nullopt;
  }

  SearchEngineType type =
      dse->GetEngineType(template_url_service_->search_terms_data());
  if (type == SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    return ai_mode_button_config::AiModeButtonConfig{
        SearchEngineType::SEARCH_ENGINE_GOOGLE,
        google_strings_.entrypoint_label,
        l10n_util::GetStringUTF16(
            IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS),
        "",
        "",
        "",
        l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_BUTTON_FOCUSED),
        google_strings_.context_menu_label,
        l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_PLACEHOLDER_TEXT)};
  }

  // TODO(crbug.com/517976551): Return valid config for certain non-Google DSEs.
  return std::nullopt;
}
