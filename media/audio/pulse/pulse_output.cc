// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_output.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#if defined(OS_LINUX)
#include "media/audio/linux/audio_manager_linux.h"
#elif defined(OS_OPENBSD)
#include "media/audio/openbsd/audio_manager_openbsd.h"
#endif
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"

namespace media {

static pa_sample_format_t BitsToPASampleFormat(int bits_per_sample) {
  switch (bits_per_sample) {
    // Unsupported sample formats shown for reference.  I am assuming we want
    // signed and little endian because that is what we gave to ALSA.
    case 8:
      return PA_SAMPLE_U8;
      // Also 8-bits: PA_SAMPLE_ALAW and PA_SAMPLE_ULAW
    case 16:
      return PA_SAMPLE_S16LE;
      // Also 16-bits: PA_SAMPLE_S16BE (big endian).
    case 24:
      return PA_SAMPLE_S24LE;
      // Also 24-bits: PA_SAMPLE_S24BE (big endian).
      // Other cases: PA_SAMPLE_24_32LE (in LSB of 32-bit field, little endian),
      //          and PA_SAMPLE_24_32BE (in LSB of 32-bit field, big endian),
    case 32:
      return PA_SAMPLE_S32LE;
      // Also 32-bits: PA_SAMPLE_S32BE (big endian),
      //               PA_SAMPLE_FLOAT32LE (floating point little endian),
      //           and PA_SAMPLE_FLOAT32BE (floating point big endian).
    default:
      return PA_SAMPLE_INVALID;
  }
}

static pa_channel_position ChromiumToPAChannelPosition(Channels channel) {
  switch (channel) {
    // PulseAudio does not differentiate between left/right and
    // stereo-left/stereo-right, both translate to front-left/front-right.
    case LEFT:
    case STEREO_LEFT:
      return PA_CHANNEL_POSITION_FRONT_LEFT;
    case RIGHT:
    case STEREO_RIGHT:
      return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case CENTER:
      return PA_CHANNEL_POSITION_FRONT_CENTER;
    case LFE:
      return PA_CHANNEL_POSITION_LFE;
    case BACK_LEFT:
      return PA_CHANNEL_POSITION_REAR_LEFT;
    case BACK_RIGHT:
      return PA_CHANNEL_POSITION_REAR_RIGHT;
    case LEFT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case RIGHT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case BACK_CENTER:
      return PA_CHANNEL_POSITION_REAR_CENTER;
    case SIDE_LEFT:
      return PA_CHANNEL_POSITION_SIDE_LEFT;
    case SIDE_RIGHT:
      return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case CHANNELS_MAX:
      return PA_CHANNEL_POSITION_INVALID;
  }
  NOTREACHED() << "Invalid channel " << channel;
  return PA_CHANNEL_POSITION_INVALID;
}

static pa_channel_map ChannelLayoutToPAChannelMap(
    ChannelLayout channel_layout) {
  // Initialize channel map.
  pa_channel_map channel_map;
  pa_channel_map_init(&channel_map);

  channel_map.channels = ChannelLayoutToChannelCount(channel_layout);

  // All channel maps have the same size array of channel positions.
  for (unsigned int channel = 0; channel != CHANNELS_MAX; ++channel) {
    int channel_position = kChannelOrderings[channel_layout][channel];
    if (channel_position > -1) {
      channel_map.map[channel_position] = ChromiumToPAChannelPosition(
          static_cast<Channels>(channel));
    } else {
      // PulseAudio expects unused channels in channel maps to be filled with
      // PA_CHANNEL_POSITION_MONO.
      channel_map.map[channel_position] = PA_CHANNEL_POSITION_MONO;
    }
  }

  // Fill in the rest of the unused channels.
  for (unsigned int channel = CHANNELS_MAX; channel != PA_CHANNELS_MAX;
       ++channel) {
    channel_map.map[channel] = PA_CHANNEL_POSITION_MONO;
  }

  return channel_map;
}

static size_t MicrosecondsToBytes(
    uint32 microseconds, uint32 sample_rate, size_t bytes_per_frame) {
  return microseconds * sample_rate * bytes_per_frame /
      base::Time::kMicrosecondsPerSecond;
}

// static
void PulseAudioOutputStream::ContextStateCallback(pa_context* context,
                                                  void* state_addr) {
  pa_context_state_t* state = static_cast<pa_context_state_t*>(state_addr);
  *state = pa_context_get_state(context);
}

// static
void PulseAudioOutputStream::WriteRequestCallback(pa_stream* playback_handle,
                                                  size_t length,
                                                  void* stream_addr) {
  PulseAudioOutputStream* stream =
      reinterpret_cast<PulseAudioOutputStream*>(stream_addr);

  DCHECK(stream->manager_->GetMessageLoop()->BelongsToCurrentThread());

  stream->write_callback_handled_ = true;

  // Fulfill write request.
  stream->FulfillWriteRequest(length);
}

PulseAudioOutputStream::PulseAudioOutputStream(const AudioParameters& params,
                                               AudioManagerPulse* manager)
    : channel_layout_(params.channel_layout()),
      channel_count_(ChannelLayoutToChannelCount(channel_layout_)),
      sample_format_(BitsToPASampleFormat(params.bits_per_sample())),
      sample_rate_(params.sample_rate()),
      bytes_per_frame_(params.GetBytesPerFrame()),
      manager_(manager),
      pa_context_(NULL),
      pa_mainloop_(NULL),
      playback_handle_(NULL),
      packet_size_(params.GetBytesPerBuffer()),
      frames_per_packet_(packet_size_ / bytes_per_frame_),
      client_buffer_(NULL),
      volume_(1.0f),
      stream_stopped_(true),
      write_callback_handled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      source_callback_(NULL) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  // TODO(slock): Sanity check input values.
}

