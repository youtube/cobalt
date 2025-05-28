// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/test_with_profile.h"

#import "base/functional/bind.h"
#import "base/test/test_file_util.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

TestWithProfile::TestWithProfile()
    : profile_data_dir_(base::CreateUniqueTempDirectoryScopedToTest()),
      profile_manager_(GetApplicationContext()->GetLocalState(),
                       profile_data_dir_) {
  TestingApplicationContext* application_context =
      TestingApplicationContext::GetGlobal();

  // IOSChromeIOThread needs to be created before the IO thread is started.
  // Thus DELAY_IO_THREAD_START is set in WebTaskEnvironment's options. The
  // thread is then started after the creation of IOSChromeIOThread.
  chrome_io_ = std::make_unique<IOSChromeIOThread>(
      application_context->GetLocalState(), application_context->GetNetLog());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      application_context->GetSystemIdentityManager(), &profile_manager_);

  // Register the objects with the TestingApplicationContext.
  application_context->SetIOSChromeIOThread(chrome_io_.get());
  application_context->SetProfileManagerAndAccountProfileMapper(
      &profile_manager_, account_profile_mapper_.get());

  // Initialize the prediction model store (required by some KeyedServices).
  optimization_guide::IOSChromePredictionModelStore::GetInstance()->Initialize(
      base::CreateUniqueTempDirectoryScopedToTest());

  // Start the IO thread.
  web_task_environment_.StartIOThread();

  // Post a task to initialize the IOSChromeIOThread object on the IO thread.
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                base::Unretained(chrome_io_.get())));

  // Init the BrowserPolicyConnect as this is required to create an instance
  // of ProfileIOSImpl.
  application_context->GetBrowserPolicyConnector()->Init(
      application_context->GetLocalState(),
      application_context->GetSharedURLLoaderFactory());

  // IOSChromeIOThread requires the SystemURLRequestContextGetter() to be
  // created before the object is shutdown, so force its creation here.
  std::ignore = chrome_io_->system_url_request_context_getter();
}

TestWithProfile::~TestWithProfile() {
  TestingApplicationContext* application_context =
      TestingApplicationContext::GetGlobal();

  // Cleanup the prediction model store (since it is a singleton).
  optimization_guide::IOSChromePredictionModelStore::GetInstance()
      ->ResetForTesting();

  // The profiles must be unloaded before the AccountProfileMapper gets
  // unregistered from the ApplicationContext, because keyed services may
  // depend on the AccountProfileMapper.
  profile_manager_.UnloadAllProfiles();

  application_context->GetBrowserPolicyConnector()->Shutdown();
  application_context->GetIOSChromeIOThread()->NetworkTearDown();
  application_context->SetProfileManagerAndAccountProfileMapper(nullptr,
                                                                nullptr);
  application_context->SetIOSChromeIOThread(nullptr);
}
