// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_CHROME_COLORS_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_CHROME_COLORS_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace chrome_colors {

class ChromeColorsService;

// Singleton that owns all ChromeColorsServices and associates them with
// Profiles.
class ChromeColorsFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ChromeColorsService for |profile|.
  static ChromeColorsService* GetForProfile(Profile* profile);

  static ChromeColorsFactory* GetInstance();

  ChromeColorsFactory(const ChromeColorsFactory&) = delete;
  ChromeColorsFactory& operator=(const ChromeColorsFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ChromeColorsFactory>;

  ChromeColorsFactory();
  ~ChromeColorsFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace chrome_colors

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_CHROME_COLORS_CHROME_COLORS_FACTORY_H_
