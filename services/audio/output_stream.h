// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_STREAM_H_
#define SERVICES_AUDIO_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/sync_socket.h"
#include "base/timer/timer.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/output_controller.h"
#include "services/audio/sync_reader.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media {
class AudioManager;
class AudioParameters;
}  // namespace media

namespace audio {
class OutputStream final : public media::mojom::AudioOutputStream,
                           public OutputController::EventHandler {
 public:
  using DeleteCallback = base::OnceCallback<void(OutputStream*)>;
  using CreatedCallback =
      base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>;
  using ManagedDeviceOutputStreamCreateCallback =
      OutputController::ManagedDeviceOutputStreamCreateCallback;

  OutputStream(
      CreatedCallback created_callback,
      DeleteCallback delete_callback,
      ManagedDeviceOutputStreamCreateCallback
          managed_device_output_stream_create_callback,
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      media::AudioManager* audio_manager,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      LoopbackCoordinator* coordinator,
      const base::UnguessableToken& loopback_group_id);

  OutputStream(const OutputStream&) = delete;
  OutputStream& operator=(const OutputStream&) = delete;

  ~OutputStream() final;

  // media::mojom::AudioOutputStream implementation.
  void Play() final;
  void Pause() final;
  void Flush() final;
  void SetVolume(double volume) final;

  // OutputController::EventHandler implementation.
  void OnControllerPlaying() final;
  void OnControllerPaused() final;
  void OnControllerError() final;
  void OnLog(base::StringPiece message) final;

 private:
  void CreateAudioPipe(CreatedCallback created_callback);
  void OnError();
  void CallDeleter();
  void PollAudioLevel();
  bool IsAudible();

  // Internal helper method for sending logs related  to this class to clients
  // registered to receive these logs. Prepends each log with "audio::OS" to
  // point out its origin. Compare with OutputController::EventHandler::OnLog()
  // which only will be called by the |controller_| object. These logs are
  // prepended with "AOC::" where AOC corresponds to AudioOutputController.
  void SendLogMessage(const char* format, ...) PRINTF_FORMAT(2, 3);

  SEQUENCE_CHECKER(owning_sequence_);

  base::CancelableSyncSocket foreign_socket_;
  DeleteCallback delete_callback_;
  mojo::Receiver<AudioOutputStream> receiver_;
  mojo::AssociatedRemote<media::mojom::AudioOutputStreamObserver> observer_;
  const mojo::SharedRemote<media::mojom::AudioLog> log_;
  const raw_ptr<LoopbackCoordinator> coordinator_;

  SyncReader reader_;
  OutputController controller_;
  // A token indicating membership in a group of output controllers/streams.
  const base::UnguessableToken loopback_group_id_;

  // This flag ensures that we only send OnStreamStateChanged notifications
  // and (de)register with the stream monitor when the state actually changes.
  bool playing_ = false;

  // Calls PollAudioLevel() at regular intervals while |playing_| is true.
  base::RepeatingTimer poll_timer_;
  bool is_audible_ = false;

  base::WeakPtrFactory<OutputStream> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_STREAM_H_
