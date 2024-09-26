// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/new_tab_page_uma.h"

#import "base/metrics/histogram_macros.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace new_tab_page_uma {

void RecordAction(bool is_incognito,
                  web::WebState* web_state,
                  ActionType action) {
  if (is_incognito || !web_state ||
      web_state->GetVisibleURL() != kChromeUINewTabURL) {
    return;
  }
  base::HistogramBase* counter = base::Histogram::FactoryGet(
      "NewTabPage.ActioniOS", 0, NUM_ACTION_TYPES, NUM_ACTION_TYPES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(action);
}

void RecordActionFromOmnibox(bool is_incognito,
                             web::WebState* web_state,
                             const GURL& url,
                             ui::PageTransition transition) {
  bool is_expecting_voice_search =
      VoiceSearchNavigationTabHelper::FromWebState(web_state)
          ->IsExpectingVoiceSearch();
  if (is_expecting_voice_search) {
    RecordAction(is_incognito, web_state, ACTION_NAVIGATED_USING_VOICE_SEARCH);
    return;
  }
  ui::PageTransition core_transition = static_cast<ui::PageTransition>(
      transition & ui::PAGE_TRANSITION_CORE_MASK);
  if (PageTransitionCoreTypeIs(core_transition,
                               ui::PAGE_TRANSITION_GENERATED)) {
    RecordAction(is_incognito, web_state, ACTION_SEARCHED_USING_OMNIBOX);
  } else {
    if (google_util::IsGoogleHomePageUrl(GURL(url))) {
      RecordAction(is_incognito, web_state,
                   ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE);
    } else {
      RecordAction(is_incognito, web_state, ACTION_NAVIGATED_USING_OMNIBOX);
    }
  }
}
}
