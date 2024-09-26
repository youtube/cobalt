// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_connection_event_logger.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"

#ifndef ARCH_CPU_64_BITS
#include "base/synchronization/lock.h"
#endif

namespace remoting {
namespace test {
namespace {

template <typename T>
class NoBarrierAtomic {
 public:
  T operator++() { return base::subtle::NoBarrier_AtomicIncrement(&i_, 1) - 1; }

  T operator++(int) { return base::subtle::NoBarrier_AtomicIncrement(&i_, 1); }

  T operator--() {
    return base::subtle::NoBarrier_AtomicIncrement(&i_, -1) - 1;
  }

  T operator--(int) { return base::subtle::NoBarrier_AtomicIncrement(&i_, -1); }

  T operator+=(T other) {
    return base::subtle::NoBarrier_AtomicIncrement(&i_, other);
  }

  T operator-=(T other) {
    return base::subtle::NoBarrier_AtomicIncrement(&i_, -other);
  }

  T operator*() const { return base::subtle::NoBarrier_Load(&i_); }

 private:
  volatile T i_;
};

class NoBarrierAtomicInt32 : public NoBarrierAtomic<base::subtle::Atomic32> {};
#ifdef ARCH_CPU_64_BITS
class NoBarrierAtomicInt64 : public NoBarrierAtomic<base::subtle::Atomic64> {};
#else  // ifdef ARCH_CPU_64_BITS

using base::AutoLock;

// A barriered, lock based implementation
class NoBarrierAtomicInt64 {
 public:
  int64_t operator++();
  int64_t operator++(int);
  int64_t operator--();
  int64_t operator--(int);
  int64_t operator+=(int64_t other);
  int64_t operator-=(int64_t other);
  int64_t operator*() const;

 private:
  volatile int64_t i_ = 0;
  mutable base::Lock lock_;  // field is used in operator*() const
};

int64_t NoBarrierAtomicInt64::operator++() {
  AutoLock l(lock_);
  const int64_t old_i = i_;
  i_ = old_i + 1;
  return old_i;
}

int64_t NoBarrierAtomicInt64::operator++(int) {
  AutoLock l(lock_);
  const int64_t new_i = i_ + 1;
  i_ = new_i;
  return new_i;
}

int64_t NoBarrierAtomicInt64::operator--() {
  AutoLock l(lock_);
  const int64_t old_i = i_;
  i_ = old_i - 1;
  return old_i;
}

int64_t NoBarrierAtomicInt64::operator--(int) {
  AutoLock l(lock_);
  const int64_t new_i = i_ - 1;
  i_ = new_i;
  return new_i;
}

int64_t NoBarrierAtomicInt64::operator+=(int64_t other) {
  AutoLock l(lock_);
  const int64_t new_i = i_ + other;
  i_ = new_i;
  return new_i;
}

int64_t NoBarrierAtomicInt64::operator-=(int64_t other) {
  AutoLock l(lock_);
  const int64_t new_i = i_ - other;
  i_ = new_i;
  return new_i;
}

int64_t NoBarrierAtomicInt64::operator*() const {
  AutoLock l(lock_);
  return i_;
}

#endif  // ifdef ARCH_CPU_64_BITS

class MessageCounter {
 public:
  MessageCounter(const char* name, const char* unit);
  explicit MessageCounter(const char* name);

  // Copy or assign the start_time_ of a MessageCounter is senseless.
  MessageCounter(const MessageCounter&) = delete;
  MessageCounter& operator=(const MessageCounter&) = delete;

  int message_count() const { return *count_; }
  int64_t message_size() const { return *size_; }
  int last_message_size() const { return last_size_; }
  double DurationSeconds() const;
  double MessagesPerSecond() const;
  double SizePerSecond() const;
  double AverageMessageSize() const;
  void LogMessage(const ::google::protobuf::MessageLite& message);
  void LogMessage(int size);
  virtual void DisplayStatistics(std::ostream& os);

