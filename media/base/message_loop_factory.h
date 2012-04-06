// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MESSAGE_LOOP_FACTORY_H_
#define MEDIA_BASE_MESSAGE_LOOP_FACTORY_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

class MessageLoop;

namespace base {
class Thread;
}

namespace media {

// Factory object that manages named MessageLoops.
//
// TODO(scherkus): replace this with something simpler http://crbug.com/116873
class MEDIA_EXPORT MessageLoopFactory {
 public:
  MessageLoopFactory();

  // Get the message loop associated with |name|. A new MessageLoop
  // is created if the factory doesn't have one associated with |name|.
  //
  // |name| must not be an empty string.
  MessageLoop* GetMessageLoop(const std::string& name);

  // Get the message loop proxy associated with |name|. A new MessageLoopProxy
  // is created if the factory doesn't have one associated with |name|.
  //
  // |name| must not be an empty string.
  scoped_refptr<base::MessageLoopProxy> GetMessageLoopProxy(
      const std::string& name);

 private:
  // Only allow scoped_ptr<> to delete factory.
  friend class scoped_ptr<MessageLoopFactory>;
  ~MessageLoopFactory();

  // Returns the thread associated with |name| creating a new thread if needed.
  base::Thread* GetThread(const std::string& name);

  // Lock used to serialize access for the following data members.
  base::Lock lock_;

  // List of pairs of created threads and their names.  We use a list to ensure
  // threads are stopped & deleted in reverse order of creation.
  typedef std::list<std::pair<std::string, base::Thread*> > ThreadList;
  ThreadList threads_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_MESSAGE_LOOP_FACTORY_H_
