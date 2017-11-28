// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_thread.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "media/audio/audio_util.h"
#include "media/base/audio_bus.h"

using base::PlatformThread;

namespace media {

// The actual worker thread implementation.  It's very bare bones and much
// simpler than SimpleThread (no synchronization in Start, etc) and supports
// joining the thread handle asynchronously via a provided message loop even
// after the Thread object itself has been deleted.
class AudioDeviceThread::Thread
    : public PlatformThread::Delegate,
      public base::RefCountedThreadSafe<AudioDeviceThread::Thread> {
 public:
  Thread(AudioDeviceThread::Callback* callback,
         base::SyncSocket::Handle socket,
         const char* thread_name);

  void Start();

  // Stops the thread.  If |loop_for_join| is non-NULL, the function posts
  // a task to join (close) the thread handle later instead of waiting for
  // the thread.  If loop_for_join is NULL, then the function waits
  // synchronously for the thread to terminate.
  void Stop(MessageLoop* loop_for_join);

 private:
  friend class base::RefCountedThreadSafe<AudioDeviceThread::Thread>;
  virtual ~Thread();

  // Overrides from PlatformThread::Delegate.
  virtual void ThreadMain() override;

  // Runs the loop that reads from the socket.
  void Run();

 private:
  base::PlatformThreadHandle thread_;
  AudioDeviceThread::Callback* callback_;
  base::CancelableSyncSocket socket_;
  base::Lock callback_lock_;
  const char* thread_name_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

// AudioDeviceThread implementation

AudioDeviceThread::AudioDeviceThread() {
}

AudioDeviceThread::~AudioDeviceThread() {
  DCHECK(!thread_);
}

void AudioDeviceThread::Start(AudioDeviceThread::Callback* callback,
                              base::SyncSocket::Handle socket,
                              const char* thread_name) {
  base::AutoLock auto_lock(thread_lock_);
  CHECK(thread_ == NULL);
  thread_ = new AudioDeviceThread::Thread(callback, socket, thread_name);
  thread_->Start();
}

void AudioDeviceThread::Stop(MessageLoop* loop_for_join) {
  base::AutoLock auto_lock(thread_lock_);
  if (thread_) {
    thread_->Stop(loop_for_join);
    thread_ = NULL;
  }
}

bool AudioDeviceThread::IsStopped() {
  base::AutoLock auto_lock(thread_lock_);
  return thread_ == NULL;
}

// AudioDeviceThread::Thread implementation
AudioDeviceThread::Thread::Thread(AudioDeviceThread::Callback* callback,
                                  base::SyncSocket::Handle socket,
                                  const char* thread_name)
    : thread_(base::kNullThreadHandle),
      callback_(callback),
      socket_(socket),
      thread_name_(thread_name) {
}

AudioDeviceThread::Thread::~Thread() {
  DCHECK_EQ(thread_, base::kNullThreadHandle) << "Stop wasn't called";
}

void AudioDeviceThread::Thread::Start() {
  base::AutoLock auto_lock(callback_lock_);
  DCHECK_EQ(thread_, base::kNullThreadHandle);
  // This reference will be released when the thread exists.
  AddRef();

  PlatformThread::CreateWithPriority(0, this, &thread_,
                                     base::kThreadPriority_RealtimeAudio);
  CHECK(thread_ != base::kNullThreadHandle);
}

void AudioDeviceThread::Thread::Stop(MessageLoop* loop_for_join) {
  socket_.Shutdown();

  base::PlatformThreadHandle thread = base::kNullThreadHandle;

  {  // NOLINT
    base::AutoLock auto_lock(callback_lock_);
    callback_ = NULL;
    std::swap(thread, thread_);
  }

  if (thread != base::kNullThreadHandle) {
    if (loop_for_join) {
      loop_for_join->PostTask(FROM_HERE,
          base::Bind(&base::PlatformThread::Join, thread));
    } else {
      base::PlatformThread::Join(thread);
    }
  }
}

void AudioDeviceThread::Thread::ThreadMain() {
  PlatformThread::SetName(thread_name_);

  // Singleton access is safe from this thread as long as callback is non-NULL.
  // The callback is the only point where the thread calls out to 'unknown' code
  // that might touch singletons and the lifetime of the callback is controlled
  // by another thread on which singleton access is OK as well.
  base::ThreadRestrictions::SetSingletonAllowed(true);

  {  // NOLINT
    base::AutoLock auto_lock(callback_lock_);
    if (callback_)
      callback_->InitializeOnAudioThread();
  }

  Run();

  // Release the reference for the thread. Note that after this, the Thread
  // instance will most likely be deleted.
  Release();
}

void AudioDeviceThread::Thread::Run() {
  while (true) {
    int pending_data = 0;
    size_t bytes_read = socket_.Receive(&pending_data, sizeof(pending_data));
    if (bytes_read != sizeof(pending_data)) {
      DCHECK_EQ(bytes_read, 0U);
      break;
    }

    base::AutoLock auto_lock(callback_lock_);
    if (callback_)
      callback_->Process(pending_data);
  }
}

// AudioDeviceThread::Callback implementation

AudioDeviceThread::Callback::Callback(
    const AudioParameters& audio_parameters,
    int input_channels,
    base::SharedMemoryHandle memory, int memory_length)
    : audio_parameters_(audio_parameters),
      input_channels_(input_channels),
      samples_per_ms_(audio_parameters.sample_rate() / 1000),
      bytes_per_ms_(audio_parameters.channels() *
                    (audio_parameters_.bits_per_sample() / 8) *
                    samples_per_ms_),
      shared_memory_(memory, false),
      memory_length_(memory_length) {
  CHECK_NE(bytes_per_ms_, 0);  // Catch division by zero early.
  CHECK_NE(samples_per_ms_, 0);
}

AudioDeviceThread::Callback::~Callback() {}

void AudioDeviceThread::Callback::InitializeOnAudioThread() {
  MapSharedMemory();
  DCHECK(shared_memory_.memory() != NULL);
}

}  // namespace media.
