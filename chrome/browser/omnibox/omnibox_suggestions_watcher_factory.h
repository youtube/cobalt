// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_OMNIBOX_SUGGESTIONS_WATCHER_FACTORY_H_
#define CHROME_BROWSER_OMNIBOX_OMNIBOX_SUGGESTIONS_WATCHER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class OmniboxSuggestionsWatcher;

class OmniboxSuggestionsWatcherFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static OmniboxSuggestionsWatcher* GetForBrowserContext(
      content::BrowserContext* context);

  static OmniboxSuggestionsWatcherFactory* GetInstance();

  OmniboxSuggestionsWatcherFactory(const OmniboxSuggestionsWatcherFactory&) =
      delete;
  OmniboxSuggestionsWatcherFactory& operator=(
      const OmniboxSuggestionsWatcherFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<OmniboxSuggestionsWatcherFactory>;

  OmniboxSuggestionsWatcherFactory();
  ~OmniboxSuggestionsWatcherFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_OMNIBOX_OMNIBOX_SUGGESTIONS_WATCHER_FACTORY_H_
