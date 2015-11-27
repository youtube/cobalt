/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "media/audio/shell_pulse_audio.h"

#include "base/bind.h"

namespace media {

ShellPulseAudioStream::ShellPulseAudioStream()
    : context_(NULL),
      latency_(kMinLatency),
      stream_(NULL),
      last_request_size_(0),
      host_(NULL) {
}

ShellPulseAudioStream::~ShellPulseAudioStream() {
  if (stream_) {
    pa_stream_set_write_callback(stream_, NULL, NULL);
    pa_stream_set_underflow_callback(stream_, NULL, NULL);
    pa_stream_disconnect(stream_);
    pa_stream_unref(stream_);
  }
}

bool ShellPulseAudioStream::Initialize(ShellPulseAudioContext* context,
                                       Host* host, int rate, int channel) {
  context_ = context;
  host_ = host;
  sample_spec_.rate = rate;
  sample_spec_.channels = channel;
  sample_spec_.format = PA_SAMPLE_FLOAT32LE;

  stream_ = pa_stream_new(context_->GetContext(), "Playback", &sample_spec_,
                          NULL);
  if (!stream_)
    return false;

  pa_stream_set_write_callback(stream_, RequestCallback, this);
  pa_stream_set_underflow_callback(stream_, UnderflowCallback, this);
  buf_attr_.fragsize = ~0;
  buf_attr_.maxlength = pa_usec_to_bytes(latency_, &sample_spec_);
  buf_attr_.minreq = pa_usec_to_bytes(0, &sample_spec_);
  buf_attr_.prebuf = ~0;
  buf_attr_.tlength = buf_attr_.maxlength;

  const pa_stream_flags_t kNoLatency =
      static_cast<pa_stream_flags_t>(PA_STREAM_INTERPOLATE_TIMING |
                                     PA_STREAM_AUTO_TIMING_UPDATE |
                                     PA_STREAM_START_CORKED);
  const pa_stream_flags_t kWithLatency =
      static_cast<pa_stream_flags_t>(kNoLatency | PA_STREAM_ADJUST_LATENCY);
  if (pa_stream_connect_playback(
      stream_, NULL, &buf_attr_, kWithLatency, NULL, NULL) >= 0) {
    return true;
  }

  // Try again without latency flag.
  if (pa_stream_connect_playback(
      stream_, NULL, &buf_attr_, kNoLatency, NULL, NULL) >= 0) {
    return true;
  }
  pa_stream_unref(stream_);
  stream_ = NULL;
  return false;
}

bool ShellPulseAudioStream::Play() {
  return Cork(false);
}

bool ShellPulseAudioStream::Pause() {
  return Cork(true);
}

uint64 ShellPulseAudioStream::GetPlaybackCursorInMicroSeconds() {
  pa_usec_t usec = 0;
  if (pa_stream_get_time(stream_, &usec) == 0)
    return usec;
  return 0;
}

void ShellPulseAudioStream::RequestFrame() {
  host_->RequestFrame(
      last_request_size_, base::Bind(&ShellPulseAudioStream::WriteFrame,
                                     base::Unretained(this)));
}

void ShellPulseAudioStream::RequestCallback(pa_stream* s, size_t length,
                                         void* userdata) {
  ShellPulseAudioStream* stream = static_cast<ShellPulseAudioStream*>(userdata);
  stream->HandleRequest(length);
}

void ShellPulseAudioStream::HandleRequest(size_t length) {
  last_request_size_ = length;
}

void ShellPulseAudioStream::UnderflowCallback(pa_stream* s, void* userdata) {
  ShellPulseAudioStream* stream = static_cast<ShellPulseAudioStream*>(userdata);
  stream->HandleUnderflow();
}

void ShellPulseAudioStream::HandleUnderflow() {
  if (latency_ < kMaxLatency) {
    latency_ *= 2;
    if (latency_ > kMaxLatency)
      latency_ = kMaxLatency;
    buf_attr_.maxlength = pa_usec_to_bytes(latency_, &sample_spec_);
    buf_attr_.tlength = buf_attr_.maxlength;
    pa_stream_set_buffer_attr(stream_, &buf_attr_, NULL, NULL);
  }
}

bool ShellPulseAudioStream::Cork(bool pause) {
  pa_stream_cork(stream_, pause, SuccessCallback, NULL);
  return true;
}

void ShellPulseAudioStream::SuccessCallback(pa_stream* s, int success,
                                         void* userdata) {
}

void ShellPulseAudioStream::WriteFrame(const uint8* data, size_t size) {
  DCHECK_LE(size, last_request_size_);
  if (size != 0)
    pa_stream_write(stream_, data, size, NULL, 0LL, PA_SEEK_RELATIVE);
  last_request_size_ -= size;
}

ShellPulseAudioContext::ShellPulseAudioContext()
    : mainloop_(NULL),
      context_(NULL),
      pulse_thread_("PulseAudioThread") {
}

ShellPulseAudioContext::~ShellPulseAudioContext() {
  pulse_thread_.Stop();
  DCHECK(streams_.empty());
  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }
  if (mainloop_) {
    pa_mainloop_free(mainloop_);
  }
  if (pulse_thread_.IsRunning()) {
    pulse_thread_.Stop();
  }
}

