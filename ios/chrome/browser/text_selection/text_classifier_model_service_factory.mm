// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/text_classifier_model_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/text_selection/text_classifier_model_service.h"
#import "ios/chrome/browser/text_selection/text_selection_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
TextClassifierModelServiceFactory*
TextClassifierModelServiceFactory::GetInstance() {
  static base::NoDestructor<TextClassifierModelServiceFactory> instance;
  return instance.get();
}

// static
TextClassifierModelService*
TextClassifierModelServiceFactory::GetForBrowserState(
    ChromeBrowserState* state) {
  return static_cast<TextClassifierModelService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

TextClassifierModelServiceFactory::TextClassifierModelServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TextClassifierModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

TextClassifierModelServiceFactory::~TextClassifierModelServiceFactory() {}

std::unique_ptr<KeyedService>
TextClassifierModelServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if ((!base::FeatureList::IsEnabled(kEnableExpKitCalendarTextClassifier) &&
       !base::FeatureList::IsEnabled(kEnableExpKitTextClassifier)) ||
      !optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    return nullptr;
  }

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  // The optimization guide service must be available for the text classifier
  // model service to be created.
  auto* opt_guide =
      OptimizationGuideServiceFactory::GetForBrowserState(browser_state);
  if (!opt_guide)
    return nullptr;
  return std::make_unique<TextClassifierModelService>(opt_guide);
}

web::BrowserState* TextClassifierModelServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool TextClassifierModelServiceFactory::ServiceIsCreatedWithBrowserState()
    const {
  return true;
}
