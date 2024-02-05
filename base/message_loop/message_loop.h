#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_

#ifndef USE_HACKY_COBALT_CHANGES
#error "Remove stubs"
#endif

#include "base/task/task_runner.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"

namespace base {

class MessageLoopForUI {
 public:
  void Start() {}
  void Quit() {}
};

class MessageLoop: public MessageLoopCurrent {
 public:
  MessageLoop() {}
  MessageLoop(MessagePumpType type) {}
  MessageLoop* message_loop() const { return nullptr; }
  const scoped_refptr<SingleThreadTaskRunner>& task_runner() const { return task_runner_; }
  static MessageLoop* current() { return nullptr; }
  static const MessagePumpType TYPE_IO = MessagePumpType::IO;
  static const MessagePumpType TYPE_DEFAULT = MessagePumpType::DEFAULT;
  void RemoveDestructionObserver(DestructionObserver* destruction_observer) {}
  enum Type {
    TYPE_DEFAULT_H,
    TYPE_UI,
  };
  Type type() { return TYPE_DEFAULT_H; }
 private:
  scoped_refptr<SingleThreadTaskRunner> task_runner_ = nullptr;
};

}

#endif