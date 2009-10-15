// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THREAD SAFETY
//
// The AlsaPcmOutputStream object's internal state is accessed by two threads:
//
//   client thread - creates the object and calls the public APIs.
//   message loop thread - executes all the internal tasks including querying
//      the data source for more data, writing to the alsa device, and closing
//      the alsa device.  It does *not* handle opening the device though.
//
// The class is designed so that most operations that read/modify the object's
// internal state are done on the message loop thread.  The exception is data
// conatined in the |shared_data_| structure.  Data in this structure needs to
// be accessed by both threads, and should only be accessed when the
// |shared_data_.lock_| is held.
//
// All member variables that are not in |shared_data_| are created/destroyed on
// the |message_loop_|.  This allows safe access to them from any task posted to
// |message_loop_|.  The values in |shared_data_| are considered to be read-only
// signals by tasks posted to |message_loop_| (see the SEMANTICS of
// |shared_data_| section below).  Because of these two constraints, tasks can,
// and must, be coded to be safe in the face of a changing |shared_data_|.
//
//
// SEMANTICS OF |shared_data_|
//
// Though |shared_data_| is accessable by both threads, the code has been
// structured so that all mutations to |shared_data_| are only done in the
// client thread.  The message loop thread only ever reads the shared data.
//
// This reduces the need for critical sections because the public API code can
// assume that no mutations occur to the |shared_data_| between queries.
//
// On the message loop side, tasks have been coded such that they can
// operate safely regardless of when state changes happen to |shared_data_|.
// Code that is sensitive to the timing of state changes are delegated to the
// |shared_data_| object so they can executed while holding
// |shared_data_.lock_|.
//
//
// SEMANTICS OF CloseTask()
//
// The CloseTask() is responsible for cleaning up any resources that were
// acquired after a successful Open().  After a CloseTask() has executed,
// scheduling of reads should stop.  Currently scheduled tasks may run, but
// they should not attempt to access any of the internal structures beyond
// querying the |stop_stream_| flag and no-oping themselves.  This will
// guarantee that eventually no more tasks will be posted into the message
// loop, and the AlsaPcmOutputStream will be able to delete itself.
//
//
// SEMANTICS OF ERROR STATES
//
// The object has two distinct error states: |shared_data_.state_| == kInError
// and |stop_stream_|.  The |shared_data_.state_| state is only settable
// by the client thread, and thus cannot be used to signal when the ALSA device
// fails due to a hardware (or other low-level) event.  The |stop_stream_|
// variable is only accessed by the message loop thread; it is used to indicate
// that the playback_handle should no longer be used either because of a
// hardware/low-level event, or because the CloseTask() has been run.
//
// When |shared_data_.state_| == kInError, all public API functions will fail
// with an error (Start() will call the OnError() function on the callback
// immediately), or no-op themselves with the exception of Close().  Even if an
// error state has been entered, if Open() has previously returned successfully,
// Close() must be called to cleanup the ALSA devices and release resources.
//
// When |stop_stream_| is set, no more commands will be made against the
// ALSA device, and playback will effectively stop.  From the client's point of
// view, it will seem that the device has just clogged and stopped requesting
// data.

#include "media/audio/linux/alsa_output.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "media/audio/audio_util.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/audio/linux/audio_manager_linux.h"

// Amount of time to wait if we've exhausted the data source.  This is to avoid
// busy looping.
static const int kNoDataSleepMilliseconds = 10;

// According to the linux nanosleep manpage, nanosleep on linux can miss the
// deadline by up to 10ms because the kernel timeslice is 10ms.  Give a 2x
// buffer to compensate for the timeslice, and any additional slowdowns.
static const int kSleepErrorMilliseconds = 20;

// Set to 0 during debugging if you want error messages due to underrun
// events or other recoverable errors.
#if defined(NDEBUG)
static const int kPcmRecoverIsSilent = 1;
#else
static const int kPcmRecoverIsSilent = 0;
#endif

const char AlsaPcmOutputStream::kDefaultDevice[] = "default";

