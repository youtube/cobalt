// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_GMOCK_MUTANT_H_
#define TESTING_GMOCK_MUTANT_H_

// The intention of this file is to make possible using GMock actions in
// all of its syntactic beauty. Classes and helper functions can be used as
// more generic variants of Task and Callback classes (see base/task.h)
// Mutant supports both pre-bound arguments (like Task) and call-time
// arguments (like Callback) - hence the name. :-)
//
// DispatchToMethod supports two sets of arguments: pre-bound (P) and
// call-time (C). The arguments as well as the return type are templatized.
// DispatchToMethod will also try to call the selected method even if provided
// pre-bound arguments does not match exactly with the function signature
// hence the X1, X2 ... XN parameters in CreateFunctor.
//
// Additionally you can bind the object at calltime by binding a pointer to
// pointer to the object at creation time - before including this file you
// have to #define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING.
//
// TODO(stoyan): It's yet not clear to me should we use T& and T&* instead
// of T* and T** when we invoke CreateFunctor to match the EXPECT_CALL style.
//
//
// Sample usage with gMock:
//
// struct Mock : public ObjectDelegate {
//   MOCK_METHOD2(string, OnRequest(int n, const string& request));
//   MOCK_METHOD1(void, OnQuit(int exit_code));
//   MOCK_METHOD2(void, LogMessage(int level, const string& message));
//
//   string HandleFlowers(const string& reply, int n, const string& request) {
//     string result = SStringPrintf("In request of %d %s ", n, request);
//     for (int i = 0; i < n; ++i) result.append(reply)
//     return result;
//   }
//
//   void DoLogMessage(int level, const string& message) {
//   }
//
//   void QuitMessageLoop(int seconds) {
//     MessageLoop* loop = MessageLoop::current();
//     loop->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask,
//                           1000 * seconds);
//   }
// };
//
// Mock mock;
// // Will invoke mock.HandleFlowers("orchids", n, request)
// // "orchids" is a pre-bound argument, and <n> and <request> are call-time
// // arguments - they are not known until the OnRequest mock is invoked.
// EXPECT_CALL(mock, OnRequest(Ge(5), StartsWith("flower"))
//   .Times(1)
//   .WillOnce(Invoke(CreateFunctor(&mock, &Mock::HandleFlowers,
//       string("orchids"))));
//
//
// // No pre-bound arguments, two call-time arguments passed
// // directly to DoLogMessage
// EXPECT_CALL(mock, OnLogMessage(_, _))
//   .Times(AnyNumber())
//   .WillAlways(Invoke(CreateFunctor, &mock, &Mock::DoLogMessage));
//
//
// // In this case we have a single pre-bound argument - 3. We ignore
// // all of the arguments of OnQuit.
// EXCEPT_CALL(mock, OnQuit(_))
//   .Times(1)
//   .WillOnce(InvokeWithoutArgs(CreateFunctor(
//       &mock, &Mock::QuitMessageLoop, 3)));
//
// MessageLoop loop;
// loop.Run();
//
//
//  // Here is another example of how we can set an action that invokes
//  // method of an object that is not yet created.
// struct Mock : public ObjectDelegate {
//   MOCK_METHOD1(void, DemiurgeCreated(Demiurge*));
//   MOCK_METHOD2(void, OnRequest(int count, const string&));
//
//   void StoreDemiurge(Demiurge* w) {
//     demiurge_ = w;
//   }
//
//   Demiurge* demiurge;
// }
//
// EXPECT_CALL(mock, DemiurgeCreated(_)).Times(1)
//    .WillOnce(Invoke(CreateFunctor(&mock, &Mock::StoreDemiurge)));
//
// EXPECT_CALL(mock, OnRequest(_, StrEq("Moby Dick")))
//    .Times(AnyNumber())
//    .WillAlways(WithArgs<0>(Invoke(
//        CreateFunctor(&mock->demiurge_, &Demiurge::DecreaseMonsters))));
//

#include "base/linked_ptr.h"
#include "base/tuple.h"  // for Tuple

namespace testing {

// 0 - 0
template <typename R, typename T, typename Method>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple0& c) {
  return (obj->*method)();
}

// 0 - 1
template <typename R, typename T, typename Method, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(c.a);
}

// 0 - 2
template <typename R, typename T, typename Method, typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(c.a, c.b);
}

// 0 - 3
template <typename R, typename T, typename Method, typename C1, typename C2,
          typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(c.a, c.b, c.c);
}

