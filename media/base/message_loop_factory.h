// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MESSAGE_LOOP_FACTORY_H_
#define MEDIA_BASE_MESSAGE_LOOP_FACTORY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "media/base/media_export.h"

class MessageLoop;

namespace media {

// Factory object that manages named MessageLoops.
class MEDIA_EXPORT MessageLoopFactory {
 public:
  // Get the message loop associated with |name|. A new MessageLoop
  // is created if the factory doesn't have one associated with |name|.
  // NULL is returned if |name| is an empty string, or a new
  // MessageLoop needs to be created and a failure occurs during the
  // creation process.
  virtual MessageLoop* GetMessageLoop(const std::string& name) = 0;

  // Get the message loop proxy associated with |name|. A new MessageLoopProxy
  // is created if the factory doesn't have one associated with |name|.
  // NULL is returned if |name| is an empty string, or a new
  // MessageLoop needs to be created and a failure occurs during the
  // creation process.
  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoopProxy(
      const std::string& name) = 0;

 protected:
  // Only allow scoped_ptr<> to delete factory.
  friend class scoped_ptr<MessageLoopFactory>;
  virtual ~MessageLoopFactory();
};

}  // namespace media

#endif  // MEDIA_BASE_MESSAGE_LOOP_FACTORY_H_