PulseAudioOutputStream::~PulseAudioOutputStream() {
  // All internal structures should already have been freed in Close(),
  // which calls AudioManagerPulse::Release which deletes this object.
  DCHECK(!playback_handle_);
  DCHECK(!pa_context_);
  DCHECK(!pa_mainloop_);
}

bool PulseAudioOutputStream::Open() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  // TODO(slock): Possibly move most of this to an OpenPlaybackDevice function
  // in a new class 'pulse_util', like alsa_util.

  // Create a mainloop API and connect to the default server.
  pa_mainloop_ = pa_mainloop_new();
  pa_mainloop_api* pa_mainloop_api = pa_mainloop_get_api(pa_mainloop_);
  pa_context_ = pa_context_new(pa_mainloop_api, "Chromium");
  pa_context_state_t pa_context_state = PA_CONTEXT_UNCONNECTED;
  pa_context_connect(pa_context_, NULL, PA_CONTEXT_NOFLAGS, NULL);

  // Wait until PulseAudio is ready.
  pa_context_set_state_callback(pa_context_, &ContextStateCallback,
                                &pa_context_state);
  while (pa_context_state != PA_CONTEXT_READY) {
    pa_mainloop_iterate(pa_mainloop_, 1, NULL);
    if (pa_context_state == PA_CONTEXT_FAILED ||
        pa_context_state == PA_CONTEXT_TERMINATED) {
      Reset();
      return false;
    }
  }

  // Set sample specifications.
  pa_sample_spec pa_sample_specifications;
  pa_sample_specifications.format = sample_format_;
  pa_sample_specifications.rate = sample_rate_;
  pa_sample_specifications.channels = channel_count_;

  // Get channel mapping and open playback stream.
  pa_channel_map* map = NULL;
  pa_channel_map source_channel_map = ChannelLayoutToPAChannelMap(
      channel_layout_);
  if (source_channel_map.channels != 0) {
    // The source data uses a supported channel map so we will use it rather
    // than the default channel map (NULL).
    map = &source_channel_map;
  }
  playback_handle_ = pa_stream_new(pa_context_, "Playback",
                                   &pa_sample_specifications, map);

  // Initialize client buffer.
  uint32 output_packet_size = frames_per_packet_ * bytes_per_frame_;
  client_buffer_.reset(new media::SeekableBuffer(0, output_packet_size));

  // Set write callback.
  pa_stream_set_write_callback(playback_handle_, &WriteRequestCallback, this);

  // Set server-side buffer attributes.
  // (uint32_t)-1 is the default and recommended value from PulseAudio's
  // documentation, found at:
  // http://freedesktop.org/software/pulseaudio/doxygen/structpa__buffer__attr.html.
  pa_buffer_attr pa_buffer_attributes;
  pa_buffer_attributes.maxlength = static_cast<uint32_t>(-1);
  pa_buffer_attributes.tlength = output_packet_size;
  pa_buffer_attributes.prebuf = static_cast<uint32_t>(-1);
  pa_buffer_attributes.minreq = static_cast<uint32_t>(-1);
  pa_buffer_attributes.fragsize = static_cast<uint32_t>(-1);

  // Connect playback stream.
  pa_stream_connect_playback(playback_handle_, NULL,
                             &pa_buffer_attributes,
                             (pa_stream_flags_t)
                             (PA_STREAM_INTERPOLATE_TIMING |
                             PA_STREAM_ADJUST_LATENCY |
                             PA_STREAM_AUTO_TIMING_UPDATE),
                             NULL, NULL);

  if (!playback_handle_) {
    Reset();
    return false;
  }

  return true;
}

void PulseAudioOutputStream::Reset() {
  stream_stopped_ = true;

  // Close the stream.
  if (playback_handle_) {
    pa_stream_flush(playback_handle_, NULL, NULL);
    pa_stream_disconnect(playback_handle_);

    // Release PulseAudio structures.
    pa_stream_unref(playback_handle_);
    playback_handle_ = NULL;
  }
  if (pa_context_) {
    pa_context_unref(pa_context_);
    pa_context_ = NULL;
  }
  if (pa_mainloop_) {
    pa_mainloop_free(pa_mainloop_);
    pa_mainloop_ = NULL;
  }

  // Release internal buffer.
  client_buffer_.reset();
}

