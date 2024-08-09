// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_THREAD_IMPL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_THREAD_IMPL_H_

#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "third_party/chromium/media/audio/audio_thread.h"
#include "third_party/chromium/media/audio/audio_thread_hang_monitor.h"

namespace media_m96 {

class MEDIA_EXPORT AudioThreadImpl final : public AudioThread {
 public:
  AudioThreadImpl();

  AudioThreadImpl(const AudioThreadImpl&) = delete;
  AudioThreadImpl& operator=(const AudioThreadImpl&) = delete;

  ~AudioThreadImpl() final;

  // AudioThread implementation.
  void Stop() final;
  bool IsHung() const final;
  base::SingleThreadTaskRunner* GetTaskRunner() final;
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() final;

 private:
  base::Thread thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  // Null on Mac OS, initialized in the constructor on other platforms.
  AudioThreadHangMonitor::Ptr hang_monitor_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_THREAD_IMPL_H_
