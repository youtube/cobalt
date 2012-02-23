// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/wavein_input_win.h"

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "base/logging.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/device_enumeration_win.h"

namespace {
const int kStopInputStreamCallbackTimeout = 3000; // Three seconds.
}

using media::AudioDeviceNames;

// Our sound buffers are allocated once and kept in a linked list using the
// the WAVEHDR::dwUser variable. The last buffer points to the first buffer.
static WAVEHDR* GetNextBuffer(WAVEHDR* current) {
  return reinterpret_cast<WAVEHDR*>(current->dwUser);
}

PCMWaveInAudioInputStream::PCMWaveInAudioInputStream(
    AudioManagerWin* manager, const AudioParameters& params, int num_buffers,
    const std::string& device_id)
    : state_(kStateEmpty),
      manager_(manager),
      device_id_(device_id),
      wavein_(NULL),
      callback_(NULL),
      num_buffers_(num_buffers),
      buffer_(NULL),
      channels_(params.channels) {
  format_.wFormatTag = WAVE_FORMAT_PCM;
  format_.nChannels = params.channels > 2 ? 2 : params.channels;
  format_.nSamplesPerSec = params.sample_rate;
  format_.wBitsPerSample = params.bits_per_sample;
  format_.cbSize = 0;
  format_.nBlockAlign = (format_.nChannels * format_.wBitsPerSample) / 8;
  format_.nAvgBytesPerSec = format_.nBlockAlign * format_.nSamplesPerSec;
  buffer_size_ = params.samples_per_packet * format_.nBlockAlign;
  // If we don't have a packet size we use 100ms.
  if (!buffer_size_)
    buffer_size_ = format_.nAvgBytesPerSec / 10;
  // The event is auto-reset.
  stopped_event_.Set(::CreateEventW(NULL, FALSE, FALSE, NULL));
}

PCMWaveInAudioInputStream::~PCMWaveInAudioInputStream() {
  DCHECK(NULL == wavein_);
}

bool PCMWaveInAudioInputStream::Open() {
  if (state_ != kStateEmpty)
    return false;
  if (num_buffers_ < 2 || num_buffers_ > 10)
    return false;

  // Convert the stored device id string into an unsigned integer
  // corresponding to the selected device.
  UINT device_id = WAVE_MAPPER;
  if (!GetDeviceId(&device_id)) {
    return false;
  }

  // Open the specified input device for recording.
  MMRESULT result = MMSYSERR_NOERROR;
  result = ::waveInOpen(&wavein_, device_id, &format_,
                        reinterpret_cast<DWORD_PTR>(WaveCallback),
                        reinterpret_cast<DWORD_PTR>(this),
                        CALLBACK_FUNCTION);
  if (result != MMSYSERR_NOERROR)
    return false;

  SetupBuffers();
  state_ = kStateReady;
  return true;
}

void PCMWaveInAudioInputStream::SetupBuffers() {
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
    ::waveInPrepareHeader(wavein_, buffer_, sizeof(WAVEHDR));
  }
  // Fix the first buffer to point to the last one.
  first->dwUser = reinterpret_cast<DWORD_PTR>(last);
}

void PCMWaveInAudioInputStream::FreeBuffers() {
  WAVEHDR* current = buffer_;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    WAVEHDR* next = GetNextBuffer(current);
    if (current->dwFlags & WHDR_PREPARED)
      ::waveInUnprepareHeader(wavein_, current, sizeof(WAVEHDR));
    delete[] reinterpret_cast<char*>(current);
    current = next;
  }
  buffer_ = NULL;
}

void PCMWaveInAudioInputStream::Start(AudioInputCallback* callback) {
  if (state_ != kStateReady)
    return;

  callback_ = callback;
  state_ = kStateRecording;

  WAVEHDR* buffer = buffer_;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    QueueNextPacket(buffer);
    buffer = GetNextBuffer(buffer);
  }
  buffer = buffer_;

  MMRESULT result = ::waveInStart(wavein_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    state_ = kStateReady;
  } else {
    manager_->IncreaseActiveInputStreamCount();
  }
}