bool ShellPulseAudioContext::Initialize() {
  mainloop_ = pa_mainloop_new();
  context_ = pa_context_new(pa_mainloop_get_api(mainloop_), "ShellPulseAudio");

  // Set the state callback. This will be called from with pa_mainloop_iterate.
  int pa_ready = kInitial;
  pa_context_set_state_callback(context_, StateCallback, &pa_ready);

  // Try to connect to the context, or return on failure.
  if (pa_context_connect(context_, NULL, pa_context_flags_t(), NULL) < 0) {
    DLOG(ERROR) << "Error connecting to context.";
    return false;
  }

  // Wait until the context is ready.
  while (pa_ready == kInitial) {
    pa_mainloop_iterate(mainloop_, 1, NULL);
  }

  // Clear the state callback.
  pa_context_set_state_callback(context_, NULL, NULL);

  if (pa_ready == kReady) {
    base::Thread::Options options;
    options.priority = base::kThreadPriority_RealtimeAudio;
    pulse_thread_.StartWithOptions(options);
    pulse_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&ShellPulseAudioContext::Iterate, base::Unretained(this)));
  }

  return pa_ready == kReady;
}

pa_context* ShellPulseAudioContext::GetContext() {
  return context_;
}

void ShellPulseAudioContext::Iterate() {
  base::AutoLock lock(lock_);
  pulse_thread_.message_loop()->PostDelayedTask(
      FROM_HERE, base::Bind(&ShellPulseAudioContext::Iterate,
                            base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(5));
  pa_mainloop_iterate(mainloop_, 0, NULL);

  for (Streams::iterator iter = streams_.begin();
       iter != streams_.end(); ++iter) {
    (*iter)->RequestFrame();
  }
}

ShellPulseAudioStream* ShellPulseAudioContext::CreateStream(
    ShellPulseAudioStream::Host* host, int rate, int channel) {
  base::AutoLock lock(lock_);

  ShellPulseAudioStream* stream = new ShellPulseAudioStream;
  bool result = stream->Initialize(this, host, rate, channel);
  DCHECK(result);
  streams_.insert(stream);
  return stream;
}

void ShellPulseAudioContext::DestroyStream(ShellPulseAudioStream* stream) {
  base::AutoLock lock(lock_);
  DCHECK(streams_.find(stream) != streams_.end());
  streams_.erase(streams_.find(stream));
  delete stream;
}

void ShellPulseAudioContext::StateCallback(pa_context* c, void* userdata) {
  int* pa_ready = static_cast<int*>(userdata);

  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      *pa_ready = kError;
      break;
    case PA_CONTEXT_READY:
      *pa_ready = kReady;
      break;
    default:
      break;
  }
}

}  // namespace media

