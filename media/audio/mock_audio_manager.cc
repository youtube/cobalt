// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_audio_manager.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"

namespace media {

MockAudioManager::MockAudioManager(base::MessageLoopProxy* message_loop_proxy)
    : message_loop_proxy_(message_loop_proxy) {
}

MockAudioManager::~MockAudioManager() {
}

bool MockAudioManager::HasAudioOutputDevices() {
  return true;
}

bool MockAudioManager::HasAudioInputDevices() {
  return true;
}

string16 MockAudioManager::GetAudioInputDeviceModel() {
  return string16();
}

bool MockAudioManager::CanShowAudioInputSettings() {
  return false;
}

void MockAudioManager::ShowAudioInputSettings() {
}

void MockAudioManager::GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) {
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStream(
        const media::AudioParameters& params) {
  NOTREACHED();
  return NULL;
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStreamProxy(
    const media::AudioParameters& params) {
  NOTREACHED();
  return NULL;
}

media::AudioInputStream* MockAudioManager::MakeAudioInputStream(
    const media::AudioParameters& params,
    const std::string& device_id) {
  NOTREACHED();
  return NULL;
}

void MockAudioManager::MuteAll() {
}

void MockAudioManager::UnMuteAll() {
}

bool MockAudioManager::IsRecordingInProcess() {
  return false;
}

scoped_refptr<base::MessageLoopProxy> MockAudioManager::GetMessageLoop() {
  return message_loop_proxy_;
}

void MockAudioManager::Init() {
}

}  // namespace media.
