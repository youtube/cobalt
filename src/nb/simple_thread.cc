/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "nb/simple_thread.h"

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace nb {

SimpleThread::SimpleThread(const std::string& name)
    : thread_(kSbThreadInvalid), name_(name),
      join_called_(false) {}

SimpleThread::~SimpleThread() {
  SB_DCHECK(join_called_.load()) << "Join not called on thread.";
}

void SimpleThread::Start() {
  SbThreadEntryPoint entry_point = ThreadEntryPoint;

  thread_ = SbThreadCreate(0,                    // default stack_size.
                           kSbThreadNoPriority,  // default priority.
                           kSbThreadNoAffinity,  // default affinity.
                           true,                 // joinable.
                           name_.c_str(), entry_point, this);

  // SbThreadCreate() above produced an invalid thread handle.
  SB_DCHECK(thread_ != kSbThreadInvalid);
  return;
}

void SimpleThread::Sleep(SbTime microseconds) {
  SbThreadSleep(microseconds);
}

void SimpleThread::SleepMilliseconds(int value) {
  return Sleep(value * kSbTimeMillisecond);
}

void* SimpleThread::ThreadEntryPoint(void* context) {
  SimpleThread* this_ptr = static_cast<SimpleThread*>(context);
  this_ptr->Run();
  return NULL;
}

void SimpleThread::Join() {
  SB_DCHECK(join_called_.load() == false);
  if (!SbThreadJoin(thread_, NULL)) {
    SB_DCHECK(false) << "Could not join thread.";
  }
  join_called_.store(true);
}

}  // namespace nb
