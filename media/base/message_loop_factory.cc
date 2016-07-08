// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/message_loop_factory.h"

#include "base/threading/thread.h"
#if !defined(OS_STARBOARD)
#include "lb_platform.h"
#include "lb_shell/lb_shell_constants.h"
#endif

namespace media {

MessageLoopFactory::MessageLoopFactory() {}

MessageLoopFactory::~MessageLoopFactory() {
  for (ThreadList::reverse_iterator it = threads_.rbegin();
       it != threads_.rend(); ++it) {
    base::Thread* thread = it->second;
    thread->Stop();
    delete thread;
  }
  threads_.clear();
}

scoped_refptr<base::MessageLoopProxy> MessageLoopFactory::GetMessageLoop(
    Type type) {
  return GetThread(type)->message_loop_proxy();
}

base::Thread* MessageLoopFactory::GetThread(Type type) {
  base::AutoLock auto_lock(lock_);
  for (ThreadList::iterator it = threads_.begin(); it != threads_.end(); ++it) {
    if (it->first == type)
      return it->second;
  }

  const char* name = NULL;
  switch (type) {
    case kPipeline:
      name = "MediaPipeline";
      break;
  }

  base::Thread* thread = new base::Thread(name);
  base::Thread::Options options;
// TODO: Determine where to define thread constants in a Starboard
// world.
#if !defined(OS_STARBOARD)
  if (type == kPipeline) {
#if defined(__LB_WIIU__)
    options.affinity = LB::Platform::kMediaPipelineThreadAffinity;
#elif defined(__LB_PS4__)
    options.stack_size = kMediaStackThreadStackSize;
#endif
  }
#endif
  CHECK(thread->StartWithOptions(options))
      << "Failed to start thread: " << name;
  threads_.push_back(std::make_pair(type, thread));
  return thread;
}

}  // namespace media