// Stopping is tricky. First, no buffer should be locked by the audio driver
// or else the waveInReset() will deadlock and secondly, the callback should
// not be inside the AudioInputCallback's OnData because waveInReset()
// forcefully kills the callback thread.
void PCMWaveInAudioInputStream::Stop() {
  if (state_ != kStateRecording)
    return;
  state_ = kStateStopping;
  // Wait for the callback to finish, it will signal us when ready to be reset.
  if (WAIT_OBJECT_0 !=
      ::WaitForSingleObject(stopped_event_, kStopInputStreamCallbackTimeout)) {
    HandleError(::GetLastError());
    return;
  }
  // Stop is always called before Close. In case of error, this will be
  // also called when closing the input controller.
  manager_->DecreaseActiveInputStreamCount();

  state_ = kStateStopped;
  MMRESULT res = ::waveInReset(wavein_);
  if (res != MMSYSERR_NOERROR) {
    state_ = kStateRecording;
    HandleError(res);
    return;
  }
  state_ = kStateReady;
}

// We can Close in any state except that when trying to close a stream that is
// recording Windows generates an error, which we propagate to the source.
void PCMWaveInAudioInputStream::Close() {
  if (wavein_) {
    // waveInClose generates a callback with WIM_CLOSE id in the same thread.
    MMRESULT res = ::waveInClose(wavein_);
    if (res != MMSYSERR_NOERROR) {
      HandleError(res);
      return;
    }
    state_ = kStateClosed;
    wavein_ = NULL;
    FreeBuffers();
  }
  // Tell the audio manager that we have been released. This can result in
  // the manager destroying us in-place so this needs to be the last thing
  // we do on this function.
  manager_->ReleaseInputStream(this);
}

void PCMWaveInAudioInputStream::HandleError(MMRESULT error) {
  DLOG(WARNING) << "PCMWaveInAudio error " << error;
  callback_->OnError(this, error);
}

void PCMWaveInAudioInputStream::QueueNextPacket(WAVEHDR *buffer) {
  MMRESULT res = ::waveInAddBuffer(wavein_, buffer, sizeof(WAVEHDR));
  if (res != MMSYSERR_NOERROR)
    HandleError(res);
}

bool PCMWaveInAudioInputStream::GetDeviceId(UINT* device_index) {
  // Deliver the default input device id (WAVE_MAPPER) if the default
  // device has been selected.
  if (device_id_ == AudioManagerBase::kDefaultDeviceId) {
    *device_index = WAVE_MAPPER;
    return true;
  }

  // Get list of all available and active devices.
  AudioDeviceNames device_names;
  if (!GetInputDeviceNamesWinXP(&device_names))
    return false;

  if (device_names.empty())
    return false;

  // Search the full list of devices and compare with the specified
  // device id which was specified in the constructor. Stop comparing
  // when a match is found and return the corresponding index.
  UINT index = 0;
  bool found_device = false;
  AudioDeviceNames::const_iterator it = device_names.begin();
  while (it != device_names.end()) {
    if (it->unique_id.compare(device_id_) == 0) {
      *device_index = index;
      found_device = true;
      break;
    }
    ++index;
    ++it;
  }

  return found_device;
}

// Windows calls us back in this function when some events happen. Most notably
// when it has an audio buffer with recorded data.
void PCMWaveInAudioInputStream::WaveCallback(HWAVEIN hwi, UINT msg,
                                             DWORD_PTR instance,
                                             DWORD_PTR param1, DWORD_PTR) {
  PCMWaveInAudioInputStream* obj =
      reinterpret_cast<PCMWaveInAudioInputStream*>(instance);

  if (msg == WIM_DATA) {
    // WIM_DONE indicates that the driver is done with our buffer. We pass it
    // to the callback and check if we need to stop playing.
    // It should be OK to assume the data in the buffer is what has been
    // recorded in the soundcard.
    WAVEHDR* buffer = reinterpret_cast<WAVEHDR*>(param1);
    obj->callback_->OnData(obj, reinterpret_cast<const uint8*>(buffer->lpData),
                           buffer->dwBytesRecorded,
                           buffer->dwBytesRecorded);

    if (obj->state_ == kStateStopping) {
      // The main thread has called Stop() and is waiting to issue waveOutReset
      // which will kill this thread. We should not enter AudioSourceCallback
      // code anymore.
      ::SetEvent(obj->stopped_event_);
    } else if (obj->state_ == kStateStopped) {
      // Not sure if ever hit this but just in case.
    } else {
      // Queue the finished buffer back with the audio driver. Since we are
      // reusing the same buffers we can get away without calling
      // waveInPrepareHeader.
      obj->QueueNextPacket(buffer);
    }
  } else if (msg == WIM_CLOSE) {
    // We can be closed before calling Start, so it is possible to have a
    // null callback at this point.
    if (obj->callback_)
      obj->callback_->OnClose(obj);
  }
}