namespace {

snd_pcm_format_t BitsToFormat(char bits_per_sample) {
  switch (bits_per_sample) {
    case 8:
      return SND_PCM_FORMAT_S8;

    case 16:
      return SND_PCM_FORMAT_S16;

    case 24:
      return SND_PCM_FORMAT_S24;

    case 32:
      return SND_PCM_FORMAT_S32;

    default:
      return SND_PCM_FORMAT_UNKNOWN;
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         AlsaPcmOutputStream::InternalState state) {
  switch (state) {
    case AlsaPcmOutputStream::kInError:
      os << "kInError";
      break;
    case AlsaPcmOutputStream::kCreated:
      os << "kCreated";
      break;
    case AlsaPcmOutputStream::kIsOpened:
      os << "kIsOpened";
      break;
    case AlsaPcmOutputStream::kIsPlaying:
      os << "kIsPlaying";
      break;
    case AlsaPcmOutputStream::kIsStopped:
      os << "kIsStopped";
      break;
    case AlsaPcmOutputStream::kIsClosed:
      os << "kIsClosed";
      break;
  };
  return os;
}

AlsaPcmOutputStream::AlsaPcmOutputStream(const std::string& device_name,
                                         AudioManager::Format format,
                                         int channels,
                                         int sample_rate,
                                         int bits_per_sample,
                                         AlsaWrapper* wrapper,
                                         AudioManagerLinux* manager,
                                         MessageLoop* message_loop)
    : shared_data_(MessageLoop::current()),
      device_name_(device_name),
      pcm_format_(BitsToFormat(bits_per_sample)),
      channels_(channels),
      sample_rate_(sample_rate),
      bytes_per_sample_(bits_per_sample / 8),
      bytes_per_frame_(channels_ * bits_per_sample / 8),
      stop_stream_(false),
      wrapper_(wrapper),
      manager_(manager),
      playback_handle_(NULL),
      frames_per_packet_(0),
      client_thread_loop_(MessageLoop::current()),
      message_loop_(message_loop) {

  // Sanity check input values.
  //
  // TODO(scherkus): ALSA works fine if you pass in multichannel audio, however
  // it seems to be mapped to the wrong channels.  We may have to do some
  // channel swizzling from decoder output to ALSA's preferred multichannel
  // format.
  if (channels_ != 1 && channels_ != 2) {
    LOG(WARNING) << "Only 1 and 2 channel audio is supported right now.";
    shared_data_.TransitionTo(kInError);
  }

  if (AudioManager::AUDIO_PCM_LINEAR != format) {
    LOG(WARNING) << "Only linear PCM supported.";
    shared_data_.TransitionTo(kInError);
  }

  if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
    LOG(WARNING) << "Unsupported bits per sample: " << bits_per_sample;
    shared_data_.TransitionTo(kInError);
  }
}

AlsaPcmOutputStream::~AlsaPcmOutputStream() {
  InternalState state = shared_data_.state();
  DCHECK(state == kCreated || state == kIsClosed || state == kInError);

  // TODO(ajwong): Ensure that CloseTask has been called and the
  // playback handle released by DCHECKing that playback_handle_ is NULL.
  // Currently, because of Bug 18217, there is a race condition on destruction
  // where the stream is not always stopped and closed, causing this to fail.
}

bool AlsaPcmOutputStream::Open(size_t packet_size) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  DCHECK_EQ(0U, packet_size % bytes_per_frame_)
      << "Buffers should end on a frame boundary. Frame size: "
      << bytes_per_frame_;

  if (shared_data_.state() == kInError) {
    return false;
  }

  if (!shared_data_.CanTransitionTo(kIsOpened)) {
    NOTREACHED() << "Invalid state: " << shared_data_.state();
    return false;
  }

  // Try to open the device.
  snd_pcm_t* handle = NULL;
  int error = wrapper_->PcmOpen(&handle, device_name_.c_str(),
                                SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
  if (error < 0) {
    LOG(ERROR) << "Cannot open audio device (" << device_name_ << "): "
               << wrapper_->StrError(error);
    return false;
  }

  // Configure the device for software resampling, and add enough buffer for
  // two audio packets.
  int micros_per_packet =
      FramesToMicros(packet_size / bytes_per_frame_, sample_rate_);
  if ((error = wrapper_->PcmSetParams(handle,
                                      pcm_format_,
                                      SND_PCM_ACCESS_RW_INTERLEAVED,
                                      channels_,
                                      sample_rate_,
                                      1,  // soft_resample -- let ALSA resample
                                      micros_per_packet * 2)) < 0) {
    LOG(ERROR) << "Unable to set PCM parameters for (" << device_name_
               << "): " << wrapper_->StrError(error);
    if (!CloseDevice(handle)) {
      // TODO(ajwong): Retry on certain errors?
      LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    }
    return false;
  }

  // We do not need to check if the transition was successful because
  // CanTransitionTo() was checked above, and it is assumed that this
  // object's public API is only called on one thread so the state cannot
  // transition out from under us.
  shared_data_.TransitionTo(kIsOpened);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AlsaPcmOutputStream::FinishOpen,
                        handle, packet_size));

  return true;
}

