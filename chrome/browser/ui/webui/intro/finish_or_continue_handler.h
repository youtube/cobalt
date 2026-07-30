// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_FINISH_OR_CONTINUE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_FINISH_OR_CONTINUE_HANDLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/intro/finish_or_continue.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

enum class FinishOrContinueChoice {
  kStartBrowsing,
  kContinueEducation,
};

class FinishOrContinueHandler
    : public intro::mojom::FinishOrContinuePageHandler {
 public:
  using FinishOrContinueCallback =
      base::OnceCallback<void(FinishOrContinueChoice choice)>;

  FinishOrContinueHandler(
      FinishOrContinueCallback callback,
      mojo::PendingReceiver<intro::mojom::FinishOrContinuePageHandler> receiver);

  FinishOrContinueHandler(const FinishOrContinueHandler&) = delete;
  FinishOrContinueHandler& operator=(const FinishOrContinueHandler&) = delete;

  ~FinishOrContinueHandler() override;

  // intro::mojom::FinishOrContinuePageHandler:
  void StartBrowsing() override;
  void ContinueEducation() override;

 private:
  void HandleChoice(FinishOrContinueChoice choice);

  FinishOrContinueCallback callback_;
  mojo::Receiver<intro::mojom::FinishOrContinuePageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_FINISH_OR_CONTINUE_HANDLER_H_
