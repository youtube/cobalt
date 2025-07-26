// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_button_controller.h"

#include "chrome/browser/glic/glic_button_controller_delegate.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/prefs/pref_service.h"

namespace glic {

GlicButtonController::GlicButtonController(
    Profile* profile,
    GlicButtonControllerDelegate* delegate,
    GlicKeyedService* service)
    : profile_(profile),
      glic_controller_delegate_(delegate),
      glic_keyed_service_(service) {
  CHECK(glic_controller_delegate_);
  CHECK(glic_keyed_service_);

  // Initialize default values
  PanelStateChanged(glic_keyed_service_->window_controller().GetPanelState());
  UpdateShowState();

  // Observe for changes in preferences and panel state events
  pref_registrar_.Init(profile_->GetPrefs());
  for (std::string_view pref :
       {glic::prefs::kGlicPinnedToTabstrip, glic::prefs::kGlicSettingsPolicy}) {
    pref_registrar_.Add(
        pref, base::BindRepeating(&GlicButtonController::UpdateShowState,
                                  base::Unretained(this)));
  }

  glic_keyed_service_->window_controller().AddStateObserver(this);
}

GlicButtonController::~GlicButtonController() {
  glic_keyed_service_->window_controller().RemoveStateObserver(this);
}

void GlicButtonController::PanelStateChanged(
    const mojom::PanelState& panel_state) {
  if (panel_state.kind == mojom::PanelState_Kind::kDetached) {
    glic_controller_delegate_->SetIcon(GlicVectorIconManager::GetVectorIcon(
        IDR_GLIC_ATTACH_BUTTON_VECTOR_ICON));
  } else {
    glic_controller_delegate_->SetIcon(
        GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON));
  }
}

void GlicButtonController::UpdateShowState() {
  const bool is_enabled_for_profile =
      GlicEnabling::IsEnabledForProfile(profile_);
  const bool is_pinned_to_tabstrip =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);

  if (is_enabled_for_profile && is_pinned_to_tabstrip) {
    glic_keyed_service_->TryPreload();
    glic_controller_delegate_->SetShowState(true);
  } else {
    glic_controller_delegate_->SetShowState(false);
  }
}

}  // namespace glic
