// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/fake_output_stream.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

FakeOutputStream::FakeOutputStream()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

FakeOutputStream::~FakeOutputStream() {
  if (created_callback_) {
    std::move(created_callback_).Run(nullptr);
  }
}

void FakeOutputStream::CreateOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(params.IsValid());

  params_ = params;
  created_callback_ = std::move(callback);

  stream_receiver_.Bind(std::move(stream));
  stream_receiver_.set_disconnect_handler(base::BindOnce(
      &FakeOutputStream::OnConnectionError, base::Unretained(this)));

  reader_ = std::make_unique<audio::SyncReader>(base::DoNothing(), params,
                                                &client_socket_);
  DCHECK(reader_->IsValid());
  audio_bus_ = media::AudioBus::Create(params_);

  std::move(created_callback_)
      .Run({std::in_place, reader_->TakeSharedMemoryRegion(),
            mojo::PlatformHandle(client_socket_.Take())});
}

void FakeOutputStream::CreateSwitchableOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
    mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
        device_switch_receiver,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback created_callback) {
  CreateOutputStream(std::move(stream_receiver), std::move(observer),
                     std::move(log), output_device_id, params, group_id,
                     std::move(created_callback));
}

void FakeOutputStream::Play() {
  play_count_++;
  if (play_loop_) {
    play_loop_->Quit();
  }
}

void FakeOutputStream::Pause() {
  pause_count_++;
  if (pause_loop_) {
    pause_loop_->Quit();
  }
}

void FakeOutputStream::ExpectPlay() {
  expected_play_count_++;
  if (play_count_ < expected_play_count_) {
    play_loop_ = std::make_unique<base::RunLoop>();
    play_loop_->Run();
    play_loop_.reset();
  }
}

int FakeOutputStream::ConsumeAllAudioFrames() {
  DCHECK(reader_);
  DCHECK(audio_bus_);

  media::AudioGlitchInfo glitch_info;
  base::TimeTicks time_stamp = base::TimeTicks::Now();
  base::TimeDelta delay;
  int total_frames = 0;
  int last_buffer_active_frames = 0;

  // We stop consuming on the first silent buffer after seeing some active
  // audio. This is fast and works for our test sounds, but if a sound has
  // temporary gaps of absolute silence longer than one buffer duration,
  // we might stop prematurely. Consider checking for multiple consecutive
  // silent buffers if this becomes an issue.
  bool seen_non_silence = false;
  while (true) {
    reader_->RequestMoreData(delay, time_stamp, glitch_info);
    if (!reader_->Read(audio_bus_.get(), false)) {
      break;
    }
    const bool is_silent = audio_bus_->AreFramesZero();
    if (!is_silent) {
      seen_non_silence = true;
      int last_non_silent_frame = -1;
      for (const auto& channel : audio_bus_->AllChannels()) {
        auto it = std::find_if(channel.rbegin(), channel.rend(),
                               [](float sample) { return sample != 0.0f; });
        if (it != channel.rend()) {
          int index = channel.size() - 1 - std::distance(channel.rbegin(), it);
          last_non_silent_frame = std::max(last_non_silent_frame, index);
        }
      }
      last_buffer_active_frames = last_non_silent_frame + 1;
    }
    if (seen_non_silence && is_silent) {
      break;
    }
    total_frames += audio_bus_->frames();
  }

  if (!seen_non_silence) {
    return 0;
  }

  // Adjust total_frames: remove the padding of the last active buffer.
  total_frames =
      total_frames - params_.frames_per_buffer() + last_buffer_active_frames;

  return total_frames;
}

void FakeOutputStream::OnConnectionError() {
  stream_receiver_.reset();
  if (disconnect_loop_) {
    disconnect_loop_->Quit();
  }
}

base::RepeatingCallback<
    void(mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>
FakeOutputStream::GetBinder() {
  return base::BindRepeating(&FakeOutputStream::BindReceiver,
                             base::Unretained(this));
}

void FakeOutputStream::BindReceiver(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeOutputStream::BindReceiverOnMainThread,
                                base::Unretained(this), std::move(receiver)));
}

void FakeOutputStream::BindReceiverOnMainThread(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void FakeOutputStream::ExpectPause() {
  expected_pause_count_++;
  if (pause_count_ < expected_pause_count_) {
    pause_loop_ = std::make_unique<base::RunLoop>();
    pause_loop_->Run();
    pause_loop_.reset();
  }
}

void FakeOutputStream::ExpectDisconnect() {
  if (!stream_receiver_.is_bound()) {
    return;
  }
  disconnect_loop_ = std::make_unique<base::RunLoop>();
  disconnect_loop_->Run();
  disconnect_loop_.reset();
}

int FakeOutputStream::ReadBuffers(int count) {
  DCHECK(reader_);
  DCHECK(audio_bus_);
  int total_frames = 0;
  media::AudioGlitchInfo glitch_info;
  base::TimeTicks time_stamp = base::TimeTicks::Now();
  base::TimeDelta delay;

  for (int i = 0; i < count; ++i) {
    reader_->RequestMoreData(delay, time_stamp, glitch_info);
    if (!reader_->Read(audio_bus_.get(), false)) {
      break;
    }
    total_frames += audio_bus_->frames();
  }
  return total_frames;
}

}  // namespace audio
