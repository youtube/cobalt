#ifndef BASE_MESSAGE_PUMP_PS3_H_
#define BASE_MESSAGE_PUMP_PS3_H_
#pragma once

#include "base/message_pump.h"
#include "base/time.h"
#include "base/synchronization/waitable_event.h"

namespace base {

class BASE_EXPORT MessagePumpShell : public MessagePump {
 public:

  MessagePumpShell();

  // ------------------------------------------- UI
  class Observer {
    // __LB_PS3__WRITE_ME__
  };

  class Dispatcher {
    // __LB_PS3__WRITE_ME__
  };

  // ------------------------------------------- IO
  class IOObserver {
     // __LB_PS3__WRITE_ME__
  };

  class Watcher {
  public:
    Watcher();
    Watcher(Watcher &);
    // __LB_PS3__WRITE_ME__
  };

  // this one only watches sockets, not all file descriptors
  class FileDescriptorWatcher {
  public:
    FileDescriptorWatcher();
    FileDescriptorWatcher(FileDescriptorWatcher &);
    // __LB_PS3__WRITE_ME__
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
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ------------------------------------------- IO
  void AddIOObserver(IOObserver* obs);
  void RemoveIOObserver(IOObserver* obs);

  bool WatchSocket(int s,
                   bool persistent,
                   Mode mode,
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

#endif // BASE_MESSAGE_PUMP_PS3_H_
