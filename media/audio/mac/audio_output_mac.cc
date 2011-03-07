// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_output_mac.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

// A custom data structure to store information an AudioQueue buffer.
struct AudioQueueUserData {
  AudioQueueUserData() : empty_buffer(false) {}
  bool empty_buffer;
};

// Overview of operation:
// 1) An object of PCMQueueOutAudioOutputStream is created by the AudioManager
// factory: audio_man->MakeAudioStream(). This just fills some structure.
// 2) Next some thread will call Open(), at that point the underliying OS
// queue is created and the audio buffers allocated.
// 3) Then some thread will call Start(source) At this point the source will be
// called to fill the initial buffers in the context of that same thread.
// Then the OS queue is started which will create its own thread which
// periodically will call the source for more data as buffers are being
// consumed.
// 4) At some point some thread will call Stop(), which we handle by directly
// stoping the OS queue.
// 5) One more callback to the source could be delivered in in the context of
// the queue's own thread. Data, if any will be discared.
// 6) The same thread that called stop will call Close() where we cleanup
// and notifiy the audio manager, which likley will destroy this object.

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
enum {
  kAudioQueueErr_EnqueueDuringReset = -66632
};
#endif

PCMQueueOutAudioOutputStream::PCMQueueOutAudioOutputStream(
    AudioManagerMac* manager, AudioParameters params)
    : format_(),
      audio_queue_(NULL),
      buffer_(),
      source_(NULL),
      manager_(manager),
      silence_bytes_(0),
      volume_(1),
      pending_bytes_(0) {
  // We must have a manager.
  DCHECK(manager_);
  // A frame is one sample across all channels. In interleaved audio the per
  // frame fields identify the set of n |channels|. In uncompressed audio, a
  // packet is always one frame.
  format_.mSampleRate = params.sample_rate;
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = params.bits_per_sample;
  format_.mChannelsPerFrame = params.channels;
  format_.mFramesPerPacket = 1;
  format_.mBytesPerPacket = (format_.mBitsPerChannel * params.channels) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;

  packet_size_ = params.GetPacketSize();

  // Silence buffer has a duration of 6ms to simulate the behavior of Windows.
  // This value is choosen by experiments and macs cannot keep up with
  // anything less than 6ms.
  silence_bytes_ = format_.mBytesPerFrame * params.sample_rate * 6 / 1000;
}

PCMQueueOutAudioOutputStream::~PCMQueueOutAudioOutputStream() {
}

void PCMQueueOutAudioOutputStream::HandleError(OSStatus err) {
  // source_ can be set to NULL from another thread. We need to cache its
  // pointer while we operate here. Note that does not mean that the source
  // has been destroyed.
  AudioSourceCallback* source = source_;
  if (source)
    source->OnError(this, static_cast<int>(err));
  NOTREACHED() << "error code " << err;
}

bool PCMQueueOutAudioOutputStream::Open() {
  // Create the actual queue object and let the OS use its own thread to
  // run its CFRunLoop.
  OSStatus err = AudioQueueNewOutput(&format_, RenderCallback, this, NULL,
                                     kCFRunLoopCommonModes, 0, &audio_queue_);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  // Allocate the hardware-managed buffers.
  for (uint32 ix = 0; ix != kNumBuffers; ++ix) {
    err = AudioQueueAllocateBuffer(audio_queue_, packet_size_, &buffer_[ix]);
    if (err != noErr) {
      HandleError(err);
      return false;
    }
    // Allocate memory for user data.
    buffer_[ix]->mUserData = new AudioQueueUserData();
  }
  // Set initial volume here.
  err = AudioQueueSetParameter(audio_queue_, kAudioQueueParam_Volume, 1.0);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  return true;
}

