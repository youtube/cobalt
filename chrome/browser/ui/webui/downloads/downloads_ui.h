// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/layout.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class RefCountedMemory;
}

class DownloadsDOMHandler;

class DownloadsUI : public ui::MojoWebUIController,
                    public downloads::mojom::PageHandlerFactory {
 public:
  explicit DownloadsUI(content::WebUI* web_ui);

  DownloadsUI(const DownloadsUI&) = delete;
  DownloadsUI& operator=(const DownloadsUI&) = delete;

  ~DownloadsUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<downloads::mojom::PageHandlerFactory> receiver);

 private:
  // downloads::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<downloads::mojom::Page> page,
      mojo::PendingReceiver<downloads::mojom::PageHandler> receiver) override;

  std::unique_ptr<DownloadsDOMHandler> page_handler_;

  mojo::Receiver<downloads::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_H_
