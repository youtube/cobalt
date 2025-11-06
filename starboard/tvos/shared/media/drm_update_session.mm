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

#include "starboard/drm.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/widevine/drm_system_widevine.h"
#import "starboard/tvos/shared/media/application_drm_system.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#include "starboard/tvos/shared/media/drm_system_platform.h"
#import "starboard/tvos/shared/starboard_application.h"

void SbDrmUpdateSession(SbDrmSystem drm_system,
                        int ticket,
                        const void* key,
                        int key_size,
                        const void* session_id,
                        int session_id_size) {
  if (!SbDrmSystemIsValid(drm_system)) {
    SB_DLOG(WARNING) << "Invalid drm system";
    return;
  }

  @autoreleasepool {
    SBDDrmManager* drmManager = SBDGetApplication().drmManager;
    if ([drmManager isApplicationDrmSystem:drm_system]) {
      SBDApplicationDrmSystem* applicationDrmSystem =
          [drmManager applicationDrmSystemForStarboardDrmSystem:drm_system];

      NSString* base64KeyString =
          [[NSString alloc] initWithBytes:(char*)key
                                   length:key_size
                                 encoding:NSUTF8StringEncoding];
      NSData* keyData =
          [[NSData alloc] initWithBase64EncodedString:base64KeyString
                                              options:0];
      NSData* sessionId = [NSData dataWithBytes:session_id
                                         length:session_id_size];

      [applicationDrmSystem updateSessionWithKey:keyData
                                          ticket:ticket
                                       sessionId:sessionId];
      return;
    }
  }

  using starboard::shared::uikit::DrmSystemPlatform;
  using starboard::shared::widevine::DrmSystemWidevine;
  SB_DCHECK(DrmSystemWidevine::IsDrmSystemWidevine(drm_system) ||
            DrmSystemPlatform::IsSupported(drm_system));
  drm_system->UpdateSession(ticket, key, key_size, session_id, session_id_size);
}
