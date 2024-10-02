// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/ios_chrome_hints_manager.h"

#import "components/optimization_guide/core/optimization_guide_features.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace optimization_guide {

IOSChromeHintsManager::IOSChromeHintsManager(
    bool off_the_record,
    const std::string& application_locale,
    PrefService* pref_service,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    optimization_guide::TopHostProvider* top_host_provider,
    optimization_guide::TabUrlProvider* tab_url_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OptimizationGuideLogger* optimization_guide_logger)
    : HintsManager(
          off_the_record,
          application_locale,
          pref_service,
          hint_store,
          top_host_provider,
          tab_url_provider,
          url_loader_factory,
          optimization_guide::features::IsPushNotificationsEnabled()
              ? std::make_unique<optimization_guide::PushNotificationManager>()
              : nullptr,
          optimization_guide_logger) {}

}  // namespace optimization_guide
