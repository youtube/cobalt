// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GUEST_OS_INSTALLER_GUEST_OS_INSTALLER_UI_H_
#define ASH_WEBUI_GUEST_OS_INSTALLER_GUEST_OS_INSTALLER_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/guest_os_installer/mojom/guest_os_installer.mojom.h"
#include "ash/webui/guest_os_installer/url_constants.h"
#include "base/functional/bind.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class GuestOSInstallerUI;

// The WebUIConfig for chrome://guest-os-installer
class GuestOSInstallerUIConfig
    : public ChromeOSWebUIConfig<GuestOSInstallerUI> {
 public:
  explicit GuestOSInstallerUIConfig(
      CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUIGuestOSInstallerHost,
                            create_controller_func) {}
};

// The WebUI for chrome://guest-os-installer
class GuestOSInstallerUI
    : public ui::MojoWebDialogUI,
      public ash::guest_os_installer::mojom::PageHandlerFactory {
 public:
  using DelegateFactory = base::RepeatingCallback<
      std::unique_ptr<ash::guest_os_installer::mojom::PageHandler>(
          GuestOSInstallerUI*,
          mojo::PendingRemote<ash::guest_os_installer::mojom::Page>,
          mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandler>)>;

  GuestOSInstallerUI(content::WebUI* web_ui, DelegateFactory delegate_factory);

  GuestOSInstallerUI(const GuestOSInstallerUI&) = delete;
  GuestOSInstallerUI& operator=(const GuestOSInstallerUI&) = delete;

  ~GuestOSInstallerUI() override;

  void BindInterface(
      mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandlerFactory>
          pending_receiver);

 private:
  // chromeos::guest_os_installer::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<ash::guest_os_installer::mojom::Page> pending_page,
      mojo::PendingReceiver<ash::guest_os_installer::mojom::PageHandler>
          pending_page_handler) override;

  mojo::Receiver<ash::guest_os_installer::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  std::unique_ptr<ash::guest_os_installer::mojom::PageHandler> handler_;

  const DelegateFactory delegate_factory_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_GUEST_OS_INSTALLER_GUEST_OS_INSTALLER_UI_H_
