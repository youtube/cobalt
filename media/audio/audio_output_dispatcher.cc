// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher.h"

#include "base/message_loop.h"

namespace media {

AudioOutputDispatcher::AudioOutputDispatcher(
    AudioManager* audio_manager,
    const AudioParameters& params)
    : audio_manager_(audio_manager),
      message_loop_(MessageLoop::current()),
      params_(params) {
  // We expect to be instantiated on the audio thread.  Otherwise the
  // message_loop_ member will point to the wrong message loop!
  DCHECK(audio_manager->GetMessageLoop()->BelongsToCurrentThread());
}

AudioOutputDispatcher::~AudioOutputDispatcher() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
}

}  // namespace media