void AlsaPcmOutputStream::Close() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  // Sanity check that the transition occurs correctly.  It is safe to
  // continue anyways because all operations for closing are idempotent.
  if (shared_data_.TransitionTo(kIsClosed) != kIsClosed) {
    NOTREACHED() << "Unable to transition Closed.";
  }

  // Signal our successful close, and disassociate the source callback.
  shared_data_.OnClose(this);
  shared_data_.set_source_callback(NULL);

  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AlsaPcmOutputStream::CloseTask));

  // Signal to the manager that we're closed and can be removed.  Since
  // we just posted a CloseTask to the message loop, we won't be deleted
  // immediately, but it will happen soon afterwards.
  manager()->ReleaseStream(this);
}

void AlsaPcmOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  CHECK(callback);

  shared_data_.set_source_callback(callback);

  // Only post the task if we can enter the playing state.
  if (shared_data_.TransitionTo(kIsPlaying) == kIsPlaying) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AlsaPcmOutputStream::StartTask));
  }
}

void AlsaPcmOutputStream::Stop() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  shared_data_.TransitionTo(kIsStopped);
}

void AlsaPcmOutputStream::SetVolume(double left_level, double right_level) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  shared_data_.set_volume(static_cast<float>(left_level));
}

void AlsaPcmOutputStream::GetVolume(double* left_level, double* right_level) {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);

  *left_level = *right_level = shared_data_.volume();
}

void AlsaPcmOutputStream::FinishOpen(snd_pcm_t* playback_handle,
                                     size_t packet_size) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  playback_handle_ = playback_handle;
  packet_.reset(new Packet(packet_size));
  frames_per_packet_ = packet_size / bytes_per_frame_;
}

void AlsaPcmOutputStream::StartTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // When starting again, drop all packets in the device and prepare it again
  // incase we are restarting from a pause state and need to flush old data.
  int error = wrapper_->PcmDrop(playback_handle_);
  if (error < 0 && error != -EAGAIN) {
    LOG(ERROR) << "Failure clearing playback device ("
               << wrapper_->PcmName(playback_handle_) << "): "
               << wrapper_->StrError(error);
    stop_stream_ = true;
    return;
  }

  error = wrapper_->PcmPrepare(playback_handle_);
  if (error < 0 && error != -EAGAIN) {
    LOG(ERROR) << "Failure preparing stream ("
               << wrapper_->PcmName(playback_handle_) << "): "
               << wrapper_->StrError(error);
    stop_stream_ = true;
    return;
  }

  // Do a best-effort write of 2 packets to pre-roll.
  //
  // TODO(ajwong): Make this track with the us_latency set in Open().
  // Also handle EAGAIN.
  BufferPacket(packet_.get());
  WritePacket(packet_.get());
  BufferPacket(packet_.get());
  WritePacket(packet_.get());

  ScheduleNextWrite(packet_.get());
}

void AlsaPcmOutputStream::CloseTask() {
  // NOTE: Keep this function idempotent to handle errors that might cause
  // multiple CloseTasks to be posted.
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Shutdown the audio device.
  if (playback_handle_ && !CloseDevice(playback_handle_)) {
    LOG(WARNING) << "Unable to close audio device. Leaking handle.";
  }
  playback_handle_ = NULL;

  // Release the buffer.
  packet_.reset();

  // Signal anything that might already be scheduled to stop.
  stop_stream_ = true;
}

