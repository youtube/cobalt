// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/android_youtube_drm.h"
#include "starboard/common/log.h"
#include "starboard/system.h"

bool SbSystemSignWithCertificationSecretKey(const uint8_t* message,
                                            size_t message_size_in_bytes,
                                            uint8_t* digest,
                                            size_t digest_size_in_bytes) {
  if (!message) {
    SB_DLOG(ERROR) << __FUNCTION__ << "Invalid para message";
    return false;
  }

  if (message_size_in_bytes <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << "Invalid para message_size_in_bytes"
                   << message_size_in_bytes;
    return false;
  }

  if (!digest) {
    SB_DLOG(ERROR) << __FUNCTION__ << "Invalid para digest";
    return false;
  }

  if (digest_size_in_bytes <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << "Invalid para digest_size_in_bytes"
                   << digest_size_in_bytes;
    return false;
  }

  return starboard::android::shared::YbMediaDrmSignMessage(
      message, message_size_in_bytes, digest, digest_size_in_bytes);
}
