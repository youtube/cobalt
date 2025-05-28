// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/peer_connection_remote_audio_source.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"

namespace blink {

namespace {
// Used as an identifier for the down-casters.
void* const kPeerConnectionRemoteTrackIdentifier =
    const_cast<void**>(&kPeerConnectionRemoteTrackIdentifier);

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("PCRAS::" + message);
}

}  // namespace

PeerConnectionRemoteAudioTrack::PeerConnectionRemoteAudioTrack(
    scoped_refptr<webrtc::AudioTrackInterface> track_interface)
    : MediaStreamAudioTrack(false /* is_local_track */),
      track_interface_(std::move(track_interface)) {
  blink::WebRtcLogMessage(
      base::StringPrintf("PCRAT::PeerConnectionRemoteAudioTrack({id=%s})",
                         track_interface_->id().c_str()));
}

PeerConnectionRemoteAudioTrack::~PeerConnectionRemoteAudioTrack() {
  blink::WebRtcLogMessage(
      base::StringPrintf("PCRAT::~PeerConnectionRemoteAudioTrack([id=%s])",
                         track_interface_->id().c_str()));
  // Ensure the track is stopped.
  MediaStreamAudioTrack::Stop();
}

// static
PeerConnectionRemoteAudioTrack* PeerConnectionRemoteAudioTrack::From(
    MediaStreamAudioTrack* track) {
  if (track &&
      track->GetClassIdentifier() == kPeerConnectionRemoteTrackIdentifier)
    return static_cast<PeerConnectionRemoteAudioTrack*>(track);
  return nullptr;
}

void PeerConnectionRemoteAudioTrack::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  blink::WebRtcLogMessage(base::StringPrintf(
      "PCRAT::SetEnabled([id=%s] {enabled=%s})", track_interface_->id().c_str(),
      base::ToString(enabled).c_str()));

  // TODO(crbug.com/40849402): AudioTrackInterface::set_enabled() is not called
  // because doing so would set the volume to 0 for the source level in the
  // receiving audio in the WebRTC side.
  // For now, we skip calling AudioTrackInterface::set_enabled() to avoid these
  // issues. We need to monitor this approach to ensure that skipping
  // set_enabled() does not introduce regressions.

  MediaStreamAudioTrack::SetEnabled(enabled);
}

void* PeerConnectionRemoteAudioTrack::GetClassIdentifier() const {
  return kPeerConnectionRemoteTrackIdentifier;
}

PeerConnectionRemoteAudioSource::PeerConnectionRemoteAudioSource(
    scoped_refptr<webrtc::AudioTrackInterface> track_interface,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : MediaStreamAudioSource(std::move(task_runner),
                             false /* is_local_source */),
      track_interface_(std::move(track_interface)),
      is_sink_of_peer_connection_(false) {
  DCHECK(track_interface_);
  SendLogMessage(base::StringPrintf("PeerConnectionRemoteAudioSource([id=%s])",
                                    track_interface_->id().c_str()));
}

PeerConnectionRemoteAudioSource::~PeerConnectionRemoteAudioSource() {
  SendLogMessage(base::StringPrintf("~PeerConnectionRemoteAudioSource([id=%s])",
                                    track_interface_->id().c_str()));
  EnsureSourceIsStopped();
}

std::unique_ptr<MediaStreamAudioTrack>
PeerConnectionRemoteAudioSource::CreateMediaStreamAudioTrack(
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<PeerConnectionRemoteAudioTrack>(track_interface_);
}

bool PeerConnectionRemoteAudioSource::EnsureSourceIsStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_sink_of_peer_connection_)
    return true;
  SendLogMessage(base::StringPrintf("EnsureSourceIsStarted([id=%s])",
                                    track_interface_->id().c_str()));
  track_interface_->AddSink(this);
  is_sink_of_peer_connection_ = true;
  return true;
}

void PeerConnectionRemoteAudioSource::EnsureSourceIsStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_sink_of_peer_connection_) {
    SendLogMessage(base::StringPrintf("EnsureSourceIsStopped([id=%s])",
                                      track_interface_->id().c_str()));
    track_interface_->RemoveSink(this);
    is_sink_of_peer_connection_ = false;
  }
}

void PeerConnectionRemoteAudioSource::OnData(const void* audio_data,
                                             int bits_per_sample,
                                             int sample_rate,
                                             size_t number_of_channels,
                                             size_t number_of_frames) {
  // Debug builds: Note that this lock isn't meant to synchronize anything.
  // Instead, it is being used as a run-time check to ensure there isn't already
  // another thread executing this method. The reason we don't use
  // base::ThreadChecker here is because we shouldn't be making assumptions
  // about the private threading model of libjingle. For example, it would be
  // legitimate for libjingle to use a different thread to invoke this method
  // whenever the audio format changes.
#ifndef NDEBUG
  CHECK(single_audio_thread_guard_.Try());
#endif

  TRACE_EVENT2("audio", "PeerConnectionRemoteAudioSource::OnData",
               "sample_rate", sample_rate, "number_of_frames",
               number_of_frames);
  // TODO(tommi): We should get the timestamp from WebRTC.
  base::TimeTicks playout_time(base::TimeTicks::Now());

  int channels_int = base::checked_cast<int>(number_of_channels);
  int frames_int = base::checked_cast<int>(number_of_frames);
  if (!audio_bus_ || audio_bus_->channels() != channels_int ||
      audio_bus_->frames() != frames_int) {
    audio_bus_ = media::AudioBus::Create(channels_int, frames_int);
  }

  // Only 16 bits per sample is ever used. The FromInterleaved() call should
  // be updated if that is no longer the case.
  DCHECK_EQ(bits_per_sample, 16);
  audio_bus_->FromInterleaved<media::SignedInt16SampleTypeTraits>(
      reinterpret_cast<const int16_t*>(audio_data), frames_int);

  media::AudioParameters params = MediaStreamAudioSource::GetAudioParameters();
  if (!params.IsValid() ||
      params.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY ||
      params.channels() != channels_int ||
      params.sample_rate() != sample_rate ||
      params.frames_per_buffer() != frames_int) {
    MediaStreamAudioSource::SetFormat(
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Guess(channels_int),
                               sample_rate, frames_int));
  }

  MediaStreamAudioSource::DeliverDataToTracks(*audio_bus_, playout_time, {});

#ifndef NDEBUG
  single_audio_thread_guard_.Release();
#endif
}

}  // namespace blink
