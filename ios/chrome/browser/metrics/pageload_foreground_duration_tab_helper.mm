// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/pageload_foreground_duration_tab_helper.h"

#import <UIKit/UIKit.h>

#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "services/metrics/public/cpp/ukm_builders.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(PageloadForegroundDurationTabHelper)

PageloadForegroundDurationTabHelper::PageloadForegroundDurationTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state);
  scoped_observation_.Observe(web_state);
  background_notification_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidEnterBackgroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                this->UpdateForAppDidBackground();
              }];
  foreground_notification_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationWillEnterForegroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                this->UpdateForAppWillForeground();
              }];
}

PageloadForegroundDurationTabHelper::~PageloadForegroundDurationTabHelper() {
  NSNotificationCenter* default_center = [NSNotificationCenter defaultCenter];
  [default_center removeObserver:foreground_notification_observer_];
  [default_center removeObserver:background_notification_observer_];
}

void PageloadForegroundDurationTabHelper::UpdateForAppWillForeground() {
  // Return early if not currently active WebState.
  if (!web_state_->IsVisible())
    return;
  last_time_shown_ = base::TimeTicks::Now();
  currently_recording_ = true;
}

void PageloadForegroundDurationTabHelper::UpdateForAppDidBackground() {
  // Return early if not currently active WebState.
  if (!web_state_->IsVisible())
    return;
  if (web_state_->IsWebPageInFullscreenMode()) {
    web_state_->CloseMediaPresentations();
  }
  RecordUkmIfInForeground();
}

void PageloadForegroundDurationTabHelper::WasShown(web::WebState* web_state) {
  if (!currently_recording_) {
    last_time_shown_ = base::TimeTicks::Now();
    currently_recording_ = true;
  }
}

void PageloadForegroundDurationTabHelper::WasHidden(web::WebState* web_state) {
  RecordUkmIfInForeground();
}

void PageloadForegroundDurationTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  // Do not start recording if the WebState is not visible. This is important to
  // not record for pre-rendering in the omnibox.
  // Do not log as end of recording for the current page session if the
  // navigation is same-document.
  if (!web_state_->IsVisible() || navigation_context->IsSameDocument())
    return;
  if (currently_recording_)
    RecordUkmIfInForeground();
  currently_recording_ = true;
  last_time_shown_ = base::TimeTicks::Now();
}

void PageloadForegroundDurationTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (!web_state_->IsVisible() || navigation_context->IsSameDocument()) {
    // Do not start recording if the WebState is not visible. This is important
    // to not record for pre-rendering in the omnibox. Do not log successful
    // navigation if it is same-document.
    return;
  }
  int has_committed = navigation_context->HasCommitted() ? 1 : 0;
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::MainFrameNavigation(source_id)
        .SetDidCommit(has_committed)
        .Record(ukm::UkmRecorder::Get());
  }
}

void PageloadForegroundDurationTabHelper::RenderProcessGone(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  if (!web_state_->IsVisible())
    return;
  RecordUkmIfInForeground();
}
void PageloadForegroundDurationTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  RecordUkmIfInForeground();
  DCHECK(scoped_observation_.IsObservingSource(web_state));
  scoped_observation_.Reset();
  web_state_ = nullptr;
}

void PageloadForegroundDurationTabHelper::RecordUkmIfInForeground() {
  if (!currently_recording_)
    return;
  currently_recording_ = false;
  base::TimeDelta foreground_duration =
      base::TimeTicks::Now() - last_time_shown_;
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::PageForegroundSession(source_id)
        .SetForegroundDuration(foreground_duration.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}
