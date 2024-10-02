// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_
#define ASH_WEBUI_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_

#include <memory>

#include "chromeos/components/print_management/mojom/printing_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {
namespace printing {
namespace printing_manager {

// The WebUI for chrome://print-management/.
class PrintManagementUI : public ui::MojoWebUIController {
 public:
  using BindPrintingMetadataProviderCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<chromeos::printing::printing_manager::mojom::
                                PrintingMetadataProvider>)>;

  // |callback_| should bind the pending receiver to an implementation of
  // chromeos::printing::printing_manager::mojom::PrintingMetadataProvider.
  PrintManagementUI(content::WebUI* web_ui,
                    BindPrintingMetadataProviderCallback callback_);
  ~PrintManagementUI() override;

  PrintManagementUI(const PrintManagementUI&) = delete;
  PrintManagementUI& operator=(const PrintManagementUI&) = delete;

  // Instantiates implementor of the
  // chromeos::printing::printing_manager::mojom::PrintingManager mojo interface
  // by passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  const BindPrintingMetadataProviderCallback bind_pending_receiver_callback_;

  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace printing_manager
}  // namespace printing
}  // namespace ash

#endif  // ASH_WEBUI_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_
