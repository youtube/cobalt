// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/common/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/chromium/media/base/audio_buffer.h"
#include "third_party/chromium/media/base/decoder_buffer.h"
#include "third_party/chromium/media/base/decrypt_config.h"
#include "third_party/chromium/media/base/subsample_entry.h"
#include "mojo/public/cpp/system/buffer.h"

namespace mojo {

// TODO(crbug.com/611224): Stop using TypeConverters.

// static
media_m96::mojom::DecryptConfigPtr
TypeConverter<media_m96::mojom::DecryptConfigPtr, media_m96::DecryptConfig>::Convert(
    const media_m96::DecryptConfig& input) {
  media_m96::mojom::DecryptConfigPtr mojo_decrypt_config(
      media_m96::mojom::DecryptConfig::New());
  mojo_decrypt_config->key_id = input.key_id();
  mojo_decrypt_config->iv = input.iv();
  mojo_decrypt_config->subsamples = input.subsamples();
  mojo_decrypt_config->encryption_scheme = input.encryption_scheme();
  mojo_decrypt_config->encryption_pattern = input.encryption_pattern();

  return mojo_decrypt_config;
}

// static
std::unique_ptr<media_m96::DecryptConfig>
TypeConverter<std::unique_ptr<media_m96::DecryptConfig>,
              media_m96::mojom::DecryptConfigPtr>::
    Convert(const media_m96::mojom::DecryptConfigPtr& input) {
  return std::make_unique<media_m96::DecryptConfig>(
      input->encryption_scheme, input->key_id, input->iv, input->subsamples,
      input->encryption_pattern);
}

// static
media_m96::mojom::DecoderBufferPtr
TypeConverter<media_m96::mojom::DecoderBufferPtr, media_m96::DecoderBuffer>::Convert(
    const media_m96::DecoderBuffer& input) {
  media_m96::mojom::DecoderBufferPtr mojo_buffer(
      media_m96::mojom::DecoderBuffer::New());
  if (input.end_of_stream()) {
    mojo_buffer->is_end_of_stream = true;
    return mojo_buffer;
  }

  mojo_buffer->is_end_of_stream = false;
  mojo_buffer->timestamp = input.timestamp();
  mojo_buffer->duration = input.duration();
  mojo_buffer->is_key_frame = input.is_key_frame();
  mojo_buffer->data_size = base::checked_cast<uint32_t>(input.data_size());
  mojo_buffer->front_discard = input.discard_padding().first;
  mojo_buffer->back_discard = input.discard_padding().second;

  // Note: The side data is always small, so this copy is okay.
  if (input.side_data()) {
    DCHECK_GT(input.side_data_size(), 0u);
    mojo_buffer->side_data.assign(input.side_data(),
                                  input.side_data() + input.side_data_size());
  }

  if (input.decrypt_config()) {
    mojo_buffer->decrypt_config =
        media_m96::mojom::DecryptConfig::From(*input.decrypt_config());
  }

  // TODO(dalecurtis): We intentionally do not serialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return mojo_buffer;
}

// static
scoped_refptr<media_m96::DecoderBuffer>
TypeConverter<scoped_refptr<media_m96::DecoderBuffer>,
              media_m96::mojom::DecoderBufferPtr>::
    Convert(const media_m96::mojom::DecoderBufferPtr& input) {
  if (input->is_end_of_stream)
    return media_m96::DecoderBuffer::CreateEOSBuffer();

  scoped_refptr<media_m96::DecoderBuffer> buffer(
      new media_m96::DecoderBuffer(input->data_size));

  if (!input->side_data.empty())
    buffer->CopySideDataFrom(input->side_data.data(), input->side_data.size());

  buffer->set_timestamp(input->timestamp);
  buffer->set_duration(input->duration);
  buffer->set_is_key_frame(input->is_key_frame);

  if (input->decrypt_config) {
    buffer->set_decrypt_config(
        input->decrypt_config.To<std::unique_ptr<media_m96::DecryptConfig>>());
  }

  media_m96::DecoderBuffer::DiscardPadding discard_padding(input->front_discard,
                                                       input->back_discard);
  buffer->set_discard_padding(discard_padding);

  // TODO(dalecurtis): We intentionally do not deserialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return buffer;
}

// static
media_m96::mojom::AudioBufferPtr
TypeConverter<media_m96::mojom::AudioBufferPtr, media_m96::AudioBuffer>::Convert(
    const media_m96::AudioBuffer& input) {
  media_m96::mojom::AudioBufferPtr buffer(media_m96::mojom::AudioBuffer::New());
  buffer->sample_format = input.sample_format_;
  buffer->channel_layout = input.channel_layout();
  buffer->channel_count = input.channel_count();
  buffer->sample_rate = input.sample_rate();
  buffer->frame_count = input.frame_count();
  buffer->end_of_stream = input.end_of_stream();
  buffer->timestamp = input.timestamp();

  if (input.data_) {
    DCHECK_GT(input.data_size(), 0u);
    buffer->data.assign(input.data_.get(),
                        input.data_.get() + input.data_size_);
  }

  return buffer;
}

// static
scoped_refptr<media_m96::AudioBuffer>
TypeConverter<scoped_refptr<media_m96::AudioBuffer>, media_m96::mojom::AudioBufferPtr>::
    Convert(const media_m96::mojom::AudioBufferPtr& input) {
  if (input->end_of_stream)
    return media_m96::AudioBuffer::CreateEOSBuffer();

  if (input->frame_count <= 0 ||
      static_cast<size_t>(input->sample_format) > media_m96::kSampleFormatMax ||
      static_cast<size_t>(input->channel_layout) > media_m96::CHANNEL_LAYOUT_MAX ||
      ChannelLayoutToChannelCount(input->channel_layout) !=
          input->channel_count) {
    LOG(ERROR) << "Receive an invalid audio buffer, replace it with EOS.";
    return media_m96::AudioBuffer::CreateEOSBuffer();
  }

  if (IsBitstream(input->sample_format)) {
    uint8_t* data = input->data.data();
    return media_m96::AudioBuffer::CopyBitstreamFrom(
        input->sample_format, input->channel_layout, input->channel_count,
        input->sample_rate, input->frame_count, &data, input->data.size(),
        input->timestamp);
  }

  // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
  // one in the case of interleaved data.
  std::vector<const uint8_t*> channel_ptrs(input->channel_count, nullptr);
  const size_t size_per_channel = input->data.size() / input->channel_count;
  DCHECK_EQ(0u, input->data.size() % input->channel_count);
  for (int i = 0; i < input->channel_count; ++i)
    channel_ptrs[i] = input->data.data() + i * size_per_channel;

  return media_m96::AudioBuffer::CopyFrom(
      input->sample_format, input->channel_layout, input->channel_count,
      input->sample_rate, input->frame_count, &channel_ptrs[0],
      input->timestamp);
}

}  // namespace mojo
