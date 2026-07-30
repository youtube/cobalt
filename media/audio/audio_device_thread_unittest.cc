// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)

#include <memory>
#include <thread>

#include "base/memory/raw_ptr.h"
#include "base/sync_socket.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "media/audio/audio_device_thread.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr base::TimeDelta kWaitTimeout = base::Seconds(1);

class FakeCallback : public AudioDeviceThread::Callback {
 public:
  explicit FakeCallback(base::WaitableEvent* process_called_event)
      : Callback(AudioParameters(),
                 /*segment_length=*/1,
                 /*total_segments=*/1),
        process_called_event_(process_called_event) {}

  void MapSharedMemory() override {}
  bool WillConfirmReadsViaShmem() const override { return true; }

  void Process(uint32_t pending_data) override {
    process_called_event_->Signal();
  }
  void OnSocketError() override {}

 private:
  raw_ptr<base::WaitableEvent> process_called_event_;
};

}  // namespace

// Verifies that destroying an AudioDeviceThread while its thread is blocked
// in Receive() completes in bounded time. This is a regression test for
// crbug.com/361250560 where shutdown(SHUT_RDWR) failed to unblock read() on
// Apple platforms.
TEST(AudioDeviceThreadTest, DestructionUnblocksReceive) {
  base::CancelableSyncSocket audio_thread_socket;
  base::SyncSocket test_socket;
  ASSERT_TRUE(base::SyncSocket::CreatePair(&audio_thread_socket, &test_socket));

  base::WaitableEvent process_called_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  FakeCallback callback(&process_called_event);
  auto thread = std::make_unique<AudioDeviceThread>(
      &callback, base::SyncSocket::ScopedHandle(audio_thread_socket.Release()),
      "TestAudioThread", base::ThreadType::kRealtimeAudio);

  uint32_t pending_data = 1;
  ASSERT_EQ(sizeof(pending_data),
            test_socket.Send(base::byte_span_from_ref(pending_data)));
  ASSERT_TRUE(process_called_event.TimedWait(kWaitTimeout));

  // The thread has completed one read and is now blocked in the next Receive().
  base::WaitableEvent destruction_done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::thread destroy_thread([&thread, &destruction_done_event]() {
    thread.reset();
    destruction_done_event.Signal();
  });

  const bool unblocked = destruction_done_event.TimedWait(kWaitTimeout);
  EXPECT_TRUE(unblocked);
  if (!unblocked) {
    test_socket.Close();
  }

  destroy_thread.join();
}

}  // namespace media

#endif  // BUILDFLAG(IS_IOS)
