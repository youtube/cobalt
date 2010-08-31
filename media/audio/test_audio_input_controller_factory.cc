// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/test_audio_input_controller_factory.h"
#include "media/audio/audio_io.h"

namespace media {

TestAudioInputController::TestAudioInputController(
    TestAudioInputControllerFactory* factory, EventHandler* event_handler)
    : AudioInputController(event_handler),
      factory_(factory),
      event_handler_(event_handler) {
}

TestAudioInputController::~TestAudioInputController() {
  // Inform the factory so that it allows creating new instances in future.
  factory_->OnTestAudioInputControllerDestroyed(this);
}

TestAudioInputControllerFactory::TestAudioInputControllerFactory()
    : controller_(NULL) {
}

AudioInputController* TestAudioInputControllerFactory::Create(
    AudioInputController::EventHandler* event_handler,
    AudioParameters params,
    int samples_per_packet) {
  DCHECK(!controller_);  // Only one test instance managed at a time.
  controller_ = new TestAudioInputController(this, event_handler);
  return controller_;
}

void TestAudioInputControllerFactory::OnTestAudioInputControllerDestroyed(
    TestAudioInputController* controller) {
  DCHECK(controller_ == controller);
  controller_ = NULL;
}

}  // namespace media
