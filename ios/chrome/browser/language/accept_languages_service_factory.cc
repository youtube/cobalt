// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/accept_languages_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace {

// AcceptLanguagesServiceForBrowserState is a thin container for
// AcceptLanguagesService to enable associating it with a BrowserState.
class AcceptLanguagesServiceForBrowserState : public KeyedService {
 public:
  explicit AcceptLanguagesServiceForBrowserState(PrefService* prefs);

  AcceptLanguagesServiceForBrowserState(
      const AcceptLanguagesServiceForBrowserState&) = delete;
  AcceptLanguagesServiceForBrowserState& operator=(
      const AcceptLanguagesServiceForBrowserState&) = delete;

  ~AcceptLanguagesServiceForBrowserState() override;

  // Returns the associated AcceptLanguagesService.
  language::AcceptLanguagesService& accept_languages() {
    return accept_languages_;
  }

 private:
  language::AcceptLanguagesService accept_languages_;
};

AcceptLanguagesServiceForBrowserState::AcceptLanguagesServiceForBrowserState(
    PrefService* prefs)
    : accept_languages_(prefs, language::prefs::kAcceptLanguages) {}

AcceptLanguagesServiceForBrowserState::
    ~AcceptLanguagesServiceForBrowserState() {}

}  // namespace

// static
AcceptLanguagesServiceFactory* AcceptLanguagesServiceFactory::GetInstance() {
  static base::NoDestructor<AcceptLanguagesServiceFactory> instance;
  return instance.get();
}

// static
language::AcceptLanguagesService*
AcceptLanguagesServiceFactory::GetForBrowserState(ChromeBrowserState* state) {
  AcceptLanguagesServiceForBrowserState* service =
      static_cast<AcceptLanguagesServiceForBrowserState*>(
          GetInstance()->GetServiceForBrowserState(state, true));
  return &service->accept_languages();
}

AcceptLanguagesServiceFactory::AcceptLanguagesServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AcceptLanguagesServiceForBrowserState",
          BrowserStateDependencyManager::GetInstance()) {}

AcceptLanguagesServiceFactory::~AcceptLanguagesServiceFactory() {}

std::unique_ptr<KeyedService>
AcceptLanguagesServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<AcceptLanguagesServiceForBrowserState>(
      browser_state->GetPrefs());
}

web::BrowserState* AcceptLanguagesServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
