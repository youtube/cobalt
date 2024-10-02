// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Handles API requests from chrome://web-app-internals page by implementing
// mojom::WebAppInternalsHandler.
class WebAppInternalsHandler : public mojom::WebAppInternalsHandler {
 public:
  static void BuildDebugInfo(
      Profile* profile,
      base::OnceCallback<void(base::Value root)> callback);

  WebAppInternalsHandler(
      Profile* profile,
      mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver);

  WebAppInternalsHandler(const WebAppInternalsHandler&) = delete;
  WebAppInternalsHandler& operator=(const WebAppInternalsHandler&) = delete;

  ~WebAppInternalsHandler() override;

  // mojom::WebAppInternalsHandler:
  void GetDebugInfoAsJsonString(
      GetDebugInfoAsJsonStringCallback callback) override;

 private:
  const raw_ptr<Profile> profile_;
  mojo::Receiver<mojom::WebAppInternalsHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
