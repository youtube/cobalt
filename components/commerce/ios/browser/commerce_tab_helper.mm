// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/ios/browser/commerce_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace commerce {

CommerceTabHelper::CommerceTabHelper(web::WebState* state,
                                     bool is_off_the_record,
                                     ShoppingService* shopping_service)
    : is_off_the_record_(is_off_the_record),
      web_wrapper_(std::make_unique<WebStateWrapper>(state)),
      shopping_service_(shopping_service) {
  scoped_observation_.Observe(state);

  if (shopping_service_)
    shopping_service_->WebWrapperCreated(web_wrapper_.get());
}

CommerceTabHelper::~CommerceTabHelper() = default;

void CommerceTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!shopping_service_ ||
      previous_main_frame_url_ == navigation_context->GetUrl()) {
    return;
  }

  // Notify the service that we're no longer interested in a particular URL.
  shopping_service_->DidNavigateAway(web_wrapper_.get(),
                                     previous_main_frame_url_);
  previous_main_frame_url_ = navigation_context->GetUrl();

  shopping_service_->DidNavigatePrimaryMainFrame(web_wrapper_.get());
}

void CommerceTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!shopping_service_)
    return;

  shopping_service_->DidFinishLoad(web_wrapper_.get());
}

void CommerceTabHelper::WebStateDestroyed(web::WebState* web_state) {
  if (shopping_service_)
    shopping_service_->WebWrapperDestroyed(web_wrapper_.get());

  web_wrapper_->ClearWebStatePointer();

  // This needs to be reset prior to the destruction of the web state in order
  // to prevent a CHECK failure for observer count.
  scoped_observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(CommerceTabHelper)

}  // namespace commerce
