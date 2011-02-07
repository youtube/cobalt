// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_TEST_AUDIO_INPUT_CONTROLLER_FACTORY_H_
#define MEDIA_AUDIO_TEST_AUDIO_INPUT_CONTROLLER_FACTORY_H_
#pragma once

#include "media/audio/audio_input_controller.h"

namespace media {

class TestAudioInputControllerFactory;

// TestAudioInputController and TestAudioInputControllerFactory are used for
// testing consumers of AudioInputController. TestAudioInputControllerFactory
// is a AudioInputController::Factory that creates TestAudioInputControllers.
//
// TestAudioInputController::Record and Close are overriden to do nothing. It is
// expected that you'll grab the EventHandler from the TestAudioInputController
// and invoke the callback methods when appropriate. In this way it's easy to
// mock a AudioInputController.
//
// Typical usage:
//   // Create and register factory.
//   TestAudioInputControllerFactory factory;
//   AudioInputController::set_factory(&factory);
//
//   // Do something that triggers creation of an AudioInputController.
//   TestAudioInputController* controller = factory.last_controller();
//   DCHECK(controller);
//
//   // Notify event handler with whatever data you want.
//   controller->event_handler()->OnCreated(...);
//
//   // Do something that triggers AudioInputController::Record to be called.
//   controller->event_handler()->OnData(...);
//   controller->event_handler()->OnError(...);
//
//   // Make sure consumer of AudioInputController does the right thing.
//   ...
//   // Reset factory.
//   AudioInputController::set_factory(NULL);

class TestAudioInputController : public AudioInputController {
 public:
  TestAudioInputController(TestAudioInputControllerFactory* factory,
                           EventHandler* event_handler);
  virtual ~TestAudioInputController();

  // Returns the event handler installed on the AudioInputController.
  EventHandler* event_handler() const { return event_handler_; }

  // Overriden to do nothing. It is assumed the caller will notify the event
  // handler with recorded data and other events.
  virtual void Record() {}
  virtual void Close() {}

 private:
  // These are not owned by us and expected to be valid for this object's
  // lifetime.
  TestAudioInputControllerFactory* factory_;
  EventHandler* event_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioInputController);
};

// Simple AudioInputController::Factory method that creates
// TestAudioInputControllers.
class TestAudioInputControllerFactory : public AudioInputController::Factory {
 public:
  TestAudioInputControllerFactory();

  // AudioInputController::Factory methods.
  virtual AudioInputController* Create(
      AudioInputController::EventHandler* event_handler,
      AudioParameters params);

  TestAudioInputController* controller() const { return controller_; }

 private:
  friend class TestAudioInputController;

  // Invoked by a TestAudioInputController when it gets destroyed.
  void OnTestAudioInputControllerDestroyed(
      TestAudioInputController* controller);

  // The caller of Create owns this object.
  TestAudioInputController* controller_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioInputControllerFactory);
};

}  // namespace media

#endif  // MEDIA_AUDIO_TEST_AUDIO_INPUT_CONTROLLER_FACTORY_H_