void AlsaPcmOutputStream::BufferPacket(Packet* packet) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // If stopped, simulate a 0-lengthed packet.
  if (stop_stream_) {
    packet->used = packet->size = 0;
    return;
  }

  // Request more data if we don't have any cached.
  if (packet->used >= packet->size) {
    // Before making a request to source for data. We need to determine the
    // delay (in bytes) for the requested data to be played.
    snd_pcm_sframes_t delay;
    int error = wrapper_->PcmDelay(playback_handle_, &delay);
    if (error < 0) {
      error = wrapper_->PcmRecover(playback_handle_,
                                   error,
                                   kPcmRecoverIsSilent);
      if (error < 0) {
        LOG(ERROR) << "Failed querying delay: " << wrapper_->StrError(error);
      }

      // TODO(hclam): If we cannot query the delay, we may want to stop
      // the playback and report an error.
      delay = 0;
    } else {
      delay *= bytes_per_frame_;
    }

    packet->used = 0;
    packet->size = shared_data_.OnMoreData(this, packet->buffer.get(),
                                           packet->capacity, delay);
    CHECK(packet->size <= packet->capacity) << "Data source overran buffer.";

    // This should not happen, but incase it does, drop any trailing bytes
    // that aren't large enough to make a frame.  Without this, packet writing
    // may stall because the last few bytes in the packet may never get used by
    // WritePacket.
    DCHECK(packet->size % bytes_per_frame_ == 0);
    packet->size = (packet->size / bytes_per_frame_) * bytes_per_frame_;

    media::AdjustVolume(packet->buffer.get(),
                        packet->size,
                        channels_,
                        bytes_per_sample_,
                        shared_data_.volume());
  }
}

void AlsaPcmOutputStream::WritePacket(Packet* packet) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  CHECK(packet->size % bytes_per_frame_ == 0);

  // If the device is in error, just eat the bytes.
  if (stop_stream_) {
    packet->used = packet->size;
    return;
  }

  if (packet->used < packet->size) {
    char* buffer_pos = packet->buffer.get() + packet->used;
    snd_pcm_sframes_t frames = FramesInPacket(*packet, bytes_per_frame_);

    DCHECK_GT(frames, 0);

    snd_pcm_sframes_t frames_written =
        wrapper_->PcmWritei(playback_handle_, buffer_pos, frames);
    if (frames_written < 0) {
      // Attempt once to immediately recover from EINTR,
      // EPIPE (overrun/underrun), ESTRPIPE (stream suspended).  WritePacket
      // will eventually be called again, so eventual recovery will happen if
      // muliple retries are required.
      frames_written = wrapper_->PcmRecover(playback_handle_,
                                            frames_written,
                                            kPcmRecoverIsSilent);
    }

    if (frames_written < 0) {
      // TODO(ajwong): Is EAGAIN the only error we want to except from stopping
      // the pcm playback?
      if (frames_written != -EAGAIN) {
        LOG(ERROR) << "Failed to write to pcm device: "
                   << wrapper_->StrError(frames_written);
        shared_data_.OnError(this, frames_written);
        stop_stream_ = true;
      }
    } else {
      packet->used += frames_written * bytes_per_frame_;
    }
  }
}

void AlsaPcmOutputStream::WriteTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (stop_stream_) {
    return;
  }

  BufferPacket(packet_.get());
  WritePacket(packet_.get());

  ScheduleNextWrite(packet_.get());
}

void AlsaPcmOutputStream::ScheduleNextWrite(Packet* current_packet) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (stop_stream_) {
    return;
  }

  // Calculate when we should have enough buffer for another packet of data.
  int frames_leftover = FramesInPacket(*current_packet, bytes_per_frame_);
  int frames_needed =
      frames_leftover > 0 ? frames_leftover : frames_per_packet_;
  int frames_until_empty_enough = frames_needed - GetAvailableFrames();
  int next_fill_time_ms =
      FramesToMillis(frames_until_empty_enough, sample_rate_);

  // Adjust for timer resolution issues.
  if (next_fill_time_ms > kSleepErrorMilliseconds) {
    next_fill_time_ms -= kSleepErrorMilliseconds;
  }

  // Avoid busy looping if the data source is exhausted.
  if (current_packet->size == 0) {
    next_fill_time_ms = std::max(next_fill_time_ms, kNoDataSleepMilliseconds);
  }

  // Only schedule more reads/writes if we are still in the playing state.
  if (shared_data_.state() == kIsPlaying) {
    if (next_fill_time_ms <= 0) {
      message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &AlsaPcmOutputStream::WriteTask));
    } else {
      // TODO(ajwong): Measure the reliability of the delay interval.  Use
      // base/histogram.h.
      message_loop_->PostDelayedTask(
          FROM_HERE,
          NewRunnableMethod(this, &AlsaPcmOutputStream::WriteTask),
          next_fill_time_ms);
    }
  }
}

snd_pcm_sframes_t AlsaPcmOutputStream::FramesInPacket(const Packet& packet,
                                                      int bytes_per_frame) {
  return (packet.size - packet.used) / bytes_per_frame;
}

