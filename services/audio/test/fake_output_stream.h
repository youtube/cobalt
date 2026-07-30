// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_FAKE_OUTPUT_STREAM_H_
#define SERVICES_AUDIO_TEST_FAKE_OUTPUT_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/sync_reader.h"

namespace audio {

// A test-only fake that acts as a media::mojom::AudioStreamFactory and
// media::mojom::AudioOutputStream. It intercepts the creation of audio output
// streams and allows the test to drive the rendering loop synchronously via
// SyncReader, capturing the output.
class FakeOutputStream : public FakeStreamFactory,
                         public media::mojom::AudioOutputStream {
 public:
  FakeOutputStream();
  ~FakeOutputStream() override;

  FakeOutputStream(const FakeOutputStream&) = delete;
  FakeOutputStream& operator=(const FakeOutputStream&) = delete;

  base::RepeatingCallback<
      void(mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>
  GetBinder();

  void BindReceiver(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver);

  // media::mojom::AudioStreamFactory overrides:
  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback callback) override;

  void CreateSwitchableOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
      mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
          device_switch_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) override;

  // media::mojom::AudioOutputStream overrides:
  void Play() override;
  void Pause() override;
  void Flush() override {}
  void SetVolume(double volume) override {}

  int play_count() const { return play_count_; }
  int pause_count() const { return pause_count_; }

  // Blocks until a Play(), Pause(), or disconnect event occurs.
  void ExpectPlay();
  void ExpectPause();
  void ExpectDisconnect();

  // Consumes all audio from the stream until it goes silent or closes.
  // Returns the number of active (non-silent) frames read.
  int ConsumeAllAudioFrames();

  // Reads a specific number of buffers from the stream.
  // Returns the number of frames read.
  int ReadBuffers(int count);

 private:
  void OnConnectionError();
  void BindReceiverOnMainThread(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver);

  mojo::Receiver<media::mojom::AudioOutputStream> stream_receiver_{this};

  media::AudioParameters params_;
  std::unique_ptr<audio::SyncReader> reader_;
  base::CancelableSyncSocket client_socket_;
  CreateOutputStreamCallback created_callback_;
  std::unique_ptr<media::AudioBus> audio_bus_;

  std::unique_ptr<base::RunLoop> play_loop_;
  std::unique_ptr<base::RunLoop> pause_loop_;
  std::unique_ptr<base::RunLoop> disconnect_loop_;
  int play_count_ = 0;
  int pause_count_ = 0;
  int expected_play_count_ = 0;
  int expected_pause_count_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_FAKE_OUTPUT_STREAM_H_