// 0 - 4
template <typename R, typename T, typename Method, typename C1, typename C2,
          typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple0& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(c.a, c.b, c.c, c.d);
}

// 1 - 0
template <typename R, typename T, typename Method, typename P1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a);
}

// 1 - 1
template <typename R, typename T, typename Method, typename P1, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, c.a);
}

// 1 - 2
template <typename R, typename T, typename Method, typename P1, typename C1,
          typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, c.a, c.b);
}

// 1 - 3
template <typename R, typename T, typename Method, typename P1, typename C1,
          typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, c.a, c.b, c.c);
}

// 1 - 4
template <typename R, typename T, typename Method, typename P1, typename C1,
          typename C2, typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple1<P1>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, c.a, c.b, c.c, c.d);
}

// 2 - 0
template <typename R, typename T, typename Method, typename P1, typename P2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a, p.b);
}

// 2 - 1
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, p.b, c.a);
}

// 2 - 2
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, p.b, c.a, c.b);
}

// 2 - 3
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1, typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, p.b, c.a, c.b, c.c);
}

// 2 - 4
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename C1, typename C2, typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple2<P1, P2>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, p.b, c.a, c.b, c.c, c.d);
}

// 3 - 0
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a, p.b, p.c);
}

// 3 - 1
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a);
}

// 3 - 2
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a, c.b);
}

// 3 - 3
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1, typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a, c.b, c.c);
}

// 3 - 4
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename C1, typename C2, typename C3, typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple3<P1, P2, P3>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, p.b, p.c, c.a, c.b, c.c, c.d);
}

// 4 - 0
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple0& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d);
}

// 4 - 1
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple1<C1>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a);
}

// 4 - 2
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1, typename C2>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple2<C1, C2>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b);
}

// 4 - 3
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1, typename C2, typename C3>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple3<C1, C2, C3>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b, c.c);
}

// 4 - 4
template <typename R, typename T, typename Method, typename P1, typename P2,
          typename P3, typename P4, typename C1, typename C2, typename C3,
          typename C4>
inline R DispatchToMethod(T* obj, Method method,
                          const Tuple4<P1, P2, P3, P4>& p,
                          const Tuple4<C1, C2, C3, C4>& c) {
  return (obj->*method)(p.a, p.b, p.c, p.d, c.a, c.b, c.c, c.d);
}

// Interface that is exposed to the consumer, that does the actual calling
// of the method.
template <typename R, typename Params>
class MutantRunner {
 public:
  virtual R RunWithParams(const Params& params) = 0;
  virtual ~MutantRunner() {}
};

// Mutant holds pre-bound arguments (like Task). Like Callback
// allows call-time arguments. You bind a pointer to the object
// at creation time.
template <typename R, typename T, typename Method,
          typename PreBound, typename Params>
class Mutant : public MutantRunner<R, Params> {
 public:
  Mutant(T* obj, Method method, const PreBound& pb)
      : obj_(obj), method_(method), pb_(pb) {
  }

  // MutantRunner implementation
  virtual R RunWithParams(const Params& params) {
    return DispatchToMethod<R>(this->obj_, this->method_, pb_, params);
  }

  T* obj_;
  Method method_;
  PreBound pb_;
};

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// MutantLateBind is like Mutant, but you bind a pointer to a pointer
// to the object. This way you can create actions for an object
// that is not yet created (has only storage for a pointer to it).
template <typename R, typename T, typename Method,
          typename PreBound, typename Params>
class MutantLateObjectBind : public MutantRunner<R, Params> {
 public:
  MutantLateObjectBind(T** obj, Method method, const PreBound& pb)
      : obj_(obj), method_(method), pb_(pb) {
  }

  // MutantRunner implementation.
  virtual R RunWithParams(const Params& params) {
    EXPECT_THAT(*this->obj_, testing::NotNull());
    if (NULL == *this->obj_)
      return R();
    return DispatchToMethod<R>( *this->obj_, this->method_, pb_, params);
  }

  T** obj_;
  Method method_;
  PreBound pb_;
};
#endif

// Simple MutantRunner<> wrapper acting as a functor.
// Redirects operator() to MutantRunner<Params>::Run()
template <typename R, typename Params>
struct MutantFunctor {
  explicit MutantFunctor(MutantRunner<R, Params>*  cb) : impl_(cb) {
  }

