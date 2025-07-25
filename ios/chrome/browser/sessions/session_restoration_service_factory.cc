// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_restoration_service_factory.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/sessions/session_restoration_service_impl.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/tabs/model/features.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#include "ios/web/common/features.h"

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SessionRestorationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SessionRestorationServiceFactory*
SessionRestorationServiceFactory::GetInstance() {
  static base::NoDestructor<SessionRestorationServiceFactory> instance;
  return instance.get();
}

SessionRestorationServiceFactory::SessionRestorationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SessionRestorationService",
          BrowserStateDependencyManager::GetInstance()) {}

SessionRestorationServiceFactory::~SessionRestorationServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionRestorationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(web::features::UseSessionSerializationOptimizations());
  return std::make_unique<SessionRestorationServiceImpl>(
      base::Seconds(5), IsPinnedTabsEnabled(),
      ChromeBrowserState::FromBrowserState(context)->GetStatePath(),
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED));
}

web::BrowserState* SessionRestorationServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
