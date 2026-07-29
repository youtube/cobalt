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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AUDIO_QUEUE_BUFFER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AUDIO_QUEUE_BUFFER_H_

#import <AVFoundation/AVFoundation.h>

@class SBDAudioSource;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Wraps the @c AudioQueueBufferRef and manages adding the queue buffers
 *      to the @c AudioQueueRef.
 */
@interface SBDAudioQueueBuffer : NSObject

/**
 *  @brief A reference to the @c AudioQueueBufferRef that was created and added
 *      to the @c AudioQueueRef.
 */
@property(nonatomic, readonly) AudioQueueBufferRef buffer;

/**
 *  @brief Designated initializer.
 *  @remarks Creates a new @c AudioQueueBufferRef and adds it to the
 *      @c audioQueue.
 *  @param audioQueue The @c AudioQueueRef to add the newly created buffer to.
 *  @param size The size the buffer should be.
 */
- (instancetype)initWithAudioQueue:(AudioQueueRef)audioQueue
                              size:(uint32_t)size;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AUDIO_QUEUE_BUFFER_H_