  ~MutantFunctor() {
  }

  inline R operator()() {
    return impl_->RunWithParams(Tuple0());
  }

  template <typename Arg1>
  inline R operator()(const Arg1& a) {
    return impl_->RunWithParams(Params(a));
  }

  template <typename Arg1, typename Arg2>
  inline R operator()(const Arg1& a, const Arg2& b) {
    return impl_->RunWithParams(Params(a, b));
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  inline R operator()(const Arg1& a, const Arg2& b, const Arg3& c) {
    return impl_->RunWithParams(Params(a, b, c));
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  inline R operator()(const Arg1& a, const Arg2& b, const Arg3& c,
                         const Arg4& d) {
    return impl_->RunWithParams(Params(a, b, c, d));
  }

 private:
  // We need copy constructor since MutantFunctor is copied few times
  // inside GMock machinery, hence no DISALLOW_EVIL_CONTRUCTORS
  MutantFunctor();
  linked_ptr<MutantRunner<R, Params> > impl_;
};



// 0 - 0
template <typename R, typename T>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)()) {
  MutantRunner<R, Tuple0> *t =
      new Mutant<R, T, R (T::*)(),
                 Tuple0, Tuple0>
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple0>(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 0 - 0
template <typename R, typename T>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T** obj, R (T::*method)()) {
  MutantRunner<R, Tuple0> *t =
      new MutantLateObjectBind<R, T, R (T::*)(),
                               Tuple0, Tuple0>
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple0>(t);
}
#endif

// 0 - 1
template <typename R, typename T, typename A1>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(A1)) {
  MutantRunner<R, Tuple1<A1> > *t =
      new Mutant<R, T, R (T::*)(A1),
                 Tuple0, Tuple1<A1> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple1<A1> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 0 - 1
template <typename R, typename T, typename A1>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T** obj, R (T::*method)(A1)) {
  MutantRunner<R, Tuple1<A1> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(A1),
                               Tuple0, Tuple1<A1> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple1<A1> >(t);
}
#endif

// 0 - 2
template <typename R, typename T, typename A1, typename A2>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(A1, A2)) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new Mutant<R, T, R (T::*)(A1, A2),
                 Tuple0, Tuple2<A1, A2> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 0 - 2
template <typename R, typename T, typename A1, typename A2>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T** obj, R (T::*method)(A1, A2)) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(A1, A2),
                               Tuple0, Tuple2<A1, A2> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}
#endif

// 0 - 3
template <typename R, typename T, typename A1, typename A2, typename A3>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(A1, A2, A3)) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new Mutant<R, T, R (T::*)(A1, A2, A3),
                 Tuple0, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 0 - 3
template <typename R, typename T, typename A1, typename A2, typename A3>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T** obj, R (T::*method)(A1, A2, A3)) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(A1, A2, A3),
                               Tuple0, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}
#endif

// 0 - 4
template <typename R, typename T, typename A1, typename A2, typename A3,
          typename A4>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(A1, A2, A3, A4)) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new Mutant<R, T, R (T::*)(A1, A2, A3, A4),
                 Tuple0, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 0 - 4
template <typename R, typename T, typename A1, typename A2, typename A3,
          typename A4>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T** obj, R (T::*method)(A1, A2, A3, A4)) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(A1, A2, A3, A4),
                               Tuple0, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple());
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}
#endif

// 1 - 0
template <typename R, typename T, typename P1, typename X1>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1), const P1& p1) {
  MutantRunner<R, Tuple0> *t =
      new Mutant<R, T, R (T::*)(X1),
                 Tuple1<P1>, Tuple0>
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple0>(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 1 - 0
template <typename R, typename T, typename P1, typename X1>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T** obj, R (T::*method)(X1), const P1& p1) {
  MutantRunner<R, Tuple0> *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1),
                               Tuple1<P1>, Tuple0>
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple0>(t);
}
#endif

// 1 - 1
template <typename R, typename T, typename P1, typename A1, typename X1>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, A1), const P1& p1) {
  MutantRunner<R, Tuple1<A1> > *t =
      new Mutant<R, T, R (T::*)(X1, A1),
                 Tuple1<P1>, Tuple1<A1> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 1 - 1
template <typename R, typename T, typename P1, typename A1, typename X1>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T** obj, R (T::*method)(X1, A1), const P1& p1) {
  MutantRunner<R, Tuple1<A1> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, A1),
                               Tuple1<P1>, Tuple1<A1> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple1<A1> >(t);
}
#endif