void PCMQueueOutAudioOutputStream::Close() {
  // It is valid to call Close() before calling Open(), thus audio_queue_
  // might be NULL.
  if (audio_queue_) {
    OSStatus err = 0;
    for (uint32 ix = 0; ix != kNumBuffers; ++ix) {
      if (buffer_[ix]) {
        // Free user data.
        delete static_cast<AudioQueueUserData*>(buffer_[ix]->mUserData);
        // Free AudioQueue buffer.
        err = AudioQueueFreeBuffer(audio_queue_, buffer_[ix]);
        if (err != noErr) {
          HandleError(err);
          break;
        }
      }
    }
    err = AudioQueueDispose(audio_queue_, true);
    if (err != noErr)
      HandleError(err);
  }
  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void PCMQueueOutAudioOutputStream::Stop() {
  // We request a synchronous stop, so the next call can take some time. In
  // the windows implementation we block here as well.
  source_ = NULL;
  // We set the source to null to signal to the data queueing thread it can stop
  // queueing data, however at most one callback might still be in flight which
  // could attempt to enqueue right after the next call. Rather that trying to
  // use a lock we rely on the internal Mac queue lock so the enqueue might
  // succeed or might fail but it won't crash or leave the queue itself in an
  // inconsistent state.
  OSStatus err = AudioQueueStop(audio_queue_, true);
  if (err != noErr)
    HandleError(err);
}

void PCMQueueOutAudioOutputStream::SetVolume(double volume) {
  if (!audio_queue_)
    return;
  volume_ = static_cast<float>(volume);
  OSStatus err = AudioQueueSetParameter(audio_queue_,
                                        kAudioQueueParam_Volume,
                                        volume);
  if (err != noErr) {
    HandleError(err);
  }
}

void PCMQueueOutAudioOutputStream::GetVolume(double* volume) {
  if (!audio_queue_)
    return;
  *volume = volume_;
}

// Reorder PCM from AAC layout to Core Audio layout.
// TODO(fbarchard): Switch layout when ffmpeg is updated.
template<class Format>
static void SwizzleLayout(Format* b, uint32 filled) {
  static const int kNumSurroundChannels = 6;
  Format aac[kNumSurroundChannels];
  for (uint32 i = 0; i < filled; i += sizeof(aac), b += kNumSurroundChannels) {
    memcpy(aac, b, sizeof(aac));
    b[0] = aac[1];  // L
    b[1] = aac[2];  // R
    b[2] = aac[0];  // C
    b[3] = aac[5];  // LFE
    b[4] = aac[3];  // Ls
    b[5] = aac[4];  // Rs
  }
}

// Note to future hackers of this function: Do not add locks here because we
// call out to third party source that might do crazy things including adquire
// external locks or somehow re-enter here because its legal for them to call
// some audio functions.
void PCMQueueOutAudioOutputStream::RenderCallback(void* p_this,
                                                  AudioQueueRef queue,
                                                  AudioQueueBufferRef buffer) {
  PCMQueueOutAudioOutputStream* audio_stream =
      static_cast<PCMQueueOutAudioOutputStream*>(p_this);
  // Call the audio source to fill the free buffer with data. Not having a
  // source means that the queue has been closed. This is not an error.
  AudioSourceCallback* source = audio_stream->source_;
  if (!source)
    return;

  // Adjust the number of pending bytes by subtracting the amount played.
  if (!static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer)
    audio_stream->pending_bytes_ -= buffer->mAudioDataByteSize;
  uint32 capacity = buffer->mAudioDataBytesCapacity;
  // TODO(sergeyu): Specify correct hardware delay for AudioBuffersState.
  uint32 filled = source->OnMoreData(
      audio_stream, reinterpret_cast<uint8*>(buffer->mAudioData), capacity,
      AudioBuffersState(audio_stream->pending_bytes_, 0));

  // In order to keep the callback running, we need to provide a positive amount
  // of data to the audio queue. To simulate the behavior of Windows, we write
  // a buffer of silence.
  if (!filled) {
    CHECK(audio_stream->silence_bytes_ <= static_cast<int>(capacity));
    filled = audio_stream->silence_bytes_;
    memset(buffer->mAudioData, 0, filled);
    static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer = true;
  } else if (filled > capacity) {
    // User probably overran our buffer.
    audio_stream->HandleError(0);
    return;
  } else {
    static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer = false;
  }

  // Handle channel order for 5.1 audio.
  if (audio_stream->format_.mChannelsPerFrame == 6) {
    if (audio_stream->format_.mBitsPerChannel == 8) {
      SwizzleLayout(reinterpret_cast<uint8*>(buffer->mAudioData), filled);
    } else if (audio_stream->format_.mBitsPerChannel == 16) {
      SwizzleLayout(reinterpret_cast<int16*>(buffer->mAudioData), filled);
    } else if (audio_stream->format_.mBitsPerChannel == 32) {
      SwizzleLayout(reinterpret_cast<int32*>(buffer->mAudioData), filled);
    }
  }

  buffer->mAudioDataByteSize = filled;

  // Incremnet bytes by amount filled into audio buffer if this is not a
  // silence buffer.
  if (!static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer)
    audio_stream->pending_bytes_ += filled;
  if (NULL == queue)
    return;
  // Queue the audio data to the audio driver.
  OSStatus err = AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
  if (err != noErr) {
    if (err == kAudioQueueErr_EnqueueDuringReset) {
      // This is the error you get if you try to enqueue a buffer and the
      // queue has been closed. Not really a problem if indeed the queue
      // has been closed.
      if (!audio_stream->source_)
        return;
    }
    audio_stream->HandleError(err);
  }
}

void PCMQueueOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  OSStatus err = noErr;
  source_ = callback;
  pending_bytes_ = 0;
  // Ask the source to pre-fill all our buffers before playing.
  for (uint32 ix = 0; ix != kNumBuffers; ++ix) {
    buffer_[ix]->mAudioDataByteSize = 0;
    RenderCallback(this, NULL, buffer_[ix]);
  }

  // Queue the buffers to the audio driver, sounds starts now.
  for (uint32 ix = 0; ix != kNumBuffers; ++ix) {
    err = AudioQueueEnqueueBuffer(audio_queue_, buffer_[ix], 0, NULL);
    if (err != noErr) {
      HandleError(err);
      return;
    }
  }
  err  = AudioQueueStart(audio_queue_, NULL);
  if (err != noErr) {
    HandleError(err);
    return;
  }
}
