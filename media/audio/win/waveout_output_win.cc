// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/waveout_output_win.h"

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_manager_win.h"

// Some general thoughts about the waveOut API which is badly documented :
// - We use CALLBACK_FUNCTION mode in which XP secretly creates two threads
//   named _MixerCallbackThread and _waveThread which have real-time priority.
//   The callbacks occur in _waveThread.
// - Windows does not provide a way to query if the device is playing or paused
//   thus it forces you to maintain state, which naturally is not exactly
//   synchronized to the actual device state.
// - Some functions, like waveOutReset cannot be called in the callback thread
//   or called in any random state because they deadlock. This results in a
//   non- instantaneous Stop() method. waveOutPrepareHeader seems to be in the
//   same boat.
// - waveOutReset() will forcefully kill the _waveThread so it is important
//   to make sure we are not executing inside the audio source's OnMoreData()
//   or that we take locks inside WaveCallback() or QueueNextPacket().

// Sixty four MB is the maximum buffer size per AudioOutputStream.
static const uint32 kMaxOpenBufferSize = 1024 * 1024 * 64;

// Our sound buffers are allocated once and kept in a linked list using the
// the WAVEHDR::dwUser variable. The last buffer points to the first buffer.
static WAVEHDR* GetNextBuffer(WAVEHDR* current) {
  return reinterpret_cast<WAVEHDR*>(current->dwUser);
}

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

PCMWaveOutAudioOutputStream::PCMWaveOutAudioOutputStream(
    AudioManagerWin* manager, AudioParameters params, int num_buffers,
    UINT device_id)
    : state_(PCMA_BRAND_NEW),
      manager_(manager),
      device_id_(device_id),
      waveout_(NULL),
      callback_(NULL),
      num_buffers_(num_buffers),
      buffer_(NULL),
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
  // The event is auto-reset.
  stopped_event_.Set(::CreateEventW(NULL, FALSE, FALSE, NULL));
}

PCMWaveOutAudioOutputStream::~PCMWaveOutAudioOutputStream() {
  DCHECK(NULL == waveout_);
}

bool PCMWaveOutAudioOutputStream::Open() {
  if (state_ != PCMA_BRAND_NEW)
    return false;
  if (num_buffers_ < 2 || num_buffers_ > 5)
    return false;
  // Open the device. We'll be getting callback in WaveCallback function.
  // They occur in a magic, time-critical thread that windows creates.
  MMRESULT result = ::waveOutOpen(&waveout_, device_id_,
                                  reinterpret_cast<LPCWAVEFORMATEX>(&format_),
                                  reinterpret_cast<DWORD_PTR>(WaveCallback),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
  if (result != MMSYSERR_NOERROR)
    return false;

  SetupBuffers();
  state_ = PCMA_READY;
  return true;
}

void PCMWaveOutAudioOutputStream::SetupBuffers() {
  WAVEHDR* last = NULL;
  WAVEHDR* first = NULL;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    uint32 sz = sizeof(WAVEHDR) + buffer_size_;
    buffer_ =  reinterpret_cast<WAVEHDR*>(new char[sz]);
    buffer_->lpData = reinterpret_cast<char*>(buffer_) + sizeof(WAVEHDR);
    buffer_->dwBufferLength = buffer_size_;
    buffer_->dwBytesRecorded = 0;
    buffer_->dwUser = reinterpret_cast<DWORD_PTR>(last);
    buffer_->dwFlags = WHDR_DONE;
    buffer_->dwLoops = 0;
    if (ix == 0)
      first = buffer_;
    last = buffer_;
    // Tell windows sound drivers about our buffers. Not documented what
    // this does but we can guess that causes the OS to keep a reference to
    // the memory pages so the driver can use them without worries.
    ::waveOutPrepareHeader(waveout_, buffer_, sizeof(WAVEHDR));
  }
  // Fix the first buffer to point to the last one.
  first->dwUser = reinterpret_cast<DWORD_PTR>(last);
}

void PCMWaveOutAudioOutputStream::FreeBuffers() {
  WAVEHDR* current = buffer_;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    WAVEHDR* next = GetNextBuffer(current);
    ::waveOutUnprepareHeader(waveout_, current, sizeof(WAVEHDR));
    delete[] reinterpret_cast<char*>(current);
    current = next;
  }
  buffer_ = NULL;
}

