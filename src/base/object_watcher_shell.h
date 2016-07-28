/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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
#ifndef BASE_OBJECT_WATCHER_SHELL_H_
#define BASE_OBJECT_WATCHER_SHELL_H_

#if defined(__LB_SHELL__)

#include <sys/poll.h>

#include <map>
#include <vector>

#include "base/base_export.h"
#include "base/message_loop.h"
#include "base/message_pump_shell.h"
#include "base/threading/simple_thread.h"

// overall interface inspired by base/win/object_watcher.h
// however internal implementation is driven by platform necessity
namespace base {

// A class that provides a means to asynchronously wait for a file descriptor to
// become signaled.  It uses an internal thread on a singleton object that
// blocks on poll() calls to an internal list of fds, each represented by an
// instance of the ObjectWatcher class.
// It provides a notification callback, OnObjectSignaled, that runs back on
// the origin thread (i.e., the thread that called StartWatching).
//
//
// Typical usage:
//
//   class MyClass : public base::ObjectWatcher::Delegate {
//    public:
//     void DoStuffWhenSignaled(int object) {
//       watcher_.StartWatching(object, this);
//     }
//     virtual void OnObjectSignaled(int object) {
//       // OK, time to do stuff!
//     }
//    private:
//     base::ObjectWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when object becomes
// signaled.  ObjectWatcher makes this task easy.  When MyClass goes out of
// scope, the watcher_ will be destroyed, and there is no need to worry about
// OnObjectSignaled being called on a deleted MyClass pointer.  Easy!
//

struct Watch;

// -----------------------------------------------------------------------------
// ObjectWatchMultiplexer
// this object runs an internal thread to block on the aggregate of all watched
// objects and poll them for activity.  the best emulation of callback-driven
// io I can muster on the PS3.
class ObjectWatchMultiplexer : public SimpleThread {
 public:
  ObjectWatchMultiplexer();
  ~ObjectWatchMultiplexer();

  // blocking call to exit the internal thread
  void Join();
  // although you can add different watches with the same file descriptor all
  // day long, please don't add the same watch twice it will create an error.
  void AddWatch(Watch* watch);
  void RemoveWatch(Watch* watch);
  static ObjectWatchMultiplexer* GetInstance() {return s_instance;}

 private:
  // the internal thread runloop
  void Run();
  void RecomposePollFDArray();

  // used to let the thread block until it actually has _something_ to poll
  base::WaitableEvent non_empty_list_event_;

#if defined(__LB_WIIU__)
  // 33ms is a good balance between quickly servicing new
  // sockets to poll, and not overloading the IO pipeline.
  static const int kPollTimeout = 33;
#else
  static const int kPollTimeout = 100;
#endif

  // this lock protects watch_map_, should_recompose_pollfd_array_, and
  // max_watch_handle_ which is used to generate unique watch handles to
  // disambiguate watches on the same file descriptor
  base::Lock add_watch_list_lock_;
  base::Lock remove_watch_list_lock_;
  typedef std::multimap<int, Watch*> WatchMap;
  WatchMap watch_map_;      // current working watches
  WatchMap new_watch_map_;  // new watches
  int max_watch_handle_;
  std::vector<pollfd> pollfd_array_;
  bool should_recompose_pollfd_array_;
  bool exit_;
  static ObjectWatchMultiplexer* s_instance;
  DISALLOW_COPY_AND_ASSIGN(ObjectWatchMultiplexer);
};

class BASE_EXPORT ObjectWatcher : public MessageLoop::DestructionObserver {
 public:
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnObjectSignaled(int object) = 0;
  };

  ObjectWatcher();
  ~ObjectWatcher();

  // When the object is signaled with mode, the given delegate is notified on
  // the thread where StartWatching is called.  The ObjectWatcher is not
  // responsible for deleting the delegate.
  //
  // Returns true if the watch was started.  Otherwise, false is returned.
  //
  bool StartWatching(int object,
                     MessagePumpShell::Mode mode,
                     Delegate* delegate);

  // Stops watching.  Does nothing if the watch has already completed.  If the
  // watch is still active, then it is canceled, and the associated delegate is
  // not notified.
  //
  // Returns true if the watch was canceled.  Otherwise, false is returned.
  //
  bool StopWatching();

  // Returns the fd of the object being watched, or -1 if the object
  // watcher is stopped.
  int GetWatchedObject();

 private:
  // MessageLoop::DestructionObserver implementation:
  virtual void WillDestroyCurrentMessageLoop();

  struct Watch* watch_;

  DISALLOW_COPY_AND_ASSIGN(ObjectWatcher);
};

}  // namespace base

#endif  // defined(__LB_SHELL__)
#endif  // BASE_OBJECT_WATCHER_SHELL_H_
