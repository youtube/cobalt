// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_buffer_wait_coordinator.h"

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"

namespace media {

// FrameAvailableEvent is a RefCounted wrapper for a WaitableEvent
// (it's not possible to put one in RefCountedData).
// This let's us safely signal an event on any thread.
struct FrameAvailableEvent
    : public base::RefCountedThreadSafe<FrameAvailableEvent> {
  FrameAvailableEvent()
      : event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  void Signal() { event.Signal(); }
  base::WaitableEvent event;

 private:
  friend class RefCountedThreadSafe<FrameAvailableEvent>;
  ~FrameAvailableEvent() = default;
};

CodecBufferWaitCoordinator::CodecBufferWaitCoordinator(
    scoped_refptr<gpu::TextureOwner> texture_owner,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)),
      texture_owner_(std::move(texture_owner)),
      frame_available_event_(new FrameAvailableEvent()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(texture_owner_);
  texture_owner_->SetFrameAvailableCallback(base::BindRepeating(
      &FrameAvailableEvent::Signal, frame_available_event_));
}

CodecBufferWaitCoordinator::~CodecBufferWaitCoordinator() {
  DCHECK(texture_owner_);
}

void CodecBufferWaitCoordinator::SetReleaseTimeToNow() {
  AssertAcquiredDrDcLock();
  release_time_ = base::TimeTicks::Now();
}

bool CodecBufferWaitCoordinator::IsExpectingFrameAvailable() {
  AssertAcquiredDrDcLock();
  return !release_time_.is_null();
}

void CodecBufferWaitCoordinator::WaitForFrameAvailable() {
  AssertAcquiredDrDcLock();
  DCHECK(!release_time_.is_null());

  const base::TimeTicks call_time = base::TimeTicks::Now();
  const base::TimeDelta elapsed = call_time - release_time_;
  const base::TimeDelta remaining = max_wait_.value() - elapsed;
  release_time_ = base::TimeTicks();

  if (remaining <= base::TimeDelta()) {
    if (!frame_available_event_->event.IsSignaled()) {
      DVLOG(1) << "Deferred WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF() << "ms";
    }
  } else {
    DCHECK_LE(remaining, max_wait_.value());
    if (!frame_available_event_->event.TimedWait(remaining)) {
      DVLOG(1) << "WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF()
               << "ms, additionally waited: " << remaining.InMillisecondsF()
               << "ms, total: " << (elapsed + remaining).InMillisecondsF()
               << "ms";
    }
  }
}

}  // namespace media
