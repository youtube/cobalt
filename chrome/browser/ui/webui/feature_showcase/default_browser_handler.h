// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_DEFAULT_BROWSER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_DEFAULT_BROWSER_HANDLER_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/feature_showcase/default_browser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class DefaultBrowserHandler
    : public feature_showcase::mojom::DefaultBrowserPageHandler {
 public:
  explicit DefaultBrowserHandler(
      mojo::PendingReceiver<feature_showcase::mojom::DefaultBrowserPageHandler>
          receiver);

#if BUILDFLAG(IS_WIN)
  using PinToTaskbarCallbackForTesting = base::OnceCallback<void(bool)>;
#endif

  // Constructor for testing.
  DefaultBrowserHandler(
      mojo::PendingReceiver<feature_showcase::mojom::DefaultBrowserPageHandler>
          receiver,
      base::OnceClosure on_set_as_default_completed_callback
#if BUILDFLAG(IS_WIN)
      ,
      PinToTaskbarCallbackForTesting on_pin_to_taskbar_callback
#endif
  );

  DefaultBrowserHandler(const DefaultBrowserHandler&) = delete;
  DefaultBrowserHandler& operator=(const DefaultBrowserHandler&) = delete;
  ~DefaultBrowserHandler() override;

  // feature_showcase::mojom::DefaultBrowserPageHandler:
  void SetAsDefaultBrowser() override;
  void SkipSetAsDefaultBrowser() override;

  void SetCanPin(bool can_pin);

 private:
  mojo::Receiver<feature_showcase::mojom::DefaultBrowserPageHandler> receiver_;
  bool can_pin_ = false;

  base::OnceClosure on_set_as_default_completed_callback_for_testing_;
#if BUILDFLAG(IS_WIN)
  PinToTaskbarCallbackForTesting on_pin_to_taskbar_callback_for_testing_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_DEFAULT_BROWSER_HANDLER_H_
