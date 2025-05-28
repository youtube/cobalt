// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace glic {

class GlicButtonControllerDelegate;
class GlicKeyedService;

// Controller class for the button entry point. Manages visibility, icon
// and attachment indicator for the button.
class GlicButtonController : public GlicWindowController::StateObserver {
 public:
  explicit GlicButtonController(Profile* profile,
                                GlicButtonControllerDelegate* delegate,
                                GlicKeyedService* service);
  ~GlicButtonController() override;

  // GlicWindowController::StateObserver:
  void PanelStateChanged(const mojom::PanelState& panel_state) override;

 private:
  void UpdateShowState();

  raw_ptr<Profile> profile_;
  raw_ptr<GlicButtonControllerDelegate> glic_controller_delegate_;
  raw_ptr<GlicKeyedService> glic_keyed_service_;
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_BUTTON_CONTROLLER_H_
