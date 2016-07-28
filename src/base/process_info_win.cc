// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_info.h"

#include <windows.h>

#include "base/basictypes.h"
#include "base/time.h"

namespace {

using base::Time;

// Returns the process creation time, or NULL if an error occurred.
Time* ProcessCreationTimeInternal() {
  FILETIME creation_time = {};
  FILETIME ignore = {};
  if (::GetProcessTimes(::GetCurrentProcess(), &creation_time, &ignore,
      &ignore, &ignore) == false)
    return NULL;

  return new Time(Time::FromFileTime(creation_time));
}

}  // namespace

namespace base {

//static
const Time* CurrentProcessInfo::CreationTime() {
  static Time* process_creation_time = ProcessCreationTimeInternal();
  return process_creation_time;
}

}  // namespace base
