// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_STANDALONE_CAST_ENVIRONMENT_H_
#define MEDIA_CAST_TEST_UTILITY_STANDALONE_CAST_ENVIRONMENT_H_

#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/cast/cast_environment.h"

namespace media {
namespace cast {

// A complete CastEnvironment where all task runners are spurned from
// internally-owned threads.  Uses base::DefaultTickClock as a clock.
//
// A user of StandaloneCastEnvironment *must* call Shutdown() on the same thread
// that constructed the instance before the ref-count reaches zero.
// http://crbug.com/396480
class StandaloneCastEnvironment : public CastEnvironment,
                                  public base::ThreadChecker {
 public:
  StandaloneCastEnvironment();

  // Stops all threads backing the task runners, blocking the caller until
  // complete.
  void Shutdown();

 protected:
  ~StandaloneCastEnvironment() override;

  base::Thread main_thread_;
  base::Thread audio_thread_;
  base::Thread video_thread_;

  DISALLOW_COPY_AND_ASSIGN(StandaloneCastEnvironment);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_STANDALONE_CAST_ENVIRONMENT_H_
