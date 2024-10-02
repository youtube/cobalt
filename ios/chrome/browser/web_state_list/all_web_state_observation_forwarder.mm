// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AllWebStateObservationForwarder::AllWebStateObservationForwarder(
    WebStateList* web_state_list,
    web::WebStateObserver* observer)
    : web_state_observations_(observer) {
  DCHECK(observer);
  DCHECK(web_state_list);
  web_state_list_observation_.Observe(web_state_list);

  for (int ii = 0; ii < web_state_list->count(); ++ii) {
    web::WebState* web_state = web_state_list->GetWebStateAt(ii);
    web_state_observations_.AddObservation(web_state);
  }
}

AllWebStateObservationForwarder::~AllWebStateObservationForwarder() {}

void AllWebStateObservationForwarder::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  web_state_observations_.AddObservation(web_state);
}

void AllWebStateObservationForwarder::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  web_state_observations_.RemoveObservation(old_web_state);
  web_state_observations_.AddObservation(new_web_state);
}

void AllWebStateObservationForwarder::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  web_state_observations_.RemoveObservation(web_state);
}