// 1 - 2
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename X1>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, A1, A2), const P1& p1) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new Mutant<R, T, R (T::*)(X1, A1, A2),
                 Tuple1<P1>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 1 - 2
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename X1>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T** obj, R (T::*method)(X1, A1, A2), const P1& p1) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, A1, A2),
                               Tuple1<P1>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}
#endif

// 1 - 3
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename A3, typename X1>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, A1, A2, A3), const P1& p1) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new Mutant<R, T, R (T::*)(X1, A1, A2, A3),
                 Tuple1<P1>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 1 - 3
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename A3, typename X1>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T** obj, R (T::*method)(X1, A1, A2, A3), const P1& p1) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, A1, A2, A3),
                               Tuple1<P1>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}
#endif

// 1 - 4
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename A3, typename A4, typename X1>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, A1, A2, A3, A4), const P1& p1) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new Mutant<R, T, R (T::*)(X1, A1, A2, A3, A4),
                 Tuple1<P1>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 1 - 4
template <typename R, typename T, typename P1, typename A1, typename A2,
          typename A3, typename A4, typename X1>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T** obj, R (T::*method)(X1, A1, A2, A3, A4), const P1& p1) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, A1, A2, A3, A4),
                               Tuple1<P1>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}
#endif

// 2 - 0
template <typename R, typename T, typename P1, typename P2, typename X1,
          typename X2>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1, X2), const P1& p1, const P2& p2) {
  MutantRunner<R, Tuple0> *t =
      new Mutant<R, T, R (T::*)(X1, X2),
                 Tuple2<P1, P2>, Tuple0>
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple0>(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 2 - 0
template <typename R, typename T, typename P1, typename P2, typename X1,
          typename X2>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T** obj, R (T::*method)(X1, X2), const P1& p1, const P2& p2) {
  MutantRunner<R, Tuple0> *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2),
                               Tuple2<P1, P2>, Tuple0>
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple0>(t);
}
#endif

// 2 - 1
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename X1, typename X2>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1), const P1& p1, const P2& p2) {
  MutantRunner<R, Tuple1<A1> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, A1),
                 Tuple2<P1, P2>, Tuple1<A1> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 2 - 1
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename X1, typename X2>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, A1), const P1& p1, const P2& p2) {
  MutantRunner<R, Tuple1<A1> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, A1),
                               Tuple2<P1, P2>, Tuple1<A1> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple1<A1> >(t);
}
#endif

// 2 - 2
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename X1, typename X2>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1, A2), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, A1, A2),
                 Tuple2<P1, P2>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 2 - 2
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename X1, typename X2>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, A1, A2), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, A1, A2),
                               Tuple2<P1, P2>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}
#endif

// 2 - 3
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename A3, typename X1, typename X2>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1, A2, A3), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, A1, A2, A3),
                 Tuple2<P1, P2>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 2 - 3
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename A3, typename X1, typename X2>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, A1, A2, A3), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, A1, A2, A3),
                               Tuple2<P1, P2>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}
#endif

// 2 - 4
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename A3, typename A4, typename X1, typename X2>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, A1, A2, A3, A4), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, A1, A2, A3, A4),
                 Tuple2<P1, P2>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 2 - 4
template <typename R, typename T, typename P1, typename P2, typename A1,
          typename A2, typename A3, typename A4, typename X1, typename X2>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, A1, A2, A3, A4), const P1& p1,
    const P2& p2) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, A1, A2, A3, A4),
                               Tuple2<P1, P2>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1, p2));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}
#endif

// 3 - 0
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3), const P1& p1, const P2& p2,
    const P3& p3) {
  MutantRunner<R, Tuple0> *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3),
                 Tuple3<P1, P2, P3>, Tuple0>
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple0>(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 3 - 0
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3), const P1& p1, const P2& p2,
    const P3& p3) {
  MutantRunner<R, Tuple0> *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3),
                               Tuple3<P1, P2, P3>, Tuple0>
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple0>(t);
}
#endif

// 3 - 1
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple1<A1> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, A1),
                 Tuple3<P1, P2, P3>, Tuple1<A1> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 3 - 1
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, A1), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple1<A1> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, A1),
                               Tuple3<P1, P2, P3>, Tuple1<A1> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple1<A1> >(t);
}
#endif

