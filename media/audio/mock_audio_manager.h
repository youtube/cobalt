// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_

#include "media/audio/audio_manager.h"

namespace media {

// This class is a simple mock around AudioManager, used exclusively for tests,
// which has the following purposes:
// 1) Avoids to use the actual (system and platform dependent) AudioManager.
//    Some bots does not have input devices, thus using the actual AudioManager
//    would causing failures on classes which expect that.
// 2) Allows the mock audio events to be dispatched on an arbitrary thread,
//    rather than forcing them on the audio thread, easing their handling in
//    browser tests (Note: sharing a thread can cause deadlocks on production
//    classes if WaitableEvents or any other form of lock is used for
//    synchronization purposes).
class MockAudioManager : public media::AudioManager {
 public:
  explicit MockAudioManager(base::MessageLoopProxy* message_loop_proxy);

  virtual bool HasAudioOutputDevices() override;

  virtual bool HasAudioInputDevices() override;

  virtual string16 GetAudioInputDeviceModel() override;

  virtual bool CanShowAudioInputSettings() override;

  virtual void ShowAudioInputSettings() override;

  virtual void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override;

  virtual media::AudioOutputStream* MakeAudioOutputStream(
      const media::AudioParameters& params) override;

  virtual media::AudioOutputStream* MakeAudioOutputStreamProxy(
      const media::AudioParameters& params) override;

  virtual media::AudioInputStream* MakeAudioInputStream(
      const media::AudioParameters& params,
      const std::string& device_id) override;

  virtual bool IsRecordingInProcess() override;

  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() override;

  virtual void AddOutputDeviceChangeListener(
      AudioDeviceListener* listener) override;
  virtual void RemoveOutputDeviceChangeListener(
      AudioDeviceListener* listener) override;

 private:
  virtual ~MockAudioManager();

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

}  // namespace media.

#endif  // MEDIA_AUDIO_MOCK_AUDIO_MANAGER_H_
