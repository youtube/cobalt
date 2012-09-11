// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
#define MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_

#include <map>
#include <string>

#include "base/atomic_ref_count.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_manager.h"

namespace base {
class Thread;
}

namespace media {

class AudioOutputDispatcher;

// AudioManagerBase provides AudioManager functions common for all platforms.
class MEDIA_EXPORT AudioManagerBase : public AudioManager {
 public:
  // Name of the generic "default" device.
  static const char kDefaultDeviceName[];
  // Unique Id of the generic "default" device.
  static const char kDefaultDeviceId[];

  virtual ~AudioManagerBase();

  virtual void Init() OVERRIDE;

  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() OVERRIDE;

  virtual string16 GetAudioInputDeviceModel() OVERRIDE;

  virtual bool CanShowAudioInputSettings() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;

  virtual void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;

  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStreamProxy(
      const AudioParameters& params) OVERRIDE;

  virtual bool IsRecordingInProcess() OVERRIDE;

  // Called internally by the audio stream when it has been closed.
  virtual void ReleaseOutputStream(AudioOutputStream* stream);
  virtual void ReleaseInputStream(AudioInputStream* stream);

  void IncreaseActiveInputStreamCount();
  void DecreaseActiveInputStreamCount();

  // Creates the output stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) = 0;

  // Creates the output stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) = 0;

  // Creates the input stream for the |AUDIO_PCM_LINEAR| format. The legacy
  // name is also from |AUDIO_PCM_LINEAR|.
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) = 0;

  // Creates the input stream for the |AUDIO_PCM_LOW_LATENCY| format.
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) = 0;

  // Returns the preferred hardware audio output parameters for opening output
  // streams in the |AUDIO_PCM_LOW_LATENCY| format.
  // TODO(dalecurtis): Retrieve the |channel_layout| value from hardware instead
  // of accepting the value.
  // TODO(dalecurtis): Each AudioManager should implement their own version, see
  // http://crbug.com/137326
  virtual AudioParameters GetPreferredLowLatencyOutputStreamParameters(
      ChannelLayout channel_layout);

 protected:
  AudioManagerBase();

  typedef std::map<AudioParameters, scoped_refptr<AudioOutputDispatcher>,
                   AudioParameters::Compare>
      AudioOutputDispatchersMap;

  // Shuts down the audio thread and releases all the audio output dispatchers
  // on the audio thread.  All audio streams should be freed before
  // Shutdown is called.
  // This must be called in the destructor of the AudioManager<Platform>.
  void Shutdown();

  void ShutdownOnAudioThread();

  void SetMaxOutputStreamsAllowed(int max) { max_num_output_streams_ = max; }

  // Thread used to interact with AudioOutputStreams created by this
  // audio manger.
  scoped_ptr<base::Thread> audio_thread_;
  mutable base::Lock audio_thread_lock_;

  // Map of cached AudioOutputDispatcher instances.  Must only be touched
  // from the audio thread (no locking).
  AudioOutputDispatchersMap output_dispatchers_;

 private:
  // Counts the number of active input streams to find out if something else
  // is currently recording in Chrome.
  base::AtomicRefCount num_active_input_streams_;

  // Max number of open output streams, modified by
  // SetMaxOutputStreamsAllowed().
  int max_num_output_streams_;

  // Max number of open input streams.
  int max_num_input_streams_;

  // Number of currently open output streams.
  int num_output_streams_;

  // Number of currently open input streams.
  int num_input_streams_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerBase);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_MANAGER_BASE_H_
