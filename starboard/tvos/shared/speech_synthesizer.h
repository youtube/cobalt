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

#ifndef STARBOARD_TVOS_SHARED_SPEECH_SYNTHESIZER_H_
#define STARBOARD_TVOS_SHARED_SPEECH_SYNTHESIZER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief A speech synthesizer implemented using AVSpeechSynthesizer.
 */
@interface SBDSpeechSynthesizer : NSObject

/**
 *  @brief Indicates the screen reader is active and the system should try to
 *      perform text-to-speech for focused elements in the UI.
 */
@property(nonatomic, readonly, getter=isScreenReaderActive)
    BOOL screenReaderActive;

/**
 *  @brief Speaks an utterance using text-to-speech.
 *  @param string The text to speak.
 */
- (void)speak:(NSString*)string;

/**
 *  @brief Stops speaking any currently playing or queued utterances.
 */
- (void)cancelAllUtterances;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_SPEECH_SYNTHESIZER_H_
