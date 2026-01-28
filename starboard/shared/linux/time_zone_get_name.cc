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

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/time_zone.h"

#define TZDEFAULT "/etc/localtime"
#define TZZONEINFOTAIL "/zoneinfo/"
#define isNonDigit(ch) (ch < '0' || '9' < ch)

// Follow symlinks up to a reasonable depth to avoid infinite loops.
const int kMaxSymlinks = 8;

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
  /*
  This is a trick to look at the name of the link to get the Olson ID
  because the tzfile contents is underspecified.
  This isn't guaranteed to work because it may not be a symlink.
  But this is production-tested solution for most versions of Linux.
  */

  struct stat time_zone_stat;
  char time_zone_input_buffer[PATH_MAX];

  if (gTimeZoneBufferPtr == NULL) {
    // Copy default path into time_zone_input_buffer
    memcpy(time_zone_input_buffer, TZDEFAULT, sizeof(TZDEFAULT));

    // On some platforms (ex. NixOS FHSEnvs), /etc/localtime will be a
    // symlink chain:
    //
    //   $ readlink /etc/localtime
    //   /.host-etc/localtime
    //
    //   $ readlink /.host-etc/localtime
    //   /etc/zoneinfo/America/New_York
    //

    for (int i = 0; i < kMaxSymlinks; ++i) {
      int32_t ret = (int32_t)readlink(time_zone_input_buffer, gTimeZoneBuffer,
                                      sizeof(gTimeZoneBuffer) - 1);

      if (ret <= 0) {
        // Failed to read link, or it's not a link.
        break;
      }

      int32_t tz_zone_info_tail_len = sizeof(TZZONEINFOTAIL) - 1;
      gTimeZoneBuffer[ret] = 0;
      char* tz_zone_info_tail_ptr = strstr(gTimeZoneBuffer, TZZONEINFOTAIL);

      // Stop on the first symlink that is a valid time zone.
      if (tz_zone_info_tail_ptr != NULL &&
          isValidOlsonID(tz_zone_info_tail_ptr + tz_zone_info_tail_len)) {
        return (gTimeZoneBufferPtr =
                    tz_zone_info_tail_ptr + tz_zone_info_tail_len);
      }

      // Check if the target is another symlink to follow.
      if (gTimeZoneBuffer[0] != '/' ||
          lstat(gTimeZoneBuffer, &time_zone_stat) == -1 ||
          !S_ISLNK(time_zone_stat.st_mode)) {
        // Not an absolute path, lstat failed, or not a symlink. Stop.
        break;
      }

      // It is another symlink, copy path and continue loop.
      memcpy(time_zone_input_buffer, gTimeZoneBuffer, ret + 1);
    }

    SB_NOTREACHED();
    return "";
  } else {
    return gTimeZoneBufferPtr;
  }
}
