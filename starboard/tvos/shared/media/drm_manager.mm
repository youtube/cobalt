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

#import "starboard/tvos/shared/media/drm_manager.h"

#include "starboard/drm.h"
#import "starboard/tvos/shared/defines.h"
#import "starboard/tvos/shared/media/application_drm_system.h"
#import "starboard/tvos/shared/starboard_application.h"

@implementation SBDDrmManager {
  /**
   *  @brief This variable is responsible for maintaining the set of all
   *      current DRM systems.
   *  @remarks The contents of the set should only be mutated on the main
   *      thread.
   */
  NSMutableSet<SBDApplicationDrmSystem*>* _drmSystems;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _drmSystems = [NSMutableSet set];
  }
  return self;
}

- (SBDApplicationDrmSystem*)
        drmSystemWithContext:(void*)context
    sessionUpdateRequestFunc:(SbDrmSessionUpdateRequestFunc)updateRequestFunc
          sessionUpdatedFunc:(SbDrmSessionUpdatedFunc)updatedFunc {
  SBDApplicationDrmSystem* applicationDrmSystem =
      [[SBDApplicationDrmSystem alloc] initWithSessionContext:context
                                     sessionUpdateRequestFunc:updateRequestFunc
                                           sessionUpdatedFunc:updatedFunc];
  @synchronized(_drmSystems) {
    [_drmSystems addObject:applicationDrmSystem];
  }
  return applicationDrmSystem;
}

- (void)destroyDrmSystem:(SBDApplicationDrmSystem*)applicationDrmSystem {
  if (!applicationDrmSystem) {
    return;
  }
  @synchronized(_drmSystems) {
    [_drmSystems removeObject:applicationDrmSystem];
  }
}

- (bool)isApplicationDrmSystem:(SbDrmSystem)drmSystem {
  @synchronized(self) {
    for (SBDApplicationDrmSystem* applicationDrmSystem : _drmSystems) {
      if ([self
              starboardDrmSystemForApplicationDrmSystem:applicationDrmSystem] ==
          drmSystem) {
        return true;
      }
    }
    return false;
  }
}

- (SBDApplicationDrmSystem*)applicationDrmSystemForStarboardDrmSystem:
    (SbDrmSystem)starboardDrmSystem {
  if ([self isApplicationDrmSystem:starboardDrmSystem]) {
    return (__bridge SBDApplicationDrmSystem*)starboardDrmSystem;
  }
  return nil;
}

- (SbDrmSystem)starboardDrmSystemForApplicationDrmSystem:
    (SBDApplicationDrmSystem*)applicationDrmSystem {
  return (__bridge SbDrmSystem)applicationDrmSystem;
}

@end
