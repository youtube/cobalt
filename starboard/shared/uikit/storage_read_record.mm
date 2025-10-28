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

#include "starboard/common/storage.h"

#import "starboard/shared/uikit/storage_internal.h"

int64_t SbStorageReadRecord(SbStorageRecord record,
                            char* out_data,
                            int64_t data_size) {
  if (!SbStorageIsValidRecord(record) || !out_data || data_size < 0) {
    return -1;
  }

  @autoreleasepool {
    NSString* recordName = (__bridge NSString*)record;
    NSData* recordData =
        [NSUserDefaults.standardUserDefaults dataForKey:recordName];

    if (!recordData) {
      return 0;
    }

    if (data_size > recordData.length) {
      data_size = recordData.length;
    }
    [recordData getBytes:out_data length:data_size];
    return data_size;
  }
}
