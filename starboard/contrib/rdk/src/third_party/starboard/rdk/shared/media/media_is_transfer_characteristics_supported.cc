//
// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#include "starboard/shared/starboard/media/media_support_internal.h"

#include "starboard/media.h"

SB_EXPORT bool SbMediaIsTransferCharacteristicsSupported(
    SbMediaTransferId transfer_id) {
  return transfer_id == kSbMediaTransferIdBt709 ||
         transfer_id == kSbMediaTransferIdSmpteSt2084 ||
         transfer_id == kSbMediaTransferIdAribStdB67 ||
         transfer_id == kSbMediaTransferIdUnspecified;
}
