// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/test_audio_input_controller_factory.h"
#include "media/audio/audio_io.h"

namespace media {

TestAudioInputController::TestAudioInputController(
    TestAudioInputControllerFactory* factory,
    AudioManager* audio_manager,
    EventHandler* event_handler,
    SyncWriter* sync_writer)
    : AudioInputController(event_handler, sync_writer),
      factory_(factory),
      event_handler_(event_handler) {
  message_loop_ = audio_manager->GetMessageLoop();
}

void TestAudioInputController::Close(const base::Closure& closed_task) {
  message_loop_->PostTask(FROM_HERE, closed_task);
}

TestAudioInputController::~TestAudioInputController() {
  // Inform the factory so that it allows creating new instances in future.
  factory_->OnTestAudioInputControllerDestroyed(this);
}

TestAudioInputControllerFactory::TestAudioInputControllerFactory()
    : controller_(NULL) {
}

AudioInputController* TestAudioInputControllerFactory::Create(
    AudioManager* audio_manager,
    AudioInputController::EventHandler* event_handler,
    AudioParameters params) {
  DCHECK(!controller_);  // Only one test instance managed at a time.
  controller_ = new TestAudioInputController(this, audio_manager,
      event_handler, NULL);
  return controller_;
}

void TestAudioInputControllerFactory::OnTestAudioInputControllerDestroyed(
    TestAudioInputController* controller) {
  DCHECK_EQ(controller_, controller);
  controller_ = NULL;
}

}  // namespace media
