// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AT_MEMORY_CROSS_TAB_COPY_PASTE_TRACKER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AT_MEMORY_CROSS_TAB_COPY_PASTE_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace autofill {

class AtMemoryCrossTabCopyPasteTracker;

class AtMemoryCrossTabCopyPasteTrackerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AtMemoryCrossTabCopyPasteTracker* GetForBrowserContext(
      content::BrowserContext* context);
  static AtMemoryCrossTabCopyPasteTrackerFactory* GetInstance();

 private:
  friend base::NoDestructor<AtMemoryCrossTabCopyPasteTrackerFactory>;

  AtMemoryCrossTabCopyPasteTrackerFactory();
  ~AtMemoryCrossTabCopyPasteTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AT_MEMORY_CROSS_TAB_COPY_PASTE_TRACKER_FACTORY_H_
