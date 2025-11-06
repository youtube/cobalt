// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "starboard/tvos/shared/speech_synthesizer.h"

#import <UIKit/UIKit.h>

#import "starboard/common/time.h"

@implementation SBDSpeechSynthesizer {
  NSMutableArray<NSString*>* _pendingText;
  BOOL _speaking;

  __weak NSTimer* _speakTimer;
  int64_t _speakTimestamp;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _pendingText = [NSMutableArray array];
    _speaking = NO;
    _speakTimestamp = 0;

    NSNotificationCenter* notifications = NSNotificationCenter.defaultCenter;
    [notifications addObserver:self
                      selector:@selector(speakFinished:)
                          name:UIAccessibilityAnnouncementDidFinishNotification
                        object:nil];
    [notifications addObserver:self
                      selector:@selector(applicationDidBecomeActive)
                          name:UIApplicationDidBecomeActiveNotification
                        object:nil];
  }
  return self;
}

- (void)applicationDidBecomeActive {
  // The system notifies the user when application focus changes (e.g.
  // going from screen saver to application) via a series of beeps. The
  // notification will interrupt any VoiceOver announcements posted during
  // this time. Enforce a small delay on resume to avoid being interrupted
  // by the focus-changed notification.
  const int64_t kResumeDelayUsec = 1250000;
  _speakTimestamp = starboard::CurrentMonotonicTime() + kResumeDelayUsec;

  [self cancelAllUtterances];
}

- (BOOL)isScreenReaderActive {
  return UIAccessibilityIsVoiceOverRunning();
}

- (void)speak:(NSString*)text {
  if (text.length == 0) {
    return;
  }

  // Use the accessibility VoiceOver system to maintain a consistent voice
  // between native and non-native UI.
  @synchronized(_pendingText) {
    if (_speaking) {
      [_pendingText addObject:text];
    } else {
      _speaking = YES;
      [self voiceOverSpeak:text];
    }
  }
}

- (void)cancelAllUtterances {
  // There isn't an API to stop the current VoiceOver speech. However,
  // announcements immediately interrupt whatever is currently spoken, so
  // issuing a silent announcement will serve to cancel the current speech.
  // NOTE: An announcement with an empty string will be ignored; single
  // characters will be spoken / described (e.g. " " is pronounced "space").
  // As a workaround, use two spaces for silence. However, there is no
  // documentation that guarantees two spaces will continue to be silent. In
  // addition, speaking two spaces will lower the volume of content for a
  // few seconds -- just as if something was being spoken.
  @synchronized(_pendingText) {
    [_pendingText removeAllObjects];
    if (_speaking) {
      _speaking = NO;
      [self voiceOverSpeak:@"  "];
    }
  }
}

- (void)speakFinished:(NSNotification*)notification {
  @synchronized(_pendingText) {
    if (_pendingText.count == 0) {
      _speaking = NO;
    } else {
      NSString* text = _pendingText.firstObject;
      [_pendingText removeObjectAtIndex:0];
      [self voiceOverSpeak:text];
    }
  }
}

- (void)voiceOverSpeak:(NSString*)text {
  if (!UIAccessibilityIsVoiceOverRunning()) {
    _speaking = NO;
    return;
  }

  // Accessibility announcements appear to be throttled. If two announcements
  // are sent back-to-back, the second announcement may be dropped. Enforce a
  // delay between announcements to avoid dropping them.
  const NSTimeInterval kMinimumSpeakInterval = 0.05;

  // This Cobalt thread doesn't process run loop events, so use the main queue.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self->_speakTimer invalidate];
    NSTimeInterval delay =
        kMinimumSpeakInterval -
        (starboard::CurrentMonotonicTime() - self->_speakTimestamp) /
            static_cast<double>(1000000);

    // NSTimer clamps the interval to a minimum value, so no need to sanitize
    // |delay|.
    self->_speakTimer = [NSTimer
        scheduledTimerWithTimeInterval:delay
                               repeats:NO
                                 block:^(NSTimer* timer) {
                                   self->_speakTimestamp =
                                       starboard::CurrentMonotonicTime();
                                   UIAccessibilityPostNotification(
                                       UIAccessibilityAnnouncementNotification,
                                       text);
                                 }];
  });
}

@end
