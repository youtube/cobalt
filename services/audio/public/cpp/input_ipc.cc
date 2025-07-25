// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/input_ipc.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "media/mojo/common/input_error_code_converter.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace audio {

InputIPC::InputIPC(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    mojo::PendingRemote<media::mojom::AudioLog> log)
    : device_id_(device_id),
      pending_stream_factory_(std::move(stream_factory)),
      log_(std::move(log)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InputIPC::~InputIPC() = default;

void InputIPC::CreateStream(media::AudioInputIPCDelegate* delegate,
                            const media::AudioParameters& params,
                            bool automatic_gain_control,
                            uint32_t total_segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  DCHECK(!delegate_);

  delegate_ = delegate;

  if (!stream_factory_.is_bound())
    stream_factory_.Bind(std::move(pending_stream_factory_));

  auto client = stream_client_receiver_.BindNewPipeAndPassRemote();

  // Unretained is safe because we own the receiver.
  stream_client_receiver_.set_disconnect_handler(
      base::BindOnce(&InputIPC::OnError, base::Unretained(this),
                     media::mojom::InputStreamErrorCode::kUnknown));

  // For now we don't care about key presses, so we pass a invalid buffer.
  base::ReadOnlySharedMemoryRegion invalid_key_press_count_buffer;

  mojo::PendingRemote<media::mojom::AudioLog> log;
  if (log_)
    log = log_.Unbind();

  stream_factory_->CreateInputStream(
      stream_.BindNewPipeAndPassReceiver(), std::move(client), {},
      std::move(log), device_id_, params, total_segments,
      automatic_gain_control, std::move(invalid_key_press_count_buffer),
      /*processing_config=*/nullptr,
      base::BindOnce(&InputIPC::StreamCreated, weak_factory_.GetWeakPtr()));
}

void InputIPC::StreamCreated(
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
    bool initially_muted,
    const absl::optional<base::UnguessableToken>& stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);

  if (data_pipe.is_null()) {
    OnError(media::mojom::InputStreamErrorCode::kUnknown);
    return;
  }

  // Keep the stream_id, if we get one. Regular input stream have stream ids,
  // but Loopback streams do not.
  stream_id_ = stream_id;

  DCHECK(data_pipe->socket.is_valid_platform_file());
  base::ScopedPlatformFile socket_handle = data_pipe->socket.TakePlatformFile();
  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  delegate_->OnStreamCreated(std::move(shared_memory_region),
                             std::move(socket_handle), initially_muted);
}

void InputIPC::RecordStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->Record();
}

void InputIPC::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->SetVolume(volume);
}

void InputIPC::SetOutputDeviceForAec(const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_factory_.is_bound());
  // Loopback streams have no stream ids and cannot be use echo cancellation
  if (stream_id_.has_value()) {
    stream_factory_->AssociateInputAndOutputForAec(*stream_id_,
                                                   output_device_id);
  }
}

void InputIPC::CloseStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = nullptr;
  if (stream_client_receiver_.is_bound())
    stream_client_receiver_.reset();
  stream_.reset();

  // Make sure we don't get any stale stream creation messages.
  weak_factory_.InvalidateWeakPtrs();
}

void InputIPC::OnError(media::mojom::InputStreamErrorCode code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);

  delegate_->OnError(media::ConvertToCaptureCallbackCode(code));
}

void InputIPC::OnMutedStateChanged(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnMuted(is_muted);
}

}  // namespace audio
