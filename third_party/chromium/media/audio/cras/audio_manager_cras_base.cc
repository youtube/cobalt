// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/audio/cras/audio_manager_cras_base.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/field_trial_params.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/chromium/media/audio/audio_device_description.h"
#include "third_party/chromium/media/audio/audio_features.h"
#include "third_party/chromium/media/audio/cras/cras_input.h"
#include "third_party/chromium/media/audio/cras/cras_unified.h"
#include "third_party/chromium/media/base/channel_layout.h"
#include "third_party/chromium/media/base/limits.h"
#include "third_party/chromium/media/base/localized_strings.h"

namespace media_m96 {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 50;

}  // namespace

AudioManagerCrasBase::AudioManagerCrasBase(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCrasBase::~AudioManagerCrasBase() = default;

const char* AudioManagerCrasBase::GetName() {
  return "CRAS";
}

AudioOutputStream* AudioManagerCrasBase::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  // Pinning stream is not supported for MakeLinearOutputStream.
  return MakeOutputStream(params, AudioDeviceDescription::kDefaultDeviceId);
}

AudioOutputStream* AudioManagerCrasBase::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params, device_id);
}

AudioInputStream* AudioManagerCrasBase::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerCrasBase::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioOutputStream* AudioManagerCrasBase::MakeOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  return new CrasUnifiedStream(params, this, device_id);
}

AudioInputStream* AudioManagerCrasBase::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new CrasInputStream(params, this, device_id);
}

}  // namespace media_m96
