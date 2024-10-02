// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_

#include <vector>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/view.h"

namespace views {
class Label;
class TableLayoutView;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOnboardingView
    : public views::View,
      public AssistantControllerObserver,
      public AssistantSuggestionsModelObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantOnboardingView(AssistantViewDelegate* delegate);
  AssistantOnboardingView(const AssistantOnboardingView&) = delete;
  AssistantOnboardingView& operator=(const AssistantOnboardingView&) = delete;
  ~AssistantOnboardingView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;

  // AssistantController:
  void OnAssistantControllerDestroying() override;

  // AssistantSuggestionsModelObserver:
  void OnOnboardingSuggestionsChanged(
      const std::vector<AssistantSuggestion>& onboarding_suggestions) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      absl::optional<AssistantEntryPoint> entry_point,
      absl::optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void UpdateGreeting();
  void UpdateSuggestions();

  const raw_ptr<AssistantViewDelegate, ExperimentalAsh>
      delegate_;  // Owned by AssistantController.
  raw_ptr<views::Label, ExperimentalAsh> greeting_ =
      nullptr;  // Owned by view hierarchy.
  raw_ptr<views::Label, ExperimentalAsh> intro_ =
      nullptr;  // Owned by view hierarchy.
  raw_ptr<views::TableLayoutView, ExperimentalAsh> table_ =
      nullptr;  // Owned by view hierarchy.

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_VIEW_H_
