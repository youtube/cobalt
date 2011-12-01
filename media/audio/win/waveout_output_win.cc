// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/waveout_output_win.h"

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_manager_win.h"

// Some general thoughts about the waveOut API which is badly documented :
// - We use CALLBACK_EVENT mode in which XP signals events such as buffer
//   releases.
// - We use RegisterWaitForSingleObject() so one of threads in thread pool
//   automatically calls our callback that feeds more data to Windows.
// - Windows does not provide a way to query if the device is playing or paused
//   thus it forces you to maintain state, which naturally is not exactly
//   synchronized to the actual device state.

// Sixty four MB is the maximum buffer size per AudioOutputStream.
static const uint32 kMaxOpenBufferSize = 1024 * 1024 * 64;

// See Also
// http://www.thx.com/consumer/home-entertainment/home-theater/surround-sound-speaker-set-up/
// http://en.wikipedia.org/wiki/Surround_sound

static const int kMaxChannelsToMask = 8;
static const unsigned int kChannelsToMask[kMaxChannelsToMask + 1] = {
  0,
  // 1 = Mono
  SPEAKER_FRONT_CENTER,
  // 2 = Stereo
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT,
  // 3 = Stereo + Center
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER,
  // 4 = Quad
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
  // 5 = 5.0
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
  // 6 = 5.1
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
  // 7 = 6.1
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
  SPEAKER_BACK_CENTER,
  // 8 = 7.1
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
  SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT
  // TODO(fbarchard): Add additional masks for 7.2 and beyond.
};

inline size_t PCMWaveOutAudioOutputStream::BufferSize() const {
  // Round size of buffer up to the nearest 16 bytes.
  return (sizeof(WAVEHDR) + buffer_size_ + 15u) & static_cast<size_t>(~15);
}

inline WAVEHDR* PCMWaveOutAudioOutputStream::GetBuffer(int n) const {
  DCHECK_GE(n, 0);
  DCHECK_LT(n, num_buffers_);
  return reinterpret_cast<WAVEHDR*>(&buffers_[n * BufferSize()]);
}


PCMWaveOutAudioOutputStream::PCMWaveOutAudioOutputStream(
    AudioManagerWin* manager, const AudioParameters& params, int num_buffers,
    UINT device_id)
    : state_(PCMA_BRAND_NEW),
      manager_(manager),
      device_id_(device_id),
      waveout_(NULL),
      callback_(NULL),
      num_buffers_(num_buffers),
      buffer_size_(params.GetPacketSize()),
      volume_(1),
      channels_(params.channels),
      pending_bytes_(0) {
  format_.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format_.Format.nChannels = params.channels;
  format_.Format.nSamplesPerSec = params.sample_rate;
  format_.Format.wBitsPerSample = params.bits_per_sample;
  format_.Format.cbSize = sizeof(format_) - sizeof(WAVEFORMATEX);
  // The next are computed from above.
  format_.Format.nBlockAlign = (format_.Format.nChannels *
                                format_.Format.wBitsPerSample) / 8;
  format_.Format.nAvgBytesPerSec = format_.Format.nBlockAlign *
                                   format_.Format.nSamplesPerSec;
  if (params.channels > kMaxChannelsToMask) {
    format_.dwChannelMask = kChannelsToMask[kMaxChannelsToMask];
  } else {
    format_.dwChannelMask = kChannelsToMask[params.channels];
  }
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  format_.Samples.wValidBitsPerSample = params.bits_per_sample;
}

PCMWaveOutAudioOutputStream::~PCMWaveOutAudioOutputStream() {
  DCHECK(NULL == waveout_);
}

bool PCMWaveOutAudioOutputStream::Open() {
  if (state_ != PCMA_BRAND_NEW)
    return false;
  if (BufferSize() * num_buffers_ > kMaxOpenBufferSize)
    return false;
  if (num_buffers_ < 2 || num_buffers_ > 5)
    return false;

  // Create buffer event.
  buffer_event_.Set(::CreateEvent(NULL,    // Security attributes.
                                  FALSE,   // It will auto-reset.
                                  FALSE,   // Initial state.
                                  NULL));  // No name.
  if (!buffer_event_.Get())
    return false;

  // Open the device.
  // We'll be getting buffer_event_ events when it's time to refill the buffer.
  MMRESULT result = ::waveOutOpen(
      &waveout_,
      device_id_,
      reinterpret_cast<LPCWAVEFORMATEX>(&format_),
      reinterpret_cast<DWORD_PTR>(buffer_event_.Get()),
      NULL,
      CALLBACK_EVENT);
  if (result != MMSYSERR_NOERROR)
    return false;

  SetupBuffers();
  state_ = PCMA_READY;
  return true;
}

