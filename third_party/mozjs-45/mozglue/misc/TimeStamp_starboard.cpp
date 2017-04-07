/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Port "TimeStamp_posix.cpp" to starboard.
//

#include "mozilla/Snprintf.h"
#include "mozilla/TimeStamp.h"

#include "starboard/time.h"

// Estimate of the smallest duration of time we can measure.
static uint64_t sResolution;
static uint64_t sResolutionSigDigs;

static const uint16_t kNsPerUs   =       1000;
static const uint64_t kNsPerMs   =    1000000;
static const uint64_t kNsPerSec  = 1000000000;
static const double kNsPerMsd    =    1000000.0;
static const double kNsPerSecd   = 1000000000.0;

static uint64_t
ClockTimeNs()
{
  return SbTimeGetMonotonicNow() * kSbTimeMillisecond;
}

static uint64_t
ClockResolutionNs()
{
  // NB: why not rely on clock_getres()?  Two reasons: (i) it might
  // lie, and (ii) it might return an "ideal" resolution that while
  // theoretically true, could never be measured in practice.  Since
  // clock_gettime() likely involves a system call on your platform,
  // the "actual" timing resolution shouldn't be lower than syscall
  // overhead.

  uint64_t start = ClockTimeNs();
  uint64_t end = ClockTimeNs();
  uint64_t minres = (end - start);

  // 10 total trials is arbitrary: what we're trying to avoid by
  // looping is getting unlucky and being interrupted by a context
  // switch or signal, or being bitten by paging/cache effects
  for (int i = 0; i < 9; ++i) {
    start = ClockTimeNs();
    end = ClockTimeNs();

    uint64_t candidate = (start - end);
    if (candidate < minres) {
      minres = candidate;
    }
  }

  if (0 == minres) {
    // clock_getres probably failed.  fall back on NSPR's resolution
    // assumption
    minres = 1 * kNsPerMs;
  }

  return minres;
}

namespace mozilla {

double
BaseTimeDurationPlatformUtils::ToSeconds(int64_t aTicks)
{
  return double(aTicks) / kNsPerSecd;
}

double
BaseTimeDurationPlatformUtils::ToSecondsSigDigits(int64_t aTicks)
{
  // don't report a value < mResolution ...
  int64_t valueSigDigs = sResolution * (aTicks / sResolution);
  // and chop off insignificant digits
  valueSigDigs = sResolutionSigDigs * (valueSigDigs / sResolutionSigDigs);
  return double(valueSigDigs) / kNsPerSecd;
}

int64_t
BaseTimeDurationPlatformUtils::TicksFromMilliseconds(double aMilliseconds)
{
  double result = aMilliseconds * kNsPerMsd;
  if (result > INT64_MAX) {
    return INT64_MAX;
  } else if (result < INT64_MIN) {
    return INT64_MIN;
  }

  return result;
}

int64_t
BaseTimeDurationPlatformUtils::ResolutionInTicks()
{
  return static_cast<int64_t>(sResolution);
}

static bool gInitialized = false;

void
TimeStamp::Startup()
{
  if (gInitialized) {
    return;
  }

  sResolution = ClockResolutionNs();

  // find the number of significant digits in sResolution, for the
  // sake of ToSecondsSigDigits()
  for (sResolutionSigDigs = 1;
       !(sResolutionSigDigs == sResolution ||
         10 * sResolutionSigDigs > sResolution);
       sResolutionSigDigs *= 10);

  gInitialized = true;

  return;
}

void
TimeStamp::Shutdown()
{
}

TimeStamp
TimeStamp::Now(bool aHighResolution)
{
  return TimeStamp(ClockTimeNs());
}

uint64_t
TimeStamp::ComputeProcessUptime()
{
  return 0;
}

} // namespace mozilla
