// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AI_MODE_BUTTON_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_AI_MODE_BUTTON_SERVICE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/ai_mode_button_config.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

class AiModeButtonService : public KeyedService,
                            public TemplateURLServiceObserver {
 public:
  // Used to pass in chrome strings for the google config.
  struct GoogleStrings {
    std::u16string entrypoint_label;
    std::u16string context_menu_label;
  };

  AiModeButtonService(TemplateURLService* template_url_service,
                      GoogleStrings google_strings);
  AiModeButtonService(const AiModeButtonService&) = delete;
  AiModeButtonService& operator=(const AiModeButtonService&) = delete;
  ~AiModeButtonService() override;

  // Registers a callback to be notified when the config changes. The callback
  // is also called immediately with the current config when
  // `RegisterOnConfigChanged()` is called.
  using Callback = base::RepeatingCallback<void(
      const ai_mode_button_config::AiModeButtonConfig*)>;
  base::CallbackListSubscription RegisterOnConfigChanged(Callback callback);

  // Returns `current_config_`. `nullptr` if the DSE doesn't support AIM button.
  // Returns a pointer to prevent callsites accidentally making copies passing
  // optionals around.
  const ai_mode_button_config::AiModeButtonConfig* GetCurrentConfig() const {
    return current_config_ ? &*current_config_ : nullptr;
  }

 private:
  friend class TestAiModeButtonService;

  // TemplateURLServiceObserver:
  // If the config has changed, updates `current_config_` and notifies
  // `callbacks_`.
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Computes the config for the current DSE.
  std::optional<ai_mode_button_config::AiModeButtonConfig>
  ComputeCurrentConfig() const;

  raw_ptr<TemplateURLService> template_url_service_;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observer{this};

  GoogleStrings google_strings_;
  std::optional<ai_mode_button_config::AiModeButtonConfig> current_config_;
  base::RepeatingCallbackList<void(
      const ai_mode_button_config::AiModeButtonConfig*)>
      callbacks_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AI_MODE_BUTTON_SERVICE_H_
