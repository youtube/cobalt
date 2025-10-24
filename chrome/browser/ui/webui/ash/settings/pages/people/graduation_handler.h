// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_GRADUATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_GRADUATION_HANDLER_H_

#include "ash/public/cpp/graduation/graduation_manager.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/mojom/graduation_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace ash::settings {

// Responsible for handling interaction between C++ and the Graduation UI in OS
// Settings.
class GraduationHandler : public graduation::mojom::GraduationHandler,
                          public ash::graduation::GraduationManagerObserver {
 public:
  explicit GraduationHandler(Profile* profile);
  GraduationHandler(const GraduationHandler&) = delete;
  GraduationHandler& operator=(const GraduationHandler&) = delete;
  ~GraduationHandler() override;

  void BindInterface(
      mojo::PendingReceiver<graduation::mojom::GraduationHandler> receiver);

  // graduation::mojom::GraduationHandler:
  void LaunchGraduationApp() override;
  void AddObserver(mojo::PendingRemote<graduation::mojom::GraduationObserver>
                       observer) override;

  // ash::graduation::GraduationManagerObserver
  void OnGraduationAppUpdate(bool is_enabled) override;

 private:
  mojo::Receiver<graduation::mojom::GraduationHandler> receiver_{this};
  mojo::RemoteSet<graduation::mojom::GraduationObserver> observer_list_;
  raw_ptr<Profile> profile_ = nullptr;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_GRADUATION_HANDLER_H_
