// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/media/audio/cast_audio_manager_helper.h"
#include "media/audio/audio_manager_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// NOTE: CastAudioManager receives a |device_id| from the audio service, and
// passes it to CastAudioOutputStream as a |device_id_or_group_id|.
//
// The output stream interprets the |device_id_or_group_id| as a device_id if it
// the value matches a valid device_id, either kCommunicationsDeviceId or
// kDefaultDeviceId (or an empty string as kDefaultDeviceId). If
// |device_id_or_group_id| does not match a valid device_id, then it is
// interpreted as a group_id. group_id is used to determine the |session_id|
// from CastSessionIdMap. Multizone audio is only enabled for kDefaultDeviceId
// so the correct device_id can be inferred without conflict. |group_id| are
// uuid.
//
// At the top end of the audio stack, StreamFactory replaces the |device_id|
// with the |group_id| if the |group_id| is not empty. This implementation in
// StreamFactory is required for multizone audio for Cast devices using CAOS for
// their primary audio playback.

namespace chromecast {

namespace media {

class CastAudioMixer;
class CastAudioManagerTest;
class CastAudioOutputStreamTest;
class CmaBackendFactory;

class CastAudioManager : public ::media::AudioManagerBase {
 public:
  CastAudioManager(
      std::unique_ptr<::media::AudioThread> audio_thread,
      ::media::AudioLogFactory* audio_log_factory,
      CastAudioManagerHelper::Delegate* delegate,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      external_service_support::ExternalConnector* connector,
      bool use_mixer);

  CastAudioManager(const CastAudioManager&) = delete;
  CastAudioManager& operator=(const CastAudioManager&) = delete;

  ~CastAudioManager() override;

  // AudioManagerBase implementation.
  bool HasAudioOutputDevices() override;
  void GetAudioOutputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  const char* GetName() override;
  void ReleaseOutputStream(::media::AudioOutputStream* stream) override;

 protected:
  // AudioManagerBase implementation.
  ::media::AudioOutputStream* MakeLinearOutputStream(
      const ::media::AudioParameters& params,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioOutputStream* MakeLowLatencyOutputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id_or_group_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLinearInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLowLatencyInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const ::media::AudioParameters& input_params) override;

  // Generates a CastAudioOutputStream for |mixer_|.
  virtual ::media::AudioOutputStream* MakeMixerOutputStream(
      const ::media::AudioParameters& params);

 private:
  FRIEND_TEST_ALL_PREFIXES(CastAudioManagerTest, CanMakeStreamProxy);
  friend class CastAudioMixer;
  friend class CastAudioManagerTest;
  friend class CastAudioOutputStreamTest;

  CastAudioManager(
      std::unique_ptr<::media::AudioThread> audio_thread,
      ::media::AudioLogFactory* audio_log_factory,
      CastAudioManagerHelper::Delegate* delegate,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      external_service_support::ExternalConnector* connector,
      bool use_mixer,
      bool force_use_cma_backend_for_output);

  // Returns false if it is not appropriate to use the mixer service for output
  // stream audio playback.
  bool UseMixerOutputStream(const ::media::AudioParameters& params);

  CastAudioManagerHelper helper_;

  scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner_;
  std::unique_ptr<::media::AudioOutputStream> mixer_output_stream_;
  std::unique_ptr<CastAudioMixer> mixer_;

  // Let unit test force the CastOutputStream to uses
  // CmaBackend implementation.
  // TODO(b/117980762): After refactoring CastOutputStream, so
  // that the CastOutputStream has a unified output API, regardless
  // of the platform condition, then the unit test would be able to test
  // CastOutputStream properly.
  bool force_use_cma_backend_for_output_;

  // Weak pointers must be dereferenced on the |browser_task_runner|.
  base::WeakPtr<CastAudioManager> weak_this_;
  base::WeakPtrFactory<CastAudioManager> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
