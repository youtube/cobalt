// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_CONSUMER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_CONSUMER_H_

#include <fuchsia/media/audio/cpp/fidl.h>
#include <fuchsia/media/audio/cpp/fidl_test_base.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include <list>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace vfs {
class PseudoDir;
}  // namespace vfs

namespace media_m96 {

// Fake implementation of fuchsia::media_m96::AudioConsumer interface. Used for
// tests.
class FakeAudioConsumer final
    : public fuchsia::media_m96::testing::AudioConsumer_TestBase,
      public fuchsia::media_m96::testing::StreamSink_TestBase,
      public fuchsia::media_m96::audio::testing::VolumeControl_TestBase {
 public:
  // Lead time range returned from WatchStatus().
  static const base::TimeDelta kMinLeadTime;
  static const base::TimeDelta kMaxLeadTime;

  FakeAudioConsumer(
      uint64_t session_id,
      fidl::InterfaceRequest<fuchsia::media_m96::AudioConsumer> request);
  ~FakeAudioConsumer() override;

  FakeAudioConsumer(const FakeAudioConsumer&) = delete;
  FakeAudioConsumer& operator=(const FakeAudioConsumer&) = delete;

  uint64_t session_id() { return session_id_; }
  float volume() const { return volume_; }
  bool is_muted() const { return is_muted_; }

  base::TimeDelta GetMediaPosition();

 private:
  enum class State {
    kStopped,
    kPlaying,
    kEndOfStream,
  };

  struct Packet {
    base::TimeDelta pts;
    bool is_eos = false;
  };

  // fuchsia::media_m96::AudioConsumer interface;
  void CreateStreamSink(
      std::vector<zx::vmo> buffers,
      fuchsia::media_m96::AudioStreamType stream_type,
      std::unique_ptr<fuchsia::media_m96::Compression> compression,
      fidl::InterfaceRequest<fuchsia::media_m96::StreamSink> stream_sink_request)
      final;
  void Start(fuchsia::media_m96::AudioConsumerStartFlags flags,
             int64_t reference_time,
             int64_t media_time) override;
  void Stop() override;
  void WatchStatus(WatchStatusCallback callback) override;
  void SetRate(float rate) override;
  void BindVolumeControl(
      fidl::InterfaceRequest<fuchsia::media_m96::audio::VolumeControl>
          volume_control_request) override;

  // fuchsia::media_m96::StreamSink interface.
  void SendPacket(fuchsia::media_m96::StreamPacket packet,
                  SendPacketCallback callback) override;
  void SendPacketNoReply(fuchsia::media_m96::StreamPacket packet) override;
  void EndOfStream() override;
  void DiscardAllPackets(DiscardAllPacketsCallback callback) override;
  void DiscardAllPacketsNoReply() override;

  // fuchsia::media_m96::audio::VolumeControl interface.
  void SetVolume(float volume) override;
  void SetMute(bool mute) override;

  // Not-implemented handler for _TestBase parents.
  void NotImplemented_(const std::string& name) override;

  void ScheduleNextStreamPosUpdate();

  // Updates stream position and drops old packets from the stream.
  void UpdateStreamPos();

  void OnStatusUpdate();
  void CallStatusCallback();

  const uint64_t session_id_;

  fidl::Binding<fuchsia::media_m96::AudioConsumer> audio_consumer_binding_;
  fidl::Binding<fuchsia::media_m96::StreamSink> stream_sink_binding_;
  fidl::Binding<fuchsia::media_m96::audio::VolumeControl> volume_control_binding_;

  size_t num_buffers_ = 0;

  State state_ = State::kStopped;

  bool have_status_update_ = true;
  WatchStatusCallback status_callback_;

  base::TimeTicks reference_time_;

  // Numerator and denumerator for current playback rate.
  uint32_t media_delta_ = 1;
  uint32_t reference_delta_ = 1;

  // Last known media position. Min value indicates that the stream position
  // hasn't been set. If stream is playing then value corresponds to
  // |reference_time_|.
  base::TimeDelta media_pos_ = base::TimeDelta::Min();

  std::list<Packet> pending_packets_;

  // Timer to call UpdateStreamPos() for the next packet.
  base::OneShotTimer update_timer_;

  float volume_ = 1.0;
  bool is_muted_ = false;
};

class FakeAudioConsumerService final
    : public fuchsia::media_m96::testing::SessionAudioConsumerFactory_TestBase {
 public:
  explicit FakeAudioConsumerService(vfs::PseudoDir* pseudo_dir);
  ~FakeAudioConsumerService() override;

  FakeAudioConsumerService(const FakeAudioConsumerService&) = delete;
  FakeAudioConsumerService& operator=(const FakeAudioConsumerService&) = delete;

  size_t num_instances() { return audio_consumers_.size(); }
  FakeAudioConsumer* instance(size_t index) {
    return audio_consumers_[index].get();
  }

 private:
  // fuchsia::media_m96::SessionAudioConsumerFactory implementation.
  void CreateAudioConsumer(uint64_t session_id,
                           fidl::InterfaceRequest<fuchsia::media_m96::AudioConsumer>
                               audio_consumer_request) override;

  // Not-implemented handler for SessionAudioConsumerFactory_TestBase.
  void NotImplemented_(const std::string& name) override;

  base::ScopedServiceBinding<fuchsia::media_m96::SessionAudioConsumerFactory>
      binding_;

  std::vector<std::unique_ptr<FakeAudioConsumer>> audio_consumers_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_AUDIO_FAKE_AUDIO_CONSUMER_H_
