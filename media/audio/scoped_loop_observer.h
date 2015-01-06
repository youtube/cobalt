// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SCOPED_LOOP_OBSERVER_H_
#define MEDIA_AUDIO_SCOPED_LOOP_OBSERVER_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"

namespace base {
class WaitableEvent;
}

namespace media {

// A common base class for AudioOutputDevice and AudioInputDevice that manages
// a message loop pointer and monitors it for destruction. If the object goes
// out of scope before the message loop, the object will automatically remove
// itself from the message loop's list of destruction observers.
// NOTE: The class that inherits from this class must implement the
// WillDestroyCurrentMessageLoop virtual method from DestructionObserver.
class ScopedLoopObserver
    : public MessageLoop::DestructionObserver {
 public:
  explicit ScopedLoopObserver(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);

 protected:
  virtual ~ScopedLoopObserver();

  // Accessor to the loop that's used by the derived class.
  const scoped_refptr<base::MessageLoopProxy>& message_loop() { return loop_; }

 private:
  // Call to add or remove ourselves from the list of destruction observers for
  // the message loop.
  void ObserveLoopDestruction(bool enable, base::WaitableEvent* done);

  // A pointer to the message loop's proxy. In case the loop gets destroyed
  // before this object goes out of scope, PostTask etc will fail but not crash.
  scoped_refptr<base::MessageLoopProxy> loop_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLoopObserver);
};

}  // namespace media.

#endif  // MEDIA_AUDIO_SCOPED_LOOP_OBSERVER_H_
