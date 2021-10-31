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

#if defined(STARBOARD)
#include "angle_hdr.h"

#include "starboard/atomic.h"
#include "starboard/common/log.h"

namespace angle
{

starboard::atomic_int32_t hdr_angle_mode_enable(0);

}

void SetHdrAngleModeEnabled(bool flag)
{
    if (!flag && angle::hdr_angle_mode_enable.load() == 0)
    {
        return;
    }
    angle::hdr_angle_mode_enable.fetch_add(flag ? 1 : -1);
    SB_DCHECK(angle::hdr_angle_mode_enable.load() >= 0);
}

bool IsHdrAngleModeEnabled()
{
    return angle::hdr_angle_mode_enable.load() > 0;
}
#endif  // STARBOARD
