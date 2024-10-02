// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/voice/text_to_speech_notification_handler.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_player.h"
#import "ios/chrome/browser/ui/voice/voice_search_notification_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TextToSpeechNotificationHandler ()
// The TextToSpeechPlayer handling playback.
@property(nonatomic, weak) TextToSpeechPlayer* TTSPlayer;
@end

@implementation TextToSpeechNotificationHandler
@synthesize enabled = _enabled;
@synthesize TTSPlayer = _TTSPlayer;

- (void)dealloc {
  [self cancelPlayback];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark Accessors

- (void)setEnabled:(BOOL)enabled {
  if (_enabled == enabled)
    return;
  _enabled = enabled;
  if (_enabled) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(audioReadyForPlayback:)
               name:kTTSAudioReadyForPlaybackNotification
             object:nil];
  } else {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self cancelPlayback];
  }
}

#pragma mark Public

- (void)cancelPlayback {
  [self.TTSPlayer cancelPlayback];
  self.TTSPlayer = nil;
}

#pragma mark Private

// Starts the TTS player sending `notification`.
- (void)audioReadyForPlayback:(NSNotification*)notification {
  self.TTSPlayer =
      base::mac::ObjCCastStrict<TextToSpeechPlayer>(notification.object);
  [self.TTSPlayer beginPlayback];
}

@end
