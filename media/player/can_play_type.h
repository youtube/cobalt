/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_PLAYER_CAN_PLAY_TYPE_H_
#define MEDIA_PLAYER_CAN_PLAY_TYPE_H_

#include <string>

namespace media {

// TODO: Find a better home for CanPlayType.  I don't want to make
// it a member function of WebMediaPlayer because of:
// 1. It is relatively complex.
// 2. The HTMLMediaElement doesn't always hold a WebMediaPlayer instance while
//    static function does't work here because it cannot be override in
//    inherited classes like WebMediaPlayerImpl.

// This function can return "", "maybe" or "probably" as defined in
// http://www.w3.org/TR/html5/embedded-content-0.html#dom-navigator-canplaytype
// to indicate how certain that the particular implementation can play videos in
// the format specified by content_type and key_system. Note that "" means the
// implementation cannot play such videos.
std::string CanPlayType(const std::string& content_type,
                        const std::string& key_system);

}  // namespace media

#endif  // MEDIA_PLAYER_CAN_PLAY_TYPE_H_