void PulseAudioOutputStream::Close() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  Reset();

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

void PulseAudioOutputStream::WaitForWriteRequest() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  if (stream_stopped_)
    return;

  // Iterate the PulseAudio mainloop.  If PulseAudio doesn't request a write,
  // post a task to iterate the mainloop again.
  write_callback_handled_ = false;
  pa_mainloop_iterate(pa_mainloop_, 1, NULL);
  if (!write_callback_handled_) {
    manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
        &PulseAudioOutputStream::WaitForWriteRequest,
        weak_factory_.GetWeakPtr()));
  }
}

bool PulseAudioOutputStream::BufferPacketFromSource() {
  uint32 buffer_delay = client_buffer_->forward_bytes();
  pa_usec_t pa_latency_micros;
  int negative;
  pa_stream_get_latency(playback_handle_, &pa_latency_micros, &negative);
  uint32 hardware_delay = MicrosecondsToBytes(pa_latency_micros,
                                              sample_rate_,
                                              bytes_per_frame_);
  // TODO(slock): Deal with negative latency (negative == 1).  This has yet
  // to happen in practice though.
  scoped_refptr<media::DataBuffer> packet =
      new media::DataBuffer(packet_size_);
  int frames_filled = RunDataCallback(
      audio_bus_.get(), AudioBuffersState(buffer_delay, hardware_delay));
  size_t packet_size = frames_filled * bytes_per_frame_;

  DCHECK_LE(packet_size, packet_size_);
  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  audio_bus_->ToInterleaved(
      frames_filled, bytes_per_frame_ / channel_count_,
      packet->GetWritableData());

  if (packet_size == 0)
    return false;

  media::AdjustVolume(packet->GetWritableData(),
                      packet_size,
                      channel_count_,
                      bytes_per_frame_ / channel_count_,
                      volume_);
  packet->SetDataSize(packet_size);
  // Add the packet to the buffer.
  client_buffer_->Append(packet);
  return true;
}

void PulseAudioOutputStream::FulfillWriteRequest(size_t requested_bytes) {
  // If we have enough data to fulfill the request, we can finish the write.
  if (stream_stopped_)
    return;

  // Request more data from the source until we can fulfill the request or
  // fail to receive anymore data.
  bool buffering_successful = true;
  size_t forward_bytes = static_cast<size_t>(client_buffer_->forward_bytes());
  while (forward_bytes < requested_bytes && buffering_successful) {
    buffering_successful = BufferPacketFromSource();
  }

  size_t bytes_written = 0;
  if (client_buffer_->forward_bytes() > 0) {
    // Try to fulfill the request by writing as many of the requested bytes to
    // the stream as we can.
    WriteToStream(requested_bytes, &bytes_written);
  }

  if (bytes_written < requested_bytes) {
    // We weren't able to buffer enough data to fulfill the request.  Try to
    // fulfill the rest of the request later.
    manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
        &PulseAudioOutputStream::FulfillWriteRequest,
        weak_factory_.GetWeakPtr(),
        requested_bytes - bytes_written));
  } else {
    // Continue playback.
    manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
        &PulseAudioOutputStream::WaitForWriteRequest,
        weak_factory_.GetWeakPtr()));
  }
}

void PulseAudioOutputStream::WriteToStream(size_t bytes_to_write,
                                           size_t* bytes_written) {
  *bytes_written = 0;
  while (*bytes_written < bytes_to_write) {
    const uint8* chunk;
    int chunk_size;

    // Stop writing if there is no more data available.
    if (!client_buffer_->GetCurrentChunk(&chunk, &chunk_size))
      break;

    // Write data to stream.
    pa_stream_write(playback_handle_, chunk, chunk_size,
                    NULL, 0LL, PA_SEEK_RELATIVE);
    client_buffer_->Seek(chunk_size);
    *bytes_written += chunk_size;
  }
}

void PulseAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  CHECK(callback);
  DLOG_IF(ERROR, !playback_handle_)
      << "Open() has not been called successfully";
  if (!playback_handle_)
    return;

  source_callback_ = callback;

  // Clear buffer, it might still have data in it.
  client_buffer_->Clear();
  stream_stopped_ = false;

  // Start playback.
  manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &PulseAudioOutputStream::WaitForWriteRequest,
      weak_factory_.GetWeakPtr()));
}

void PulseAudioOutputStream::Stop() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  stream_stopped_ = true;
}

void PulseAudioOutputStream::SetVolume(double volume) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  volume_ = static_cast<float>(volume);
}

void PulseAudioOutputStream::GetVolume(double* volume) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  *volume = volume_;
}

int PulseAudioOutputStream::RunDataCallback(
    AudioBus* audio_bus, AudioBuffersState buffers_state) {
  if (source_callback_)
    return source_callback_->OnMoreData(audio_bus, buffers_state);

  return 0;
}

}  // namespace media
