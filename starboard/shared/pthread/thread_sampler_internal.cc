// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/pthread/thread_sampler_internal.h"

#include <semaphore.h>
#include <signal.h>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/pthread/thread_context_internal.h"
#include "starboard/shared/pthread/types_internal.h"
#include "starboard/thread.h"

namespace {

class SignalHandler {
 public:
  static void AddSampler() {
    starboard::ScopedLock lock(GetMutex());
    if (++sampler_count_ == 1) {
      Install();
    }
  }

  static void RemoveSampler() {
    starboard::ScopedLock lock(GetMutex());
    if (--sampler_count_ == 0) {
      Restore();
    }
  }

  static SbThreadContext Freeze(SbThreadSampler sampler);
  static bool Thaw(SbThreadSampler sampler);

 private:
  // Getter with static local to lazily initialize the mutex once.
  static const starboard::Mutex& GetMutex() {
    static starboard::Mutex mutex;
    return mutex;
  }

  static void Install() {
    sem_init(&freeze_semaphore_, 0, 0);
    sem_init(&thaw_semaphore_, 0, 0);
    struct sigaction sa;
    sa.sa_sigaction = &HandleProfilerSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    signal_handler_installed_ =
        (sigaction(SIGPROF, &sa, &old_signal_handler_) == 0);
  }

  static void Restore() {
    if (signal_handler_installed_) {
      sigaction(SIGPROF, &old_signal_handler_, 0);
      signal_handler_installed_ = false;
    }
    sem_destroy(&freeze_semaphore_);
    sem_destroy(&thaw_semaphore_);
  }

  static void HandleProfilerSignal(int signal, siginfo_t* info, void* context);

  static int sampler_count_;
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
  static SbThreadContextPrivate sb_context_;
  static SbThreadSampler frozen_sampler_;
  // POSIX semaphore can be used in signal handler. Other synchronization,
  // including mutex, condition variable and Starboard's semaphore can't.
  static sem_t freeze_semaphore_;
  static sem_t thaw_semaphore_;
};

int SignalHandler::sampler_count_ = 0;
bool SignalHandler::signal_handler_installed_ = false;
struct sigaction SignalHandler::old_signal_handler_;
SbThreadContextPrivate SignalHandler::sb_context_;
SbThreadSampler SignalHandler::frozen_sampler_ = kSbThreadSamplerInvalid;
sem_t SignalHandler::freeze_semaphore_;
sem_t SignalHandler::thaw_semaphore_;

SbThreadContext SignalHandler::Freeze(SbThreadSampler sampler) {
  starboard::ScopedLock lock(GetMutex());
  SB_DCHECK(frozen_sampler_ != sampler) << "SbThreadSampler frozen twice.";
  if (!signal_handler_installed_ || SbThreadSamplerIsValid(frozen_sampler_)) {
    return kSbThreadContextInvalid;
  }
  frozen_sampler_ = sampler;
  pthread_kill(SB_PTHREAD_INTERNAL_THREAD(sampler->thread()), SIGPROF);
  sem_wait(&freeze_semaphore_);
  return &sb_context_;
}

bool SignalHandler::Thaw(SbThreadSampler sampler) {
  starboard::ScopedLock lock(GetMutex());
  SB_DCHECK(frozen_sampler_ == sampler) << "SbThreadSampler didn't freeze.";
  if (frozen_sampler_ != sampler) return false;
  sb_context_ = SbThreadContextPrivate();
  sem_post(&thaw_semaphore_);
  frozen_sampler_ = kSbThreadSamplerInvalid;
  return true;
}

void SignalHandler::HandleProfilerSignal(int signal, siginfo_t* info,
                                         void* context) {
  if (signal != SIGPROF) return;
  sb_context_ = SbThreadContextPrivate(reinterpret_cast<ucontext_t*>(context));
  // |Freeze| can return the context now.
  sem_post(&freeze_semaphore_);
  // Keep this thread frozen until |Thaw| is called.
  sem_wait(&thaw_semaphore_);
}

}  // namespace

SbThreadSamplerPrivate::SbThreadSamplerPrivate(SbThread thread)
    : thread_(thread) {
  SignalHandler::AddSampler();
}

SbThreadSamplerPrivate::~SbThreadSamplerPrivate() {
  SignalHandler::RemoveSampler();
}

SbThreadContext SbThreadSamplerPrivate::Freeze() {
  return SignalHandler::Freeze(this);
}

bool SbThreadSamplerPrivate::Thaw() { return SignalHandler::Thaw(this); }
