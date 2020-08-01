// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_H5VCC_H5VCC_EVENT_LISTENER_CONTAINER_H_
#define COBALT_H5VCC_H5VCC_EVENT_LISTENER_CONTAINER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

// Template class to implement a container of event listeners where the
// listener callback can take an argument of any type, including none (void).
// Callback type must be specified in addition to callback argument type,
// as we cannot typedef a callback taking a void argument.
template <class CallbackArgType, class CallbackType>
class H5vccEventListenerContainer {
 public:
  typedef script::ScriptValue<CallbackType> CallbackHolderType;

  // Type for a callback that returns the value of the argument to be passed
  // to the callback for each listener.
  typedef base::Callback<CallbackArgType()> GetArgumentCallback;

  // Type for a listener.
  // We store the message loop from which the listener was registered,
  // so we can run the callback on the same loop.
  struct Listener {
    Listener(script::Wrappable* owner, const CallbackHolderType& cb)
        : callback(owner, cb),
          task_runner(base::MessageLoop::current()->task_runner()) {}

    // Notifies listener. Must be called on the same message loop the
    // listener registered its callback from.
    void Notify(GetArgumentCallback on_notify) {
      DCHECK_EQ(base::MessageLoop::current()->task_runner(), task_runner);
      CallbackArgType arg = on_notify.Run();
      callback.value().Run(arg);
    }

    typename CallbackHolderType::Reference callback;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };

  explicit H5vccEventListenerContainer(script::Wrappable* owner)
      : owner_(owner) {}

  ~H5vccEventListenerContainer() {
    // Delete all registered listeners.
    for (typename ListenerVector::const_iterator it = listeners_.begin();
         it != listeners_.end(); ++it) {
      delete *it;
    }
  }

  // Returns true if there is no listener.
  bool empty() {
    base::AutoLock auto_lock(lock_);
    return listeners_.empty();
  }

  // Called from JavaScript to register an event listener. May be called from
  // any thread, and event notification will be called on the same thread.
  void AddListener(const CallbackHolderType& callback_holder) {
    base::AutoLock auto_lock(lock_);
    listeners_.push_back(new Listener(owner_, callback_holder));
  }

  // Called from JavaScript to register an event listener. May be called from
  // any thread, and event notification will be called on the same thread.
  // Call the first listener if the callback does not return empty.
  void AddListenerAndCallIfFirst(const CallbackHolderType& callback_holder,
                                 GetArgumentCallback arg_callback) {
    base::AutoLock auto_lock(lock_);
    bool was_empty = listeners_.empty();
    listeners_.push_back(new Listener(owner_, callback_holder));
    if (was_empty) {
      CallbackArgType arg = arg_callback.Run();
      if (!arg.empty()) {
        listeners_.back()->callback.value().Run(arg);
      }
    }
  }

  // Dispatches an event to the registered listeners. May be called from any
  // thread, and the callbacks will be invoked on the same thread each listener
  // was registered on. |get_argument_callback| must be a function that
  // returns the argument value for this event.
  void DispatchEvent(GetArgumentCallback get_argument_callback) {
    base::AutoLock auto_lock(lock_);
    for (typename ListenerVector::iterator it = listeners_.begin();
         it != listeners_.end(); ++it) {
      Listener* listener = *it;
      listener->task_runner->PostTask(
          FROM_HERE, base::Bind(&Listener::Notify, base::Unretained(listener),
                                get_argument_callback));
    }
  }

 private:
  typedef std::vector<Listener*> ListenerVector;

  script::Wrappable* owner_;
  ListenerVector listeners_;
  base::Lock lock_;
};

// Explicit template specialization for the no callback argument case, where
// we don't need to call the |GetArgumentCallback| callback.
template <>
inline void
    H5vccEventListenerContainer<void, script::CallbackFunction<void()> >::
        Listener::Notify(GetArgumentCallback) {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(), task_runner);
  callback.value().Run();
}

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_EVENT_LISTENER_CONTAINER_H_
