//
// Copyright 2020 Comcast Cable Communications Management, LLC
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
// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
//
// Based on code from ICU which is:
// Copyright 2016-2023 Unicode, Inc.
// and
// Copyright (c) 1995-2016 International Business Machines Corporation and others
// All rights reserved.
// Licensed under UNICODE LICENSE V3 and the ICU License from https://github.com/unicode-org/icu/blob/main/LICENSE

#include <limits.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/time_zone.h"

#define TZDEFAULT "/etc/localtime"
#define TZZONEINFOTAIL "/zoneinfo/"
#define isNonDigit(ch) (ch < '0' || '9' < ch)

static char gTimeZoneBuffer[PATH_MAX];
static char* gTimeZoneBufferPtr = NULL;

static bool isValidOlsonID(const char* id) {
  int32_t idx = 0;

  /* Determine if this is something like Iceland (Olson ID)
  or AST4ADT (non-Olson ID) */
  while (id[idx] && isNonDigit(id[idx]) && id[idx] != ',') {
    idx++;
  }

  /* If we went through the whole string, then it might be okay.
  The timezone is sometimes set to "CST-7CDT", "CST6CDT5,J129,J131/19:30",
  "GRNLNDST3GRNLNDDT" or similar, so we cannot use it.
  The rest of the time it could be an Olson ID. George */
  return static_cast<bool>(id[idx] == 0 || strcmp(id, "PST8PDT") == 0 ||
                           strcmp(id, "MST7MDT") == 0 ||
                           strcmp(id, "CST6CDT") == 0 ||
                           strcmp(id, "EST5EDT") == 0);
}

// Similar to how ICU::putil.cpp gets IANA(Olsen) timezone ID.
const char* SbTimeZoneGetName() {
  /* Check TZ variable */
  if (gTimeZoneBufferPtr == NULL) {
    const char* tz = getenv("TZ");
    if (tz && *tz == ':') ++tz;
    if (tz && isValidOlsonID(tz)) {
      strncpy(gTimeZoneBuffer, tz, sizeof(gTimeZoneBuffer));
      gTimeZoneBuffer[sizeof(gTimeZoneBuffer) - 1] = 0;
      gTimeZoneBufferPtr = &gTimeZoneBuffer[0];
    }
  }

  /*
  This is a trick to look at the name of the link to get the Olson ID
  because the tzfile contents is underspecified.
  This isn't guaranteed to work because it may not be a symlink.
  But this is production-tested solution for most versions of Linux.
  */

  if (gTimeZoneBufferPtr == NULL) {
    int32_t ret = (int32_t)readlink(TZDEFAULT, gTimeZoneBuffer,
                                    sizeof(gTimeZoneBuffer) - 1);
    if (0 < ret) {
      int32_t tzZoneInfoTailLen = strlen(TZZONEINFOTAIL);
      gTimeZoneBuffer[ret] = 0;
      char* tzZoneInfoTailPtr = strstr(gTimeZoneBuffer, TZZONEINFOTAIL);

      if (tzZoneInfoTailPtr != NULL &&
          isValidOlsonID(tzZoneInfoTailPtr + tzZoneInfoTailLen)) {
        return (gTimeZoneBufferPtr = tzZoneInfoTailPtr + tzZoneInfoTailLen);
      }
    }
    SB_NOTREACHED();
    return "";
  } else {
    return gTimeZoneBufferPtr;
  }
}
