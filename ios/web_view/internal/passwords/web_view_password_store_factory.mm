// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/sync/driver/sync_service.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
scoped_refptr<password_manager::PasswordStoreInterface>
WebViewPasswordStoreFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
WebViewPasswordStoreFactory* WebViewPasswordStoreFactory::GetInstance() {
  static base::NoDestructor<WebViewPasswordStoreFactory> instance;
  return instance.get();
}

WebViewPasswordStoreFactory::WebViewPasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "PasswordStore",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewPasswordStoreFactory::~WebViewPasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
WebViewPasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          context->GetStatePath()));

  scoped_refptr<base::SequencedTaskRunner> main_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());
  // USER_VISIBLE priority is chosen for the background task runner, because
  // the passwords obtained through tasks on the background runner influence
  // what the user sees.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));

  scoped_refptr<password_manager::PasswordStore> store =
      new password_manager::PasswordStore(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db)));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  return store;
}

web::BrowserState* WebViewPasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

bool WebViewPasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios_web_view
