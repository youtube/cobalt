// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_DEVICE_ENUMERATION_WIN_H_
#define MEDIA_AUDIO_WIN_DEVICE_ENUMERATION_WIN_H_

#include "media/audio/audio_device_name.h"

// Returns a list of audio input device structures (name and unique device ID)
// using the MMDevice API which is supported on Windows Vista and higher.
// Example record in the output list:
// - device_name: "Microphone (Realtek High Definition Audio)".
// - unique_id: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}"
// This method must be called from a COM thread using MTA.
bool GetInputDeviceNamesWin(media::AudioDeviceNames* device_names);

// Returns a list of audio input device structures (name and unique device ID)
// using the WaveIn API which is supported on Windows XP and higher.
// Example record in the output list:
// - device_name: "Microphone (Realtek High Defini".
// - unique_id: "Microphone (Realtek High Defini" (same as friendly name).
bool GetInputDeviceNamesWinXP(media::AudioDeviceNames* device_names);

#endif  // MEDIA_AUDIO_WIN_DEVICE_ENUMERATION_WIN_H_

