// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_output_mac.h"

#include <CoreServices/CoreServices.h>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_util.h"
#include "media/audio/mac/audio_manager_mac.h"

namespace media {

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

PCMQueueOutAudioOutputStream::PCMQueueOutAudioOutputStream(
    AudioManagerMac* manager, const AudioParameters& params)
    : audio_queue_(NULL),
      source_(NULL),
      manager_(manager),
      packet_size_(params.GetBytesPerBuffer()),
      silence_bytes_(0),
      volume_(1),
      pending_bytes_(0),
      num_source_channels_(params.channels()),
      source_layout_(params.channel_layout()),
      num_core_channels_(0),
      should_swizzle_(false),
      should_down_mix_(false),
      stopped_event_(true /* manual reset */, false /* initial state */),
      num_buffers_left_(kNumBuffers),
      audio_bus_(AudioBus::Create(params)) {
  // We must have a manager.
  DCHECK(manager_);
  // A frame is one sample across all channels. In interleaved audio the per
  // frame fields identify the set of n |channels|. In uncompressed audio, a
  // packet is always one frame.
  format_.mSampleRate = params.sample_rate();
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked;
  format_.mBitsPerChannel = params.bits_per_sample();
  format_.mChannelsPerFrame = params.channels();
  format_.mFramesPerPacket = 1;
  format_.mBytesPerPacket = (format_.mBitsPerChannel * params.channels()) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;
  format_.mReserved = 0;

  memset(buffer_, 0, sizeof(buffer_));
  memset(core_channel_orderings_, 0, sizeof(core_channel_orderings_));
  memset(channel_remap_, 0, sizeof(channel_remap_));

  if (params.bits_per_sample() > 8) {
    format_.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
  }

  // Silence buffer has a duration of 6ms to simulate the behavior of Windows.
  // This value is choosen by experiments and macs cannot keep up with
  // anything less than 6ms.
  silence_bytes_ = format_.mBytesPerFrame * params.sample_rate() * 6 / 1000;
}

PCMQueueOutAudioOutputStream::~PCMQueueOutAudioOutputStream() {
}

void PCMQueueOutAudioOutputStream::HandleError(OSStatus err) {
  // source_ can be set to NULL from another thread. We need to cache its
  // pointer while we operate here. Note that does not mean that the source
  // has been destroyed.
  AudioSourceCallback* source = GetSource();
  if (source)
    source->OnError(this, static_cast<int>(err));
  LOG(ERROR) << "error " << GetMacOSStatusErrorString(err)
             << " (" << err << ")";
}

bool PCMQueueOutAudioOutputStream::Open() {
  // Get the default device id.
  AudioObjectID device_id = 0;
  AudioObjectPropertyAddress property_address = {
      kAudioHardwarePropertyDefaultOutputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
  };
  UInt32 device_id_size = sizeof(device_id);
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address, 0, NULL,
                                            &device_id_size, &device_id);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  // Get the size of the channel layout.
  UInt32 core_layout_size;
  property_address.mSelector = kAudioDevicePropertyPreferredChannelLayout;
  property_address.mScope = kAudioDevicePropertyScopeOutput;
  err = AudioObjectGetPropertyDataSize(device_id, &property_address, 0, NULL,
                                       &core_layout_size);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  // Get the device's channel layout.  This layout may vary in sized based on
  // the number of channels.  Use |core_layout_size| to allocate memory.
  scoped_ptr_malloc<AudioChannelLayout> core_channel_layout;
  core_channel_layout.reset(
      reinterpret_cast<AudioChannelLayout*>(malloc(core_layout_size)));
  memset(core_channel_layout.get(), 0, core_layout_size);
  err = AudioObjectGetPropertyData(device_id, &property_address, 0, NULL,
                                   &core_layout_size,
                                   core_channel_layout.get());
  if (err != noErr) {
    HandleError(err);
    return false;
  }

