// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/message_loop_factory.h"

#include "base/threading/thread.h"

namespace media {

MessageLoopFactory::MessageLoopFactory() {}

MessageLoopFactory::~MessageLoopFactory() {
  for (ThreadMap::iterator iter = thread_map_.begin();
       iter != thread_map_.end();
       ++iter) {
    base::Thread* thread = (*iter).second;

    if (thread) {
      thread->Stop();
      delete thread;
    }
  }
  thread_map_.clear();
}

MessageLoop* MessageLoopFactory::GetMessageLoop(const std::string& name) {
  return GetThread(name)->message_loop();
}

scoped_refptr<base::MessageLoopProxy>
MessageLoopFactory::GetMessageLoopProxy(const std::string& name) {
  return GetThread(name)->message_loop_proxy();
}

base::Thread* MessageLoopFactory::GetThread(const std::string& name) {
  DCHECK(!name.empty());

  base::AutoLock auto_lock(lock_);
  ThreadMap::iterator it = thread_map_.find(name);
  if (it != thread_map_.end())
    return (*it).second;

  base::Thread* thread = new base::Thread(name.c_str());
  CHECK(thread->Start()) << "Failed to start thread: " << name;
  thread_map_[name] = thread;
  return thread;
}

}  // namespace media