// 3 - 2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1, A2), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, A1, A2),
                 Tuple3<P1, P2, P3>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 3 - 2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename X1, typename X2, typename X3>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, A1, A2), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, A1, A2),
                               Tuple3<P1, P2, P3>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}
#endif

// 3 - 3
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename A3, typename X1, typename X2,
          typename X3>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1, A2, A3), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, A1, A2, A3),
                 Tuple3<P1, P2, P3>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 3 - 3
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename A3, typename X1, typename X2,
          typename X3>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, A1, A2, A3), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, A1, A2, A3),
                               Tuple3<P1, P2, P3>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}
#endif

// 3 - 4
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename A3, typename A4, typename X1,
          typename X2, typename X3>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, A1, A2, A3, A4), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, A1, A2, A3, A4),
                 Tuple3<P1, P2, P3>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 3 - 4
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename A1, typename A2, typename A3, typename A4, typename X1,
          typename X2, typename X3>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, A1, A2, A3, A4), const P1& p1,
    const P2& p2, const P3& p3) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, A1, A2, A3, A4),
                               Tuple3<P1, P2, P3>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1, p2, p3));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}
#endif

// 4 - 0
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename X1, typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple0> *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, X4),
                 Tuple4<P1, P2, P3, P4>, Tuple0>
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple0>(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 4 - 0
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename X1, typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple0>
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, X4), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple0> *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, X4),
                               Tuple4<P1, P2, P3, P4>, Tuple0>
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple0>(t);
}
#endif

// 4 - 1
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename X1, typename X2, typename X3,
          typename X4>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple1<A1> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, X4, A1),
                 Tuple4<P1, P2, P3, P4>, Tuple1<A1> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple1<A1> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 4 - 1
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename X1, typename X2, typename X3,
          typename X4>
inline MutantFunctor<R, Tuple1<A1> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, X4, A1), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple1<A1> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, X4, A1),
                               Tuple4<P1, P2, P3, P4>, Tuple1<A1> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple1<A1> >(t);
}
#endif

// 4 - 2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename X1, typename X2,
          typename X3, typename X4>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1, A2), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, X4, A1, A2),
                 Tuple4<P1, P2, P3, P4>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 4 - 2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename X1, typename X2,
          typename X3, typename X4>
inline MutantFunctor<R, Tuple2<A1, A2> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, X4, A1, A2), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple2<A1, A2> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, X4, A1, A2),
                               Tuple4<P1, P2, P3, P4>, Tuple2<A1, A2> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple2<A1, A2> >(t);
}
#endif

// 4 - 3
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename A3, typename X1,
          typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1, A2, A3), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, X4, A1, A2, A3),
                 Tuple4<P1, P2, P3, P4>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 4 - 3
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename A3, typename X1,
          typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple3<A1, A2, A3> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, X4, A1, A2, A3), const P1& p1,
    const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple3<A1, A2, A3> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, X4, A1, A2, A3),
                               Tuple4<P1, P2, P3, P4>, Tuple3<A1, A2, A3> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple3<A1, A2, A3> >(t);
}
#endif

// 4 - 4
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename A3, typename A4,
          typename X1, typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T* obj, R (T::*method)(X1, X2, X3, X4, A1, A2, A3, A4),
    const P1& p1, const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new Mutant<R, T, R (T::*)(X1, X2, X3, X4, A1, A2, A3, A4),
                 Tuple4<P1, P2, P3, P4>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}

#ifdef GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
// 4 - 4
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename A1, typename A2, typename A3, typename A4,
          typename X1, typename X2, typename X3, typename X4>
inline MutantFunctor<R, Tuple4<A1, A2, A3, A4> >
CreateFunctor(T** obj, R (T::*method)(X1, X2, X3, X4, A1, A2, A3, A4),
    const P1& p1, const P2& p2, const P3& p3, const P4& p4) {
  MutantRunner<R, Tuple4<A1, A2, A3, A4> > *t =
      new MutantLateObjectBind<R, T, R (T::*)(X1, X2, X3, X4, A1, A2, A3, A4),
                               Tuple4<P1, P2, P3, P4>, Tuple4<A1, A2, A3, A4> >
          (obj, method, MakeTuple(p1, p2, p3, p4));
  return MutantFunctor<R, Tuple4<A1, A2, A3, A4> >(t);
}
#endif

}  // namespace testing

#endif  // TESTING_GMOCK_MUTANT_H_
