// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_input_mac.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
enum {
  kAudioQueueErr_EnqueueDuringReset = -66632
};
#endif

PCMQueueInAudioInputStream::PCMQueueInAudioInputStream(
    AudioManagerMac* manager,
    AudioParameters params,
    uint32 samples_per_buffer)
    : manager_(manager),
      callback_(NULL),
      audio_queue_(NULL),
      buffer_size_bytes_(0) {
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
  format_.mBytesPerPacket = (params.bits_per_sample * params.channels) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;

  buffer_size_bytes_ = format_.mBytesPerFrame * samples_per_buffer;
}

PCMQueueInAudioInputStream::~PCMQueueInAudioInputStream() {
  DCHECK(!callback_);
  DCHECK(!audio_queue_);
}

bool PCMQueueInAudioInputStream::Open() {
  OSStatus err = AudioQueueNewInput(&format_,
                                    &HandleInputBufferStatic,
                                    this,
                                    NULL,  // Use OS CFRunLoop for |callback|
                                    kCFRunLoopCommonModes,
                                    0,  // Reserved
                                    &audio_queue_);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  return SetupBuffers();
}

void PCMQueueInAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  DCHECK(audio_queue_) << "Must call Open() first";
  if (callback_)
    return;
  callback_ = callback;
  OSStatus err = AudioQueueStart(audio_queue_, NULL);
  if (err != noErr)
    HandleError(err);
}

void PCMQueueInAudioInputStream::Stop() {
  if (!audio_queue_)
    return;
  // We request a synchronous stop, so the next call can take some time. In
  // the windows implementation we block here as well.
  OSStatus err = AudioQueueStop(audio_queue_, true);
  if (err != noErr)
    HandleError(err);
}

void PCMQueueInAudioInputStream::Close() {
  // It is valid to call Close() before calling Open() or Start(), thus
  // |audio_queue_| and |callback_| might be NULL.
  if (audio_queue_) {
    OSStatus err = AudioQueueDispose(audio_queue_, true);
    audio_queue_ = NULL;
    if (err != noErr)
      HandleError(err);
  }
  if (callback_) {
    callback_->OnClose(this);
    callback_ = NULL;
  }
  manager_->ReleaseInputStream(this);
  // CARE: This object may now be destroyed.
}

void PCMQueueInAudioInputStream::HandleError(OSStatus err) {
  if (callback_)
    callback_->OnError(this, static_cast<int>(err));
  NOTREACHED() << "error code " << err;
}

bool PCMQueueInAudioInputStream::SetupBuffers() {
  DCHECK(buffer_size_bytes_);
  for (int i = 0; i < kNumberBuffers; ++i) {
    AudioQueueBufferRef buffer;
    OSStatus err = AudioQueueAllocateBuffer(audio_queue_,
                                            buffer_size_bytes_,
                                            &buffer);
    if (err == noErr)
      err = QueueNextBuffer(buffer);
    if (err != noErr) {
      HandleError(err);
      return false;
    }
    // |buffer| will automatically be freed when |audio_queue_| is released.
  }
  return true;
}

OSStatus PCMQueueInAudioInputStream::QueueNextBuffer(
    AudioQueueBufferRef audio_buffer) {
  // Only the first 2 params are needed for recording.
  return AudioQueueEnqueueBuffer(audio_queue_, audio_buffer, 0, NULL);
}

// static
void PCMQueueInAudioInputStream::HandleInputBufferStatic(
    void* data,
    AudioQueueRef audio_queue,
    AudioQueueBufferRef audio_buffer,
    const AudioTimeStamp* start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription* desc) {
  reinterpret_cast<PCMQueueInAudioInputStream*>(data)->
      HandleInputBuffer(audio_queue, audio_buffer, start_time,
                        num_packets, desc);
}

void PCMQueueInAudioInputStream::HandleInputBuffer(
    AudioQueueRef audio_queue,
    AudioQueueBufferRef audio_buffer,
    const AudioTimeStamp* start_time,
    UInt32 num_packets,
    const AudioStreamPacketDescription* packet_desc) {
  DCHECK_EQ(audio_queue_, audio_queue);
  DCHECK(audio_buffer->mAudioData);
  if (!callback_) {
    // This can happen if Stop() was called without start.
    DCHECK_EQ(0U, audio_buffer->mAudioDataByteSize);
    return;
  }

  if (audio_buffer->mAudioDataByteSize)
    callback_->OnData(this,
                      reinterpret_cast<const uint8*>(audio_buffer->mAudioData),
                      audio_buffer->mAudioDataByteSize);
  // Recycle the buffer.
  OSStatus err = QueueNextBuffer(audio_buffer);
  if (err != noErr) {
    if (err == kAudioQueueErr_EnqueueDuringReset) {
      // This is the error you get if you try to enqueue a buffer and the
      // queue has been closed. Not really a problem if indeed the queue
      // has been closed.
      // TODO(joth): PCMQueueOutAudioOutputStream uses callback_ to provide an
      // extra guard for this situation, but it seems to introduce more
      // complications than it solves (memory barrier issues accessing it from
      // multiple threads, looses the means to indicate OnClosed to client).
      // Should determine if we need to do something equivalent here.
      return;
    }
    HandleError(err);
  }
}