  num_core_channels_ =
      static_cast<int>(core_channel_layout->mNumberChannelDescriptions);
  if (num_core_channels_ == 2 &&
      ChannelLayoutToChannelCount(source_layout_) > 2) {
    should_down_mix_ = true;
    format_.mChannelsPerFrame = num_core_channels_;
    format_.mBytesPerFrame = (format_.mBitsPerChannel >> 3) *
        format_.mChannelsPerFrame;
    format_.mBytesPerPacket = format_.mBytesPerFrame * format_.mFramesPerPacket;
  } else {
    should_down_mix_ = false;
  }
  // Create the actual queue object and let the OS use its own thread to
  // run its CFRunLoop.
  err = AudioQueueNewOutput(&format_, RenderCallback, this, NULL,
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

  // Capture channel layout in a format we can use.
  for (int i = 0; i < CHANNELS_MAX; ++i)
    core_channel_orderings_[i] = kEmptyChannel;

  bool all_channels_unknown = true;
  for (int i = 0; i < num_core_channels_; ++i) {
    AudioChannelLabel label =
        core_channel_layout->mChannelDescriptions[i].mChannelLabel;
    if (label == kAudioChannelLabel_Unknown) {
      continue;
    }
    all_channels_unknown = false;
    switch (label) {
      case kAudioChannelLabel_Left:
        core_channel_orderings_[LEFT] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][LEFT];
        break;
      case kAudioChannelLabel_Right:
        core_channel_orderings_[RIGHT] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][RIGHT];
        break;
      case kAudioChannelLabel_Center:
        core_channel_orderings_[CENTER] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][CENTER];
        break;
      case kAudioChannelLabel_LFEScreen:
        core_channel_orderings_[LFE] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][LFE];
        break;
      case kAudioChannelLabel_LeftSurround:
        core_channel_orderings_[SIDE_LEFT] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][SIDE_LEFT];
        break;
      case kAudioChannelLabel_RightSurround:
        core_channel_orderings_[SIDE_RIGHT] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][SIDE_RIGHT];
        break;
      case kAudioChannelLabel_LeftCenter:
        core_channel_orderings_[LEFT_OF_CENTER] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][LEFT_OF_CENTER];
        break;
      case kAudioChannelLabel_RightCenter:
        core_channel_orderings_[RIGHT_OF_CENTER] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][RIGHT_OF_CENTER];
        break;
      case kAudioChannelLabel_CenterSurround:
        core_channel_orderings_[BACK_CENTER] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][BACK_CENTER];
        break;
      case kAudioChannelLabel_RearSurroundLeft:
        core_channel_orderings_[BACK_LEFT] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][BACK_LEFT];
        break;
      case kAudioChannelLabel_RearSurroundRight:
        core_channel_orderings_[BACK_RIGHT] = i;
        channel_remap_[i] = kChannelOrderings[source_layout_][BACK_RIGHT];
        break;
      default:
        DLOG(WARNING) << "Channel label not supported";
        channel_remap_[i] = kEmptyChannel;
        break;
    }
  }

  if (all_channels_unknown) {
    return true;
  }

  // Check if we need to adjust the layout.
  // If the device has a BACK_LEFT and no SIDE_LEFT and the source has
  // a SIDE_LEFT but no BACK_LEFT, then move (and preserve the channel).
  // e.g. CHANNEL_LAYOUT_5POINT1 -> CHANNEL_LAYOUT_5POINT1_BACK
  CheckForAdjustedLayout(SIDE_LEFT, BACK_LEFT);
  // Same for SIDE_RIGHT -> BACK_RIGHT.
  CheckForAdjustedLayout(SIDE_RIGHT, BACK_RIGHT);
  // Move BACK_LEFT to SIDE_LEFT.
  // e.g. CHANNEL_LAYOUT_5POINT1_BACK -> CHANNEL_LAYOUT_5POINT1
  CheckForAdjustedLayout(BACK_LEFT, SIDE_LEFT);
  // Same for BACK_RIGHT -> SIDE_RIGHT.
  CheckForAdjustedLayout(BACK_RIGHT, SIDE_RIGHT);
  // Move SIDE_LEFT to LEFT_OF_CENTER.
  // e.g. CHANNEL_LAYOUT_7POINT1 -> CHANNEL_LAYOUT_7POINT1_WIDE
  CheckForAdjustedLayout(SIDE_LEFT, LEFT_OF_CENTER);
  // Same for SIDE_RIGHT -> RIGHT_OF_CENTER.
  CheckForAdjustedLayout(SIDE_RIGHT, RIGHT_OF_CENTER);
  // Move LEFT_OF_CENTER to SIDE_LEFT.
  // e.g. CHANNEL_LAYOUT_7POINT1_WIDE -> CHANNEL_LAYOUT_7POINT1
  CheckForAdjustedLayout(LEFT_OF_CENTER, SIDE_LEFT);
  // Same for RIGHT_OF_CENTER -> SIDE_RIGHT.
  CheckForAdjustedLayout(RIGHT_OF_CENTER, SIDE_RIGHT);
  // For MONO -> STEREO, move audio to LEFT and RIGHT if applicable.
  CheckForAdjustedLayout(CENTER, LEFT);
  CheckForAdjustedLayout(CENTER, RIGHT);

  // Check if we will need to swizzle from source to device layout (maybe not!).
  should_swizzle_ = false;
  for (int i = 0; i < num_core_channels_; ++i) {
    if (kChannelOrderings[source_layout_][i] != core_channel_orderings_[i]) {
      should_swizzle_ = true;
      break;
    }
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
  if (source_) {
    // We request a synchronous stop, so the next call can take some time. In
    // the windows implementation we block here as well.
    SetSource(NULL);
    stopped_event_.Wait();
  }
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

template<class Format>
void PCMQueueOutAudioOutputStream::SwizzleLayout(Format* b, uint32 filled) {
  Format src_format[num_source_channels_];
  int filled_channels = (num_core_channels_ < num_source_channels_) ?
                        num_core_channels_ : num_source_channels_;
  for (uint32 i = 0; i < filled; i += sizeof(src_format),
      b += num_source_channels_) {
    // TODO(fbarchard): This could be further optimized with pshufb.
    memcpy(src_format, b, sizeof(src_format));
    for (int ch = 0; ch < filled_channels; ++ch) {
      if (channel_remap_[ch] != kEmptyChannel &&
          channel_remap_[ch] <= CHANNELS_MAX) {
        b[ch] = src_format[channel_remap_[ch]];
      } else {
        b[ch] = 0;
      }
    }
  }
}

bool PCMQueueOutAudioOutputStream::CheckForAdjustedLayout(
    Channels input_channel,
    Channels output_channel) {
  if (core_channel_orderings_[output_channel] > kEmptyChannel &&
      core_channel_orderings_[input_channel] == kEmptyChannel &&
      kChannelOrderings[source_layout_][input_channel] > kEmptyChannel &&
      kChannelOrderings[source_layout_][output_channel] == kEmptyChannel) {
    channel_remap_[core_channel_orderings_[output_channel]] =
        kChannelOrderings[source_layout_][input_channel];
    return true;
  }
  return false;
}

// Note to future hackers of this function: Do not add locks to this function
// that are held through any calls made back into AudioQueue APIs, or other
// OS audio functions.  This is because the OS dispatch may grab external
// locks, or possibly re-enter this function which can lead to a deadlock.
void PCMQueueOutAudioOutputStream::RenderCallback(void* p_this,
                                                  AudioQueueRef queue,
                                                  AudioQueueBufferRef buffer) {
  TRACE_EVENT0("audio", "PCMQueueOutAudioOutputStream::RenderCallback");

  PCMQueueOutAudioOutputStream* audio_stream =
      static_cast<PCMQueueOutAudioOutputStream*>(p_this);

  // Call the audio source to fill the free buffer with data. Not having a
  // source means that the queue has been stopped.
  AudioSourceCallback* source = audio_stream->GetSource();
  if (!source) {
    // PCMQueueOutAudioOutputStream::Stop() is waiting for callback to
    // stop the stream and signal when all callbacks are done.
    // (we probably can stop the stream there, but it is better to have
    // all the complex logic in one place; stopping latency is not very
    // important if you reuse audio stream in the mixer and not close it
    // immediately).
    --audio_stream->num_buffers_left_;
    if (audio_stream->num_buffers_left_ == kNumBuffers - 1) {
      // First buffer after stop requested, stop the queue.
      OSStatus err = AudioQueueStop(audio_stream->audio_queue_, true);
      if (err != noErr)
        audio_stream->HandleError(err);
    }
    if (audio_stream->num_buffers_left_ == 0) {
      // Now we finally saw all the buffers.
      // Signal that stopping is complete.
      // Should never touch audio_stream after signaling as it
      // can be deleted at any moment.
      audio_stream->stopped_event_.Signal();
    }
    return;
  }

  // Adjust the number of pending bytes by subtracting the amount played.
  if (!static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer)
    audio_stream->pending_bytes_ -= buffer->mAudioDataByteSize;

  uint32 capacity = buffer->mAudioDataBytesCapacity;
  AudioBus* audio_bus = audio_stream->audio_bus_.get();
  DCHECK_EQ(
      audio_bus->frames() * audio_stream->format_.mBytesPerFrame, capacity);
  // TODO(sergeyu): Specify correct hardware delay for AudioBuffersState.
  int frames_filled = source->OnMoreData(
      audio_bus, AudioBuffersState(audio_stream->pending_bytes_, 0));
  uint32 filled = frames_filled * audio_stream->format_.mBytesPerFrame;
  // Note: If this ever changes to output raw float the data must be clipped and
  // sanitized since it may come from an untrusted source such as NaCl.
  audio_bus->ToInterleaved(
      frames_filled, audio_stream->format_.mBitsPerChannel / 8,
      buffer->mAudioData);

  // In order to keep the callback running, we need to provide a positive amount
  // of data to the audio queue. To simulate the behavior of Windows, we write
  // a buffer of silence.
  if (!filled) {
    CHECK(audio_stream->silence_bytes_ <= static_cast<int>(capacity));
    filled = audio_stream->silence_bytes_;

    // Assume unsigned audio.
    int silence_value = 128;
    if (audio_stream->format_.mBitsPerChannel > 8) {
      // When bits per channel is greater than 8, audio is signed.
      silence_value = 0;
    }

    memset(buffer->mAudioData, silence_value, filled);
    static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer = true;
  } else if (filled > capacity) {
    // User probably overran our buffer.
    audio_stream->HandleError(0);
    return;
  } else {
    static_cast<AudioQueueUserData*>(buffer->mUserData)->empty_buffer = false;
  }

  if (audio_stream->should_down_mix_) {
    // Downmixes the L, R, C channels to stereo.
    if (media::FoldChannels(buffer->mAudioData,
                            filled,
                            audio_stream->num_source_channels_,
                            audio_stream->format_.mBitsPerChannel >> 3,
                            audio_stream->volume_)) {
      filled = filled * 2 / audio_stream->num_source_channels_;
    } else {
      LOG(ERROR) << "Folding failed";
    }
  } else if (audio_stream->should_swizzle_) {
    // Handle channel order for surround sound audio.
    if (audio_stream->format_.mBitsPerChannel == 8) {
      audio_stream->SwizzleLayout(reinterpret_cast<uint8*>(buffer->mAudioData),
                                  filled);
    } else if (audio_stream->format_.mBitsPerChannel == 16) {
      audio_stream->SwizzleLayout(reinterpret_cast<int16*>(buffer->mAudioData),
                                  filled);
    } else if (audio_stream->format_.mBitsPerChannel == 32) {
      audio_stream->SwizzleLayout(reinterpret_cast<int32*>(buffer->mAudioData),
                                  filled);
    }
  }

  buffer->mAudioDataByteSize = filled;

  // Increment bytes by amount filled into audio buffer if this is not a
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
      // has been closed.  We recheck the value of source now to see if it has
      // indeed been closed.
      if (!audio_stream->GetSource())
        return;
    }
    audio_stream->HandleError(err);
  }
}

void PCMQueueOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  DLOG_IF(ERROR, !audio_queue_) << "Open() has not been called successfully";
  if (!audio_queue_)
    return;

  OSStatus err = noErr;
  SetSource(callback);
  pending_bytes_ = 0;
  stopped_event_.Reset();
  num_buffers_left_ = kNumBuffers;
  // Ask the source to pre-fill all our buffers before playing.
  for (uint32 ix = 0; ix != kNumBuffers; ++ix) {
    buffer_[ix]->mAudioDataByteSize = 0;
    // Caller waits for 1st packet to become available, but not for others,
    // so we wait for them here.
    if (ix != 0) {
      AudioSourceCallback* source = GetSource();
      if (source)
        source->WaitTillDataReady();
    }
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

void PCMQueueOutAudioOutputStream::SetSource(AudioSourceCallback* source) {
  base::AutoLock lock(source_lock_);
  source_ = source;
}

AudioOutputStream::AudioSourceCallback*
PCMQueueOutAudioOutputStream::GetSource() {
  base::AutoLock lock(source_lock_);
  return source_;
}

}  // namespace media