int64 AlsaPcmOutputStream::FramesToMicros(int frames, int sample_rate) {
  return frames * base::Time::kMicrosecondsPerSecond / sample_rate;
}

int64 AlsaPcmOutputStream::FramesToMillis(int frames, int sample_rate) {
  return frames * base::Time::kMillisecondsPerSecond / sample_rate;
}

bool AlsaPcmOutputStream::CloseDevice(snd_pcm_t* handle) {
  int error = wrapper_->PcmClose(handle);
  if (error < 0) {
    LOG(ERROR) << "Cannot close audio device (" << wrapper_->PcmName(handle)
               << "): " << wrapper_->StrError(error);
    return false;
  }

  return true;
}

snd_pcm_sframes_t AlsaPcmOutputStream::GetAvailableFrames() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (stop_stream_) {
    return 0;
  }

  // Find the number of frames queued in the sound device.
  snd_pcm_sframes_t available_frames =
      wrapper_->PcmAvailUpdate(playback_handle_);
  if (available_frames  < 0) {
    available_frames = wrapper_->PcmRecover(playback_handle_,
                                            available_frames,
                                            kPcmRecoverIsSilent);
  }
  if (available_frames < 0) {
    LOG(ERROR) << "Failed querying available frames. Assuming 0: "
               << wrapper_->StrError(available_frames);
    return 0;
  }

  return available_frames;
}

AudioManagerLinux* AlsaPcmOutputStream::manager() {
  DCHECK_EQ(MessageLoop::current(), client_thread_loop_);
  return manager_;
}

AlsaPcmOutputStream::SharedData::SharedData(
    MessageLoop* state_transition_loop)
    : state_(kCreated),
      volume_(1.0f),
      source_callback_(NULL),
      state_transition_loop_(state_transition_loop) {
}

bool AlsaPcmOutputStream::SharedData::CanTransitionTo(InternalState to) {
  AutoLock l(lock_);
  return CanTransitionTo_Locked(to);
}

bool AlsaPcmOutputStream::SharedData::CanTransitionTo_Locked(
    InternalState to) {
  lock_.AssertAcquired();

  switch (state_) {
    case kCreated:
      return to == kIsOpened || to == kIsClosed || to == kInError;

    case kIsOpened:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsPlaying:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kIsStopped:
      return to == kIsPlaying || to == kIsStopped ||
          to == kIsClosed || to == kInError;

    case kInError:
      return to == kIsClosed || to == kInError;

    case kIsClosed:
    default:
      return false;
  }
}

AlsaPcmOutputStream::InternalState
AlsaPcmOutputStream::SharedData::TransitionTo(InternalState to) {
  DCHECK_EQ(MessageLoop::current(), state_transition_loop_);

  AutoLock l(lock_);
  if (!CanTransitionTo_Locked(to)) {
    NOTREACHED() << "Cannot transition from: " << state_ << " to: " << to;
    state_ = kInError;
  } else {
    state_ = to;
  }
  return state_;
}

AlsaPcmOutputStream::InternalState AlsaPcmOutputStream::SharedData::state() {
  AutoLock l(lock_);
  return state_;
}

float AlsaPcmOutputStream::SharedData::volume() {
  AutoLock l(lock_);
  return volume_;
}

void AlsaPcmOutputStream::SharedData::set_volume(float v) {
  AutoLock l(lock_);
  volume_ = v;
}

size_t AlsaPcmOutputStream::SharedData::OnMoreData(AudioOutputStream* stream,
                                                   void* dest,
                                                   size_t max_size,
                                                   int pending_bytes) {
  AutoLock l(lock_);
  if (source_callback_) {
    return source_callback_->OnMoreData(stream, dest, max_size, pending_bytes);
  }

  return 0;
}

void AlsaPcmOutputStream::SharedData::OnClose(AudioOutputStream* stream) {
  AutoLock l(lock_);
  if (source_callback_) {
    source_callback_->OnClose(stream);
  }
}

void AlsaPcmOutputStream::SharedData::OnError(AudioOutputStream* stream,
                                              int code) {
  AutoLock l(lock_);
  if (source_callback_) {
    source_callback_->OnError(stream, code);
  }
}

// Changes the AudioSourceCallback to proxy calls to.  Pass in NULL to
// release ownership of the currently registered callback.
void AlsaPcmOutputStream::SharedData::set_source_callback(
    AudioSourceCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), state_transition_loop_);
  AutoLock l(lock_);
  source_callback_ = callback;
}