 private:
  const std::string name_;
  const std::string unit_;
  NoBarrierAtomicInt32 count_;
  NoBarrierAtomicInt64 size_;
  int last_size_ = 0;
  base::Time start_time_;
};

MessageCounter::MessageCounter(const char* name, const char* unit)
    : name_(name),
      unit_(unit),
      count_(),
      size_(),
      start_time_(base::Time::Now()) {}

MessageCounter::MessageCounter(const char* name)
    : MessageCounter(name, "bytes") {}

double MessageCounter::DurationSeconds() const {
  return (base::Time::Now() - start_time_).InSecondsF();
}

double MessageCounter::MessagesPerSecond() const {
  return static_cast<double>(message_count()) / DurationSeconds();
}
double MessageCounter::SizePerSecond() const {
  return static_cast<double>(message_size()) / DurationSeconds();
}

double MessageCounter::AverageMessageSize() const {
  return static_cast<double>(message_size()) / message_count();
}

void MessageCounter::LogMessage(
    const ::google::protobuf::MessageLite& message) {
  LogMessage(message.ByteSize());
}

void MessageCounter::LogMessage(int size) {
  count_++;
  last_size_ = size;
  size_ += size;
}

void MessageCounter::DisplayStatistics(std::ostream& os) {
  os << name_ << ": " << message_size() << " " << unit_ << " in "
     << message_count() << " packages, last package " << last_message_size()
     << " " << unit_ << ", " << AverageMessageSize() << " " << unit_
     << "/package, " << MessagesPerSecond() << " packages/sec, "
     << SizePerSecond() << " " << unit_ << "/sec" << std::endl;
}

}  // namespace

// Analyzes messages from DeliverHostMessage function.
class FakeConnectionEventLogger::CounterClientStub
    : public protocol::ClientStub,
      public MessageCounter {
 public:
  CounterClientStub();

 private:
  void DeliverHostMessage(const protocol::ExtensionMessage& message) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override {}
  void SetCapabilities(const protocol::Capabilities& capabilities) override {}
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override {}
  void SetKeyboardLayout(const protocol::KeyboardLayout& layout) override {}
  void SetPairingResponse(const protocol::PairingResponse& response) override {}
  void SetVideoLayout(const protocol::VideoLayout& video_layout) override {}
  void SetTransportInfo(
      const protocol::TransportInfo& transport_info) override {}
};

FakeConnectionEventLogger::CounterClientStub::CounterClientStub()
    : MessageCounter("client") {}

void FakeConnectionEventLogger::CounterClientStub::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  LogMessage(message);
}

// Analyzes messages from DeliverClientMessage function.
class FakeConnectionEventLogger::CounterHostStub : public protocol::HostStub,
                                                   public MessageCounter {
 public:
  CounterHostStub();

 private:
  void ControlAudio(const protocol::AudioControl& audio_control) override {}
  void ControlVideo(const protocol::VideoControl& video_control) override {}
  void ControlPeerConnection(
      const protocol::PeerConnectionParameters& parameters) override {}
  void DeliverClientMessage(const protocol::ExtensionMessage& message) override;
  void NotifyClientResolution(
      const protocol::ClientResolution& resolution) override {}
  void RequestPairing(
      const protocol::PairingRequest& pairing_request) override {}
  void SetCapabilities(const protocol::Capabilities& capabilities) override {}
  void SelectDesktopDisplay(
      const protocol::SelectDesktopDisplayRequest& select_display) override {}
  void SetVideoLayout(const protocol::VideoLayout& video_layout) override {}
};

FakeConnectionEventLogger::CounterHostStub::CounterHostStub()
    : MessageCounter("host") {}

void FakeConnectionEventLogger::CounterHostStub::DeliverClientMessage(
    const protocol::ExtensionMessage& message) {
  LogMessage(message);
}

// Analyzes messages from ProcessAudioPacket function.
class FakeConnectionEventLogger::CounterAudioStub : public protocol::AudioStub,
                                                    public MessageCounter {
 public:
  CounterAudioStub();

 private:
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> audio_packet,
                          base::OnceClosure done) override;
};

FakeConnectionEventLogger::CounterAudioStub::CounterAudioStub()
    : MessageCounter("audio") {}

void FakeConnectionEventLogger::CounterAudioStub::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> audio_packet,
    base::OnceClosure done) {
  if (audio_packet) {
    LogMessage(*audio_packet);
  }
  std::move(done).Run();
}

// Analyzes messages from ProcessVideoPacket function.
class FakeConnectionEventLogger::CounterVideoStub : public protocol::VideoStub,
                                                    public MessageCounter {
 public:
  CounterVideoStub(protocol::FakeConnectionToClient* connection);

  void DisplayStatistics(std::ostream& os) override;

 private:
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          base::OnceClosure done) override;

  raw_ptr<protocol::FakeConnectionToClient> connection_ = nullptr;
  MessageCounter video_data_;
  MessageCounter capture_time_;
  MessageCounter encode_time_;
};

FakeConnectionEventLogger::CounterVideoStub::CounterVideoStub(
    protocol::FakeConnectionToClient* connection)
    : MessageCounter("video"),
      connection_(connection),
      video_data_("video-data"),
      capture_time_("capture-time", "ms"),
      encode_time_("encode-time", "ms") {}

void FakeConnectionEventLogger::CounterVideoStub::DisplayStatistics(
    std::ostream& os) {
  MessageCounter::DisplayStatistics(os);
  video_data_.DisplayStatistics(os);
  capture_time_.DisplayStatistics(os);
  encode_time_.DisplayStatistics(os);
}

void FakeConnectionEventLogger::CounterVideoStub::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> video_packet,
    base::OnceClosure done) {
  if (video_packet && video_packet->has_capture_overhead_time_ms()) {
    // Not a keepalive packet.
    if (connection_ && connection_->video_feedback_stub()) {
      std::unique_ptr<VideoAck> ack(new VideoAck());
      ack->set_frame_id(video_packet->frame_id());
      connection_->video_feedback_stub()->ProcessVideoAck(std::move(ack));
    }
    LogMessage(*video_packet);
    video_data_.LogMessage(video_packet->data().size());
    capture_time_.LogMessage(video_packet->capture_time_ms());
    encode_time_.LogMessage(video_packet->encode_time_ms());
  }
  std::move(done).Run();
}

FakeConnectionEventLogger::FakeConnectionEventLogger(
    protocol::FakeConnectionToClient* connection)
    : client_stub_(new CounterClientStub()),
      host_stub_(new CounterHostStub()),
      audio_stub_(new CounterAudioStub()),
      video_stub_(new CounterVideoStub(connection)) {}

FakeConnectionEventLogger::~FakeConnectionEventLogger() {}

protocol::ClientStub* FakeConnectionEventLogger::client_stub() {
  return client_stub_.get();
}

protocol::HostStub* FakeConnectionEventLogger::host_stub() {
  return host_stub_.get();
}

protocol::AudioStub* FakeConnectionEventLogger::audio_stub() {
  return audio_stub_.get();
}

protocol::VideoStub* FakeConnectionEventLogger::video_stub() {
  return video_stub_.get();
}

std::ostream& operator<<(std::ostream& os,
                         const FakeConnectionEventLogger& logger) {
  logger.audio_stub_->DisplayStatistics(os);
  logger.video_stub_->DisplayStatistics(os);
  logger.client_stub_->DisplayStatistics(os);
  logger.host_stub_->DisplayStatistics(os);
  return os;
}

}  // namespace test
}  // namespace remoting