// Initially we ask the source to fill up both audio buffers. If we don't do
// this then we would always get the driver callback when it is about to run
// samples and that would leave too little time to react.
void PCMWaveOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  if (state_ != PCMA_READY)
    return;
  callback_ = callback;
  state_ = PCMA_PLAYING;
  pending_bytes_ = 0;
  WAVEHDR* buffer = buffer_;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    QueueNextPacket(buffer);  // Read more data.
    pending_bytes_ += buffer->dwBufferLength;
    buffer = GetNextBuffer(buffer);
  }
  buffer = buffer_;
  MMRESULT result = ::waveOutPause(waveout_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    return;
  }
  // Send the buffers to the audio driver. Note that the device is paused
  // so we avoid entering the callback method while still here.
  for (int ix = 0; ix != num_buffers_; ++ix) {
    result = ::waveOutWrite(waveout_, buffer, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
      HandleError(result);
      break;
    }
    buffer = GetNextBuffer(buffer);
  }
  result = ::waveOutRestart(waveout_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    return;
  }
}

// Stopping is tricky. First, no buffer should be locked by the audio driver
// or else the waveOutReset() will deadlock and secondly, the callback should
// not be inside the AudioSource's OnMoreData because waveOutReset() forcefully
// kills the callback thread.
void PCMWaveOutAudioOutputStream::Stop() {
  if (state_ != PCMA_PLAYING)
    return;
  state_ = PCMA_STOPPING;
  // Wait for the callback to finish, it will signal us when ready to be reset.
  if (WAIT_OBJECT_0 != ::WaitForSingleObject(stopped_event_, INFINITE)) {
    HandleError(::GetLastError());
    return;
  }
  state_ = PCMA_STOPPED;
  MMRESULT res = ::waveOutReset(waveout_);
  if (res != MMSYSERR_NOERROR) {
    state_ = PCMA_PLAYING;
    HandleError(res);
    return;
  }

  // Don't use callback after Stop().
  callback_ = NULL;

  state_ = PCMA_READY;
}

// We can Close in any state except that trying to close a stream that is
// playing Windows generates an error, which we propagate to the source.
void PCMWaveOutAudioOutputStream::Close() {
  if (waveout_) {
    // waveOutClose generates a callback with WOM_CLOSE id in the same thread.
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

// Windows call us back in this function when some events happen. Most notably
// when it is done playing a buffer. Since we use double buffering it is
// convenient to think of |buffer| as free and GetNextBuffer(buffer) as in
// use by the driver.
void PCMWaveOutAudioOutputStream::WaveCallback(HWAVEOUT hwo, UINT msg,
                                               DWORD_PTR instance,
                                               DWORD_PTR param1, DWORD_PTR) {
  PCMWaveOutAudioOutputStream* obj =
      reinterpret_cast<PCMWaveOutAudioOutputStream*>(instance);

  if (msg == WOM_DONE) {
    // WOM_DONE indicates that the driver is done with our buffer, we can
    // either ask the source for more data or check if we need to stop playing.
    WAVEHDR* buffer = reinterpret_cast<WAVEHDR*>(param1);
    buffer->dwFlags = WHDR_DONE;

    if (obj->state_ == PCMA_STOPPING) {
      // The main thread has called Stop() and is waiting to issue waveOutReset
      // which will kill this thread. We should not enter AudioSourceCallback
      // code anymore.
      ::SetEvent(obj->stopped_event_);
      return;
    } else if (obj->state_ == PCMA_STOPPED) {
      // Not sure if ever hit this but just in case.
      return;
    }

    // Before we queue the next packet, we need to adjust the number of pending
    // bytes since the last write to hardware.
    obj->pending_bytes_ -= buffer->dwBufferLength;

    obj->QueueNextPacket(buffer);

    // Time to send the buffer to the audio driver. Since we are reusing
    // the same buffers we can get away without calling waveOutPrepareHeader.
    MMRESULT result = ::waveOutWrite(hwo, buffer, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR)
      obj->HandleError(result);

    obj->pending_bytes_ += buffer->dwBufferLength;

  }
}