void PCMWaveOutAudioOutputStream::SetupBuffers() {
  buffers_.reset(new char[BufferSize() * num_buffers_]);
  for (int ix = 0; ix != num_buffers_; ++ix) {
    WAVEHDR* buffer = GetBuffer(ix);
    buffer->lpData = reinterpret_cast<char*>(buffer) + sizeof(WAVEHDR);
    buffer->dwBufferLength = buffer_size_;
    buffer->dwBytesRecorded = 0;
    buffer->dwFlags = WHDR_DONE;
    buffer->dwLoops = 0;
    // Tell windows sound drivers about our buffers. Not documented what
    // this does but we can guess that causes the OS to keep a reference to
    // the memory pages so the driver can use them without worries.
    ::waveOutPrepareHeader(waveout_, buffer, sizeof(WAVEHDR));
  }
}

void PCMWaveOutAudioOutputStream::FreeBuffers() {
  for (int ix = 0; ix != num_buffers_; ++ix) {
    ::waveOutUnprepareHeader(waveout_, GetBuffer(ix), sizeof(WAVEHDR));
  }
  buffers_.reset(NULL);
}

// Initially we ask the source to fill up all audio buffers. If we don't do
// this then we would always get the driver callback when it is about to run
// samples and that would leave too little time to react.
void PCMWaveOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  if (state_ != PCMA_READY)
    return;
  callback_ = callback;

  // Start watching for buffer events.
  {
    HANDLE waiting_handle = NULL;
    ::RegisterWaitForSingleObject(&waiting_handle,
                                  buffer_event_.Get(),
                                  &BufferCallback,
                                  this,
                                  INFINITE,
                                  WT_EXECUTEDEFAULT);
    if (!waiting_handle) {
      HandleError(MMSYSERR_ERROR);
      return;
    }
    waiting_handle_.Set(waiting_handle);
  }

  state_ = PCMA_PLAYING;

  // Queue the buffers.
  pending_bytes_ = 0;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    WAVEHDR* buffer = GetBuffer(ix);
    // Caller waits for 1st packet to become available, but not for others,
    // so we wait for them here.
    if (ix != 0)
      callback_->WaitTillDataReady();
    QueueNextPacket(buffer);  // Read more data.
    pending_bytes_ += buffer->dwBufferLength;
  }

  // From now on |pending_bytes_| would be accessed by callback thread.
  // Most likely waveOutPause() or waveOutRestart() has its own memory barrier,
  // but issuing our own is safer.
  MemoryBarrier();

  MMRESULT result = ::waveOutPause(waveout_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    return;
  }

  // Send the buffers to the audio driver. Note that the device is paused
  // so we avoid entering the callback method while still here.
  for (int ix = 0; ix != num_buffers_; ++ix) {
    result = ::waveOutWrite(waveout_, GetBuffer(ix), sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
      HandleError(result);
      break;
    }
  }
  result = ::waveOutRestart(waveout_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    return;
  }
}

// Stopping is tricky if we want it be fast.
// For now just do it synchronously and avoid all the complexities.
// TODO(enal): if we want faster Stop() we can create singleton that keeps track
//             of all currently playing streams. Then you don't have to wait
//             till all callbacks are completed. Of course access to singleton
//             should be under its own lock, and checking the liveness and
//             acquiring the lock on stream should be done atomically.
void PCMWaveOutAudioOutputStream::Stop() {
  if (state_ != PCMA_PLAYING)
    return;
  state_ = PCMA_STOPPING;
  MemoryBarrier();

  // Stop playback.
  MMRESULT res = ::waveOutReset(waveout_);
  if (res != MMSYSERR_NOERROR) {
    state_ = PCMA_PLAYING;
    HandleError(res);
    return;
  }

  // Stop watching for buffer event, wait till all the callbacks are complete.
  BOOL unregister  = UnregisterWaitEx(waiting_handle_.Take(),
                                      INVALID_HANDLE_VALUE);
  if (!unregister) {
    state_ = PCMA_PLAYING;
    HandleError(MMSYSERR_ERROR);
    return;
  }

  // waveOutReset() leaves buffers in the unpredictable state, causing
  // problems if we want to release or reuse them. Fix the states.
  for (int ix = 0; ix != num_buffers_; ++ix) {
    GetBuffer(ix)->dwFlags = WHDR_PREPARED;
  }

  // Don't use callback after Stop().
  callback_ = NULL;

  state_ = PCMA_READY;
}

