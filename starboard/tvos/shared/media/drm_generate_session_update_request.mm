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

#include "starboard/common/log.h"
#include "starboard/drm.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/widevine/drm_system_widevine.h"
#import "starboard/tvos/shared/media/application_drm_system.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#include "starboard/tvos/shared/media/drm_system_platform.h"
#import "starboard/tvos/shared/starboard_application.h"

namespace {

/**
 *  @brief Unpacks data in the following format:
 *      [4 bytes | initData | 4 bytes | contentID | 4 bytes | certificationData]
 *      Where each of the |4 bytes| represent the length of the section that
 *      follows.
 */
NSArray<NSData*>* unpackData(NSData* input) {
  NSMutableArray<NSData*>* unpackedData = [NSMutableArray array];
  NSInteger offset = 0;
  UInt32 recordSize;
  for (NSInteger i = 0; i < 3; i++) {
    if (offset + sizeof(recordSize) > input.length) {
      return nil;
    }
    [input getBytes:&recordSize range:NSMakeRange(offset, sizeof(recordSize))];
    offset += sizeof(recordSize);
    if ((offset + recordSize) > input.length) {
      return nil;
    }
    NSData* recordData =
        [input subdataWithRange:NSMakeRange(offset, recordSize)];
    offset += recordSize;
    [unpackedData addObject:recordData];
  }
  return unpackedData;
}
}  // namespace

void SbDrmGenerateSessionUpdateRequest(SbDrmSystem drm_system,
                                       int ticket,
                                       const char* type,
                                       const void* initialization_data,
                                       int initialization_data_size) {
  if (!SbDrmSystemIsValid(drm_system)) {
    SB_DLOG(ERROR) << "Invalid DRM system.";
    return;
  }

  if (ticket == kSbDrmTicketInvalid) {
    SB_DLOG(ERROR) << "Ticket must be specified.";
    return;
  }

  @autoreleasepool {
    SBDDrmManager* drmManager = SBDGetApplication().drmManager;
    if ([drmManager isApplicationDrmSystem:drm_system]) {
      SBDApplicationDrmSystem* applicationDrmSystem =
          [drmManager applicationDrmSystemForStarboardDrmSystem:drm_system];

      NSData* packedData = [NSData dataWithBytes:initialization_data
                                          length:initialization_data_size];
      NSArray<NSData*>* unpackedData = unpackData(packedData);
      if (unpackedData.count < 3) {
        SB_DLOG(ERROR) << "Invalid initialization data.";
        return;
      }
      NSData* initData = unpackedData[0];
      NSData* contentID = unpackedData[1];
      NSData* certData = unpackedData[2];

      [applicationDrmSystem
          generateSessionUpdateRequestWithCertificationData:certData
                                          contentIdentifier:contentID
                                                   initData:initData
                                                     ticket:ticket];
      return;
    }
  }

  using starboard::shared::uikit::DrmSystemPlatform;
  using starboard::shared::widevine::DrmSystemWidevine;
  SB_DCHECK(DrmSystemWidevine::IsDrmSystemWidevine(drm_system) ||
            DrmSystemPlatform::IsSupported(drm_system));
  drm_system->GenerateSessionUpdateRequest(ticket, type, initialization_data,
                                           initialization_data_size);
}
