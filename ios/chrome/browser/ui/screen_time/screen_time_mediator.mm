// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/screen_time/screen_time_mediator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/screen_time/screen_time_consumer.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ScreenTimeMediator () <WebStateListObserving, CRWWebStateObserver> {
  std::unique_ptr<WebStateListObserver> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
}
// This class updates ScreenTime with information from this WebStateList.
@property(nonatomic, assign, readonly) WebStateList* webStateList;
// Whether ScreenTime should be recording web usage.
@property(nonatomic, assign, readonly) BOOL suppressUsageRecording;
@end

@implementation ScreenTimeMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              suppressUsageRecording:(BOOL)suppressUsageRecording {
  self = [super init];
  if (self) {
    DCHECK(webStateList);
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());

    _suppressUsageRecording = suppressUsageRecording;
  }
  return self;
}

- (void)dealloc {
  // `-disconnect` must be called before deallocation.
  DCHECK(!_webStateList);
}

- (void)disconnect {
  if (self.webStateList) {
    self.webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
    _forwarder = nullptr;
    _observer = nullptr;
  }
}

#pragma mark - Public properties

- (void)setConsumer:(id<ScreenTimeConsumer>)consumer {
  _consumer = consumer;
  [consumer setSuppressUsageRecording:self.suppressUsageRecording];
  [self updateConsumer];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(self.webStateList, webStateList);
  [self updateConsumer];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(self.webStateList, webStateList);
  [self updateConsumer];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateConsumer];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self updateConsumer];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self updateConsumer];
}

#pragma mark - Private methods

- (void)updateConsumer {
  DCHECK(self.consumer);
  NSURL* activeURL = nil;
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (activeWebState) {
    activeURL = net::NSURLWithGURL(activeWebState->GetVisibleURL());
  }
  [self.consumer setURL:activeURL];
}

@end
