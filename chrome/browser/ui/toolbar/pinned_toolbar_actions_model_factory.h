// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_ACTIONS_MODEL_FACTORY_H_
#define CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_ACTIONS_MODEL_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

class PinnedToolbarActionsModel;

class PinnedToolbarActionsModelFactory : public ProfileKeyedServiceFactory {
 public:
  static PinnedToolbarActionsModel* GetForProfile(Profile* profile);

  static PinnedToolbarActionsModelFactory* GetInstance();

 private:
  friend base::NoDestructor<PinnedToolbarActionsModelFactory>;

  PinnedToolbarActionsModelFactory();
  ~PinnedToolbarActionsModelFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_ACTIONS_MODEL_FACTORY_H_
