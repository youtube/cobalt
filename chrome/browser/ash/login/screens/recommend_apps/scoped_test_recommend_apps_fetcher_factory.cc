// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recommend_apps/scoped_test_recommend_apps_fetcher_factory.h"

namespace ash {

ScopedTestRecommendAppsFetcherFactory::ScopedTestRecommendAppsFetcherFactory(
    const RecommendAppsFetcher::FactoryCallback& factory_callback)
    : factory_callback_(factory_callback) {
  RecommendAppsFetcher::SetFactoryCallbackForTesting(&factory_callback_);
}

ScopedTestRecommendAppsFetcherFactory::
    ~ScopedTestRecommendAppsFetcherFactory() {
  RecommendAppsFetcher::SetFactoryCallbackForTesting(nullptr);
}

}  // namespace ash
