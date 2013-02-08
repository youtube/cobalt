/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef BASE_MESSAGE_PUMP_SHELL_H_
#define BASE_MESSAGE_PUMP_SHELL_H_
#pragma once

#include "base/message_pump.h"
#include "base/time.h"
#include "base/synchronization/waitable_event.h"

namespace base {

class BASE_EXPORT MessagePumpShell : public MessagePump {
 public:

  MessagePumpShell();

  class Dispatcher {
  };

  class Observer {
  };

  class IOObserver {
  };

  class Watcher {
  public:
    Watcher();
    Watcher(Watcher &);
    // __LB_SHELL__WRITE_ME__

    virtual void OnFileCanReadWithoutBlocking(int fd);
    virtual void OnFileCanWriteWithoutBlocking(int fd);
  };

  // this one only watches sockets, not all file descriptors
  class FileDescriptorWatcher {
  public:
    FileDescriptorWatcher();
    FileDescriptorWatcher(FileDescriptorWatcher &);
    // __LB_SHELL__WRITE_ME__
    bool StopWatchingFileDescriptor();
  };

    // borrowed from message_pump_libevent.h
  enum Mode {
    WATCH_READ = 1 << 0,
    WATCH_WRITE = 1 << 1,
    WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
  };

  // ------------------------------------------- UI

  // cribbed from message_pump.h
  virtual bool DoWork();
  virtual bool DoDelayedWork(TimeTicks *next_delayed_work_time);
  virtual bool DoIdleWork();

  virtual void Run(Delegate * delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

  // cribbed from message_pump_glib.h
  virtual void RunWithDispatcher(Delegate* delegate, Dispatcher* dispatcher);

  // Add an Observer, which will start receiving notifications immediately.
  void AddObserver(Observer* observer);

  // Remove an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(Observer* observer);

  // ------------------------------------------- IO
  void AddIOObserver(IOObserver* obs);
  void RemoveIOObserver(IOObserver* obs);

  bool WatchSocket(int s,
                   bool persistent,
                   int mode,
                   FileDescriptorWatcher *controller,
                   Watcher *del);

 private:
  // set to false when run should return
  bool keep_running_;

  WaitableEvent event_;

  TimeTicks delayed_work_time_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpShell);
};

typedef MessagePumpShell MessagePumpForUI;
typedef MessagePumpShell MessagePumpForIO;

} // namespace base

#endif // BASE_MESSAGE_PUMP_SHELL_H_
