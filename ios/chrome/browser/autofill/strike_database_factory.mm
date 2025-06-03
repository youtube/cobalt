// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/strike_database_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/strike_databases/strike_database.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace autofill {

// static
StrikeDatabase* StrikeDatabaseFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<StrikeDatabase*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
StrikeDatabaseFactory* StrikeDatabaseFactory::GetInstance() {
  static base::NoDestructor<StrikeDatabaseFactory> instance;
  return instance.get();
}

StrikeDatabaseFactory::StrikeDatabaseFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillStrikeDatabase",
          BrowserStateDependencyManager::GetInstance()) {}

StrikeDatabaseFactory::~StrikeDatabaseFactory() {}

std::unique_ptr<KeyedService> StrikeDatabaseFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      chrome_browser_state->GetProtoDatabaseProvider();

  return std::make_unique<autofill::StrikeDatabase>(
      db_provider, chrome_browser_state->GetStatePath());
}

}  // namespace autofill
