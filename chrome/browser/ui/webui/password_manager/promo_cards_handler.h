// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace password_manager {

class PromoCardInterface;

// A class allowing providing PasswordManager WebUI capability to dynamically
// display actionable promo cards depending on the current account preferences
// and already seen promos.
class PromoCardsHandler : public content::WebUIMessageHandler {
 public:
  PromoCardsHandler(
      Profile* profile,
      std::vector<std::unique_ptr<PromoCardInterface>> promo_cards);

  PromoCardsHandler(const PromoCardsHandler&) = delete;
  PromoCardsHandler& operator=(const PromoCardsHandler&) = delete;

  ~PromoCardsHandler() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleGetAvailablePromoCard(const base::Value::List& args);
  void HandleRecordPromoDismissed(const base::Value::List& args);

  PromoCardInterface* GetPromoToShowAndUpdatePref();

  raw_ptr<Profile> profile_;

  std::vector<std::unique_ptr<PromoCardInterface>> promo_cards_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_HANDLER_H_