// We can Close in any state except that trying to close a stream that is
// playing Windows generates an error, which we propagate to the source.
void PCMWaveOutAudioOutputStream::Close() {
  Stop();  // Just to be sure. No-op if not playing.
  if (waveout_) {
    MMRESULT res = ::waveOutClose(waveout_);
    if (res != MMSYSERR_NOERROR) {
      HandleError(res);
      return;
    }
    state_ = PCMA_CLOSED;
    waveout_ = NULL;
    FreeBuffers();
  }
  // Tell the audio manager that we have been released. This can result in
  // the manager destroying us in-place so this needs to be the last thing
  // we do on this function.
  manager_->ReleaseOutputStream(this);
}

void PCMWaveOutAudioOutputStream::SetVolume(double volume) {
  if (!waveout_)
    return;
  volume_ = static_cast<float>(volume);
}

void PCMWaveOutAudioOutputStream::GetVolume(double* volume) {
  if (!waveout_)
    return;
  *volume = volume_;
}

void PCMWaveOutAudioOutputStream::HandleError(MMRESULT error) {
  DLOG(WARNING) << "PCMWaveOutAudio error " << error;
  callback_->OnError(this, error);
}

void PCMWaveOutAudioOutputStream::QueueNextPacket(WAVEHDR *buffer) {
  // Call the source which will fill our buffer with pleasant sounds and
  // return to us how many bytes were used.
  // If we are down sampling to a smaller number of channels, we need to
  // scale up the amount of pending bytes.
  // TODO(fbarchard): Handle used 0 by queueing more.
  uint32 scaled_pending_bytes = pending_bytes_ * channels_ /
                                format_.Format.nChannels;
  // TODO(sergeyu): Specify correct hardware delay for AudioBuffersState.
  uint32 used = callback_->OnMoreData(
      this, reinterpret_cast<uint8*>(buffer->lpData), buffer_size_,
      AudioBuffersState(scaled_pending_bytes, 0));
  if (used <= buffer_size_) {
    buffer->dwBufferLength = used * format_.Format.nChannels / channels_;
    if (channels_ > 2 && format_.Format.nChannels == 2) {
      media::FoldChannels(buffer->lpData, used,
                          channels_, format_.Format.wBitsPerSample >> 3,
                          volume_);
    } else {
      media::AdjustVolume(buffer->lpData, used,
                          format_.Format.nChannels,
                          format_.Format.wBitsPerSample >> 3,
                          volume_);
    }
  } else {
    HandleError(0);
    return;
  }
  buffer->dwFlags = WHDR_PREPARED;
}

// One of the threads in our thread pool asynchronously calls this function when
// buffer_event_ is signalled. Search through all the buffers looking for freed
// ones, fills them with data, and "feed" the Windows.
// Note: by searching through all the buffers we guarantee that we fill all the
//       buffers, even when "event loss" happens, i.e. if Windows signals event
//       when it did not flip into unsignaled state from the previous signal.
void NTAPI PCMWaveOutAudioOutputStream::BufferCallback(PVOID lpParameter,
                                                       BOOLEAN timer_fired) {
  TRACE_EVENT0("audio", "PCMWaveOutAudioOutputStream::BufferCallback");

  DCHECK(!timer_fired);
  PCMWaveOutAudioOutputStream* stream =
      reinterpret_cast<PCMWaveOutAudioOutputStream*>(lpParameter);

  // Lock the stream so callbacks do not interfere with each other.
  // Several callbacks can be called simultaneously by different threads in the
  // thread pool if some of the callbacks are slow, or system is very busy and
  // scheduled callbacks are not called on time.
  base::AutoLock auto_lock(stream->lock_);
  if (stream->state_ != PCMA_PLAYING)
    return;

  for (int ix = 0; ix != stream->num_buffers_; ++ix) {
    WAVEHDR* buffer = stream->GetBuffer(ix);
    if (buffer->dwFlags & WHDR_DONE) {
      // Before we queue the next packet, we need to adjust the number of
      // pending bytes since the last write to hardware.
      stream->pending_bytes_ -= buffer->dwBufferLength;
      stream->QueueNextPacket(buffer);

      // QueueNextPacket() can take a long time, especially if several of them
      // were called back-to-back. Check if we are stopping now.
      if (stream->state_ != PCMA_PLAYING)
        return;

      // Time to send the buffer to the audio driver. Since we are reusing
      // the same buffers we can get away without calling waveOutPrepareHeader.
      MMRESULT result = ::waveOutWrite(stream->waveout_,
                                       buffer,
                                       sizeof(WAVEHDR));
      if (result != MMSYSERR_NOERROR)
        stream->HandleError(result);
      stream->pending_bytes_ += buffer->dwBufferLength;
    }
  }
}
