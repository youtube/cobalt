// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_mixer.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_util.h"

namespace media {

AudioOutputMixer::AudioOutputMixer(AudioManager* audio_manager,
                                   const AudioParameters& params,
                                   const base::TimeDelta& close_delay)
    : AudioOutputDispatcher(audio_manager, params),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)),
      close_timer_(FROM_HERE,
                   close_delay,
                   weak_this_.GetWeakPtr(),
                   &AudioOutputMixer::ClosePhysicalStream) {
  // TODO(enal): align data.
  mixer_data_.reset(new uint8[params_.GetBytesPerBuffer()]);
}

AudioOutputMixer::~AudioOutputMixer() {
}

bool AudioOutputMixer::OpenStream() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (physical_stream_.get())
    return true;
  AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(params_);
  if (!stream)
    return false;
  if (!stream->Open()) {
    stream->Close();
    return false;
  }
  physical_stream_.reset(stream);
  close_timer_.Reset();
  return true;
}

bool AudioOutputMixer::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // May need to re-open the physical stream if no active proxies and
  // enough time had pass.
  OpenStream();
  if (!physical_stream_.get())
    return false;

  double volume = 0.0;
  stream_proxy->GetVolume(&volume);
  bool should_start = proxies_.empty();
  {
    base::AutoLock lock(lock_);
    ProxyData* proxy_data = &proxies_[stream_proxy];
    proxy_data->audio_source_callback = callback;
    proxy_data->volume = volume;
    proxy_data->pending_bytes = 0;
  }
  // We cannot start physical stream under the lock,
  // OnMoreData() would try acquiring it...
  if (should_start) {
    physical_stream_->SetVolume(1.0);
    physical_stream_->Start(this);
  }
  return true;
}

void AudioOutputMixer::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Because of possible deadlock we cannot stop physical stream under the lock
  // (physical_stream_->Stop() can call OnError(), and it acquires the lock to
  // iterate through proxies), so acquire the lock, update proxy list, release
  // the lock, and only then stop physical stream if necessary.
  bool stop_physical_stream = false;
  {
    base::AutoLock lock(lock_);
    ProxyMap::iterator it = proxies_.find(stream_proxy);
    if (it != proxies_.end()) {
      proxies_.erase(it);
      stop_physical_stream = proxies_.empty();
    }
  }
  if (physical_stream_.get()) {
    if (stop_physical_stream)
      physical_stream_->Stop();
    close_timer_.Reset();
  }
}

void AudioOutputMixer::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                       double volume) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  ProxyMap::iterator it = proxies_.find(stream_proxy);

  // Do nothing if stream is not currently playing.
  if (it != proxies_.end()) {
    base::AutoLock lock(lock_);
    it->second.volume = volume;
  }
}

void AudioOutputMixer::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  StopStream(stream_proxy);
}

void AudioOutputMixer::Shutdown() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Cancel any pending tasks to close physical stream.
  weak_this_.InvalidateWeakPtrs();

  while (!proxies_.empty()) {
    CloseStream(proxies_.begin()->first);
  }
  ClosePhysicalStream();

  // No AudioOutputProxy objects should hold a reference to us when we get
  // to this stage.
  DCHECK(HasOneRef()) << "Only the AudioManager should hold a reference";
}

void AudioOutputMixer::ClosePhysicalStream() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (proxies_.empty() && physical_stream_.get() != NULL)
    physical_stream_.release()->Close();
}

// AudioSourceCallback implementation.
uint32 AudioOutputMixer::OnMoreData(AudioOutputStream* stream,
                                    uint8* dest,
                                    uint32 max_size,
                                    AudioBuffersState buffers_state) {
  max_size = std::min(max_size,
                      static_cast<uint32>(params_.GetBytesPerBuffer()));
  // TODO(enal): consider getting rid of lock as it is in time-critical code.
  //             E.g. swap |proxies_| with local variable, and merge 2 lists
  //             at the end. That would speed things up but complicate stopping
  //             the stream.
  base::AutoLock lock(lock_);
  if (proxies_.empty())
    return 0;
  uint32 actual_total_size = 0;
  uint32 bytes_per_sample = params_.bits_per_sample() >> 3;

  // Go through all the streams, getting data for every one of them
  // and mixing it into destination.
  // Minor optimization: for the first stream we are writing data directly into
  // destination. This way we don't have to mix the data when there is only one
  // active stream, and net win in other cases, too.
  bool first_stream = true;
  uint8* actual_dest = dest;
  for (ProxyMap::iterator it = proxies_.begin(); it != proxies_.end(); ++it) {
    AudioOutputProxy* stream_proxy = it->first;
    ProxyData* proxy_data = &it->second;
    // TODO(enal): We don't know |pending _bytes| for individual stream, and we
    // should give that value to individual stream's OnMoreData(). I believe it
    // can be used there to evaluate exact position of data it should return.
    // Current code "sorta works" if everything works perfectly, but would have
    // problems if some of the buffers are only partially filled -- we don't
    // know how how much data was in the buffer OS returned to us, so we cannot
    // correctly calculate new value. If we know number of buffers we can solve
    // the problem by storing not one value but vector of them.
    int pending_bytes = std::min(proxy_data->pending_bytes,
                                 buffers_state.pending_bytes);
    // Note: there is no way we can deduce hardware_delay_bytes for the
    // particular proxy stream. Use zero instead.
    uint32 actual_size = proxy_data->audio_source_callback->OnMoreData(
        stream_proxy,
        actual_dest,
        max_size,
        AudioBuffersState(pending_bytes, 0));

    // Should update pending_bytes for each proxy.
    // If stream ended, pending_bytes goes down by max_size.
    if (actual_size == 0) {
      pending_bytes -= max_size;
      proxy_data->pending_bytes = std::max(pending_bytes, 0);
      continue;
    }

    // Otherwise, it goes up by amount of data. It cannot exceed max amount of
    // data we can buffer, but we don't know that value. So we increment
    // pending_bytes unconditionally but adjust it before actual use (which
    // would be on a next OnMoreData() call).
    proxy_data->pending_bytes = pending_bytes + actual_size;

    // No need to mix muted stream.
    double volume = proxy_data->volume;
    if (volume == 0.0)
      continue;

    // Different handling for first and all subsequent streams.
    if (first_stream) {
      if (volume != 1.0) {
        media::AdjustVolume(actual_dest,
                            actual_size,
                            params_.channels(),
                            bytes_per_sample,
                            volume);
      }
      if (actual_size < max_size)
        memset(dest + actual_size, 0, max_size - actual_size);
      first_stream = false;
      actual_dest = mixer_data_.get();
      actual_total_size = actual_size;
    } else {
      media::MixStreams(dest,
                        actual_dest,
                        actual_size,
                        bytes_per_sample,
                        volume);
      actual_total_size = std::max(actual_size, actual_total_size);
    }
  }
  return actual_total_size;
}

void AudioOutputMixer::OnError(AudioOutputStream* stream, int code) {
  base::AutoLock lock(lock_);
  for (ProxyMap::iterator it = proxies_.begin(); it != proxies_.end(); ++it) {
    it->second.audio_source_callback->OnError(it->first, code);
  }
}

void AudioOutputMixer::WaitTillDataReady() {
  base::AutoLock lock(lock_);
  for (ProxyMap::iterator it = proxies_.begin(); it != proxies_.end(); ++it) {
    it->second.audio_source_callback->WaitTillDataReady();
  }
}

}  // namespace media
