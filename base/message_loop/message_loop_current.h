#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_CURRENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_CURRENT_H_

#ifndef USE_HACKY_COBALT_CHANGES
#error "Remove stubs"
#endif

#include "base/task/task_runner.h"

namespace base {

class MessageLoopCurrent {
 public:
  static MessageLoopCurrent Get() { return MessageLoopCurrent(); }
  MessageLoopCurrent* operator->() { return this; }
  class BASE_EXPORT DestructionObserver {
   public:
    virtual void WillDestroyCurrentMessageLoop() = 0;

   protected:
    virtual ~DestructionObserver() = default;
  };
  void AddDestructionObserver(DestructionObserver* destruction_observer) {};
};

}

#endif