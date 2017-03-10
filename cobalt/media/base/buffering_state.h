// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_BUFFERING_STATE_H_
#define COBALT_MEDIA_BASE_BUFFERING_STATE_H_

#include "base/callback_forward.h"

namespace cobalt {
namespace media {

enum BufferingState {
  // Indicates that there is no data buffered.
  //
  // Typical reason is data underflow and hence playback should be paused.
  BUFFERING_HAVE_NOTHING,

  // Indicates that enough data has been buffered.
  //
  // Typical reason is enough data has been prerolled to start playback.
  BUFFERING_HAVE_ENOUGH,
};

// Used to indicate changes in buffering state;
typedef base::Callback<void(BufferingState)> BufferingStateCB;

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_BUFFERING_STATE_H_
