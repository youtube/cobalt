// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_CLOSURE_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_CLOSURE_H_

#include "starboard/common/ref_counted.h"
#include "starboard/log.h"
#include "starboard/shared/internal_only.h"

#ifndef __cplusplus
#error "Only C++ files can include this header."
#endif

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

class Closure {
 public:
  class Functor : public RefCountedThreadSafe<Functor> {
   public:
    virtual ~Functor() {}
    virtual void Run() = 0;
  };

  explicit Closure(const scoped_refptr<Functor> functor = NULL)
      : functor_(functor) {}

  bool operator==(const Closure& that) const {
    return functor_ == that.functor_;
  }

  bool is_valid() const { return functor_ != NULL; }
  void reset() { functor_ = NULL; }
  void Run() {
    SB_DCHECK(functor_);
    if (functor_) {
      functor_->Run();
    }
  }

 private:
  scoped_refptr<Functor> functor_;
};

inline Closure Bind(void (*closure)()) {
  class FunctorImpl : public Closure::Functor {
   public:
    explicit FunctorImpl(void (*closure)()) : closure_(closure) {}

    void Run() override { closure_(); }

   private:
    void (*closure_)();
  };
  return Closure(new FunctorImpl(closure));
}

template <typename Param>
inline Closure Bind(void (*func)(Param), Param param) {
  class FunctorImpl : public Closure::Functor {
   public:
    FunctorImpl(void (*func)(Param), Param param)
        : func_(func), param_(param) {}

    void Run() override { func_(param_); }

   private:
    void (*func_)(Param);
    Param param_;
  };
  return Closure(new FunctorImpl(func, param));
}

template <typename C>
inline Closure Bind(void (C::*func)(), C* obj) {
  class FunctorImpl : public Closure::Functor {
   public:
    FunctorImpl(void (C::*func)(), C* obj) : func_(func), obj_(obj) {}

    void Run() override { ((*obj_).*func_)(); }

   private:
    void (C::*func_)();
    C* obj_;
  };
  return Closure(new FunctorImpl(func, obj));
}

template <typename C, typename Param>
inline Closure Bind(void (C::*func)(Param), C* obj, Param param) {
  class FunctorImpl : public Closure::Functor {
   public:
    FunctorImpl(void (C::*func)(Param), C* obj, Param param)
        : func_(func), obj_(obj), param_(param) {}

    void Run() override { ((*obj_).*func_)(param_); }

   private:
    void (C::*func_)(Param);
    C* obj_;
    Param param_;
  };
  return Closure(new FunctorImpl(func, obj, param));
}

template <typename C, typename Param>
inline Closure Bind(void (C::*func)(const Param&), C* obj, const Param& param) {
  class FunctorImpl : public Closure::Functor {
   public:
    FunctorImpl(void (C::*func)(const Param&), C* obj, const Param& param)
        : func_(func), obj_(obj), param_(param) {}

    void Run() override { ((*obj_).*func_)(param_); }

   private:
    void (C::*func_)(const Param&);
    C* obj_;
    Param param_;
  };
  return Closure(new FunctorImpl(func, obj, param));
}

template <typename C, typename Param1, typename Param2>
inline Closure Bind(void (C::*func)(Param1, Param2),
                    C* obj,
                    Param1 param1,
                    Param2 param2) {
  class FunctorImpl : public Closure::Functor {
   public:
    FunctorImpl(void (C::*func)(Param1, Param2),
                C* obj,
                Param1 param1,
                Param2 param2)
        : func_(func), obj_(obj), param1_(param1), param2_(param2) {}

    void Run() override { ((*obj_).*func_)(param1_, param2_); }

   private:
    void (C::*func_)(Param1, Param2);
    C* obj_;
    Param1 param1_;
    Param2 param2_;
  };
  return Closure(new FunctorImpl(func, obj, param1, param2));
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_CLOSURE_H_
