// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mocks the opt guide service, to be used in unittests.
//
// Can be used with `ChromeRenderViewHostTestHarness` based tests.
//
// For non ChromeRenderViewHostTestHarness based tests set the local state using
// `TestingBrowserProcess::GetGlobal()->SetLocalState()`.
class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context);
  ~MockOptimizationGuideKeyedService() override;

  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>& urls,
       const base::flat_set<optimization_guide::proto::OptimizationType>&
           optimization_types,
       optimization_guide::proto::RequestContext request_context,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      ExecuteModel,
      (optimization_guide::proto::ModelExecutionFeature,
       const google::protobuf::MessageLite&,
       optimization_guide::OptimizationGuideModelExecutionResultCallback));
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
