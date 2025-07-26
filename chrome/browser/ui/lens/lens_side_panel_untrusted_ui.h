// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_UNTRUSTED_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_ghost_loader.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/searchbox/searchbox.mojom-forward.h"

class LensOverlayController;

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace lens {

class LensSidePanelUntrustedUI;

class LensSidePanelUntrustedUIConfig
    : public DefaultTopChromeWebUIConfig<LensSidePanelUntrustedUI> {
 public:
  LensSidePanelUntrustedUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                    chrome::kChromeUILensSidePanelHost) {}
};

// WebUI controller for the chrome-untrusted://lens/ page.
class LensSidePanelUntrustedUI
    : public UntrustedTopChromeWebUIController,
      public lens::mojom::LensSidePanelPageHandlerFactory,
      public lens::mojom::LensGhostLoaderPageHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit LensSidePanelUntrustedUI(content::WebUI* web_ui);

  LensSidePanelUntrustedUI(const LensSidePanelUntrustedUI&) = delete;
  LensSidePanelUntrustedUI& operator=(const LensSidePanelUntrustedUI&) = delete;
  ~LensSidePanelUntrustedUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandlerFactory>
          receiver);

  // Instantiates the implementor of the
  // lens::mojom::LensGhostLoaderPageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<lens::mojom::LensGhostLoaderPageHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the searchbox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver);

  // Instantiates the implementor of the
  // color_change_listener::mojom::PageHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Instantiates the implementor of the
  // help_bubble::mojom::HelpBubbleHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          receiver);

  static constexpr std::string GetWebUIName() {
    return "LensSidePanelUntrusted";
  }

 private:
  LensOverlayController& GetLensOverlayController();

  // lens::mojom::LensSidePanelPageHandlerFactory:
  void CreateSidePanelPageHandler(
      mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) override;

  // lens::mojom::LensGhostLoaderPageHandlerFactory:
  void CreateGhostLoaderPage(
      mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  mojo::Receiver<lens::mojom::LensSidePanelPageHandlerFactory>
      lens_side_panel_page_factory_receiver_{this};

  mojo::Receiver<lens::mojom::LensGhostLoaderPageHandlerFactory>
      lens_ghost_loader_page_factory_receiver_{this};

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;

  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  base::WeakPtrFactory<LensSidePanelUntrustedUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_UNTRUSTED_UI_H_
