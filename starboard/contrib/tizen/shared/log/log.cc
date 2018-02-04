/*
* Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "starboard/log.h"

#include <dlog/dlog.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "COBALT"

void SbLog(SbLogPriority priority, const char* message) {
  SB_UNREFERENCED_PARAMETER(priority);
  log_priority dlog_priority = DLOG_UNKNOWN;

  switch (priority) {
    case kSbLogPriorityInfo:
      dlog_priority = DLOG_INFO;
      break;
    case kSbLogPriorityWarning:
      dlog_priority = DLOG_WARN;
      break;
    case kSbLogPriorityError:
      dlog_priority = DLOG_ERROR;
      break;
    case kSbLogPriorityFatal:
      dlog_priority = DLOG_FATAL;
      break;
    default:
      break;
  }

  dlog_print(dlog_priority, LOG_TAG, "%s", message);
}
