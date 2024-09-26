// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_FUNCTION_TEMPLATE_H_
#define GIN_FUNCTION_TEMPLATE_H_

#include <stddef.h>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/gin_export.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-template.h"

namespace gin {

struct InvokerOptions {
  bool holder_is_first_argument = false;
  const char* holder_type = nullptr;  // Null if unknown or not applicable.
};

namespace internal {

template<typename T>
struct CallbackParamTraits {
  typedef T LocalType;
};
template<typename T>
struct CallbackParamTraits<const T&> {
  typedef T LocalType;
};
template<typename T>
struct CallbackParamTraits<const T*> {
  typedef T* LocalType;
};

// CallbackHolder and CallbackHolderBase are used to pass a
// base::RepeatingCallback from CreateFunctionTemplate through v8 (via
// v8::FunctionTemplate) to DispatchToCallback, where it is invoked.

// This simple base class is used so that we can share a single object template
// among every CallbackHolder instance.
class GIN_EXPORT CallbackHolderBase {
 public:
  CallbackHolderBase(const CallbackHolderBase&) = delete;
  CallbackHolderBase& operator=(const CallbackHolderBase&) = delete;

  v8::Local<v8::External> GetHandle(v8::Isolate* isolate);

 protected:
  explicit CallbackHolderBase(v8::Isolate* isolate);
  virtual ~CallbackHolderBase();

 private:
  static void FirstWeakCallback(
      const v8::WeakCallbackInfo<CallbackHolderBase>& data);
  static void SecondWeakCallback(
      const v8::WeakCallbackInfo<CallbackHolderBase>& data);

  v8::Global<v8::External> v8_ref_;
};

template<typename Sig>
class CallbackHolder : public CallbackHolderBase {
 public:
  CallbackHolder(v8::Isolate* isolate,
                 base::RepeatingCallback<Sig> callback,
                 InvokerOptions invoker_options)
      : CallbackHolderBase(isolate),
        callback(std::move(callback)),
        invoker_options(std::move(invoker_options)) {}
  CallbackHolder(const CallbackHolder&) = delete;
  CallbackHolder& operator=(const CallbackHolder&) = delete;

  base::RepeatingCallback<Sig> callback;
  InvokerOptions invoker_options;

 private:
  ~CallbackHolder() override = default;
};

template <typename T>
bool GetNextArgument(Arguments* args,
                     const InvokerOptions& invoker_options,
                     bool is_first,
                     T* result) {
  if (is_first && invoker_options.holder_is_first_argument) {
    return args->GetHolder(result);
  } else {
    return args->GetNext(result);
  }
}

// For advanced use cases, we allow callers to request the unparsed Arguments
// object and poke around in it directly.
inline bool GetNextArgument(Arguments* args,
                            const InvokerOptions& invoker_options,
                            bool is_first,
                            Arguments* result) {
  *result = *args;
  return true;
}
inline bool GetNextArgument(Arguments* args,
                            const InvokerOptions& invoker_options,
                            bool is_first,
                            Arguments** result) {
  *result = args;
  return true;
}

// It's common for clients to just need the isolate, so we make that easy.
inline bool GetNextArgument(Arguments* args,
                            const InvokerOptions& invoker_options,
                            bool is_first,
                            v8::Isolate** result) {
  *result = args->isolate();
  return true;
}

// Throws an error indicating conversion failure.
GIN_EXPORT void ThrowConversionError(Arguments* args,
                                     const InvokerOptions& invoker_options,
                                     size_t index);

// Class template for extracting and storing single argument for callback
// at position |index|.
template <size_t index, typename ArgType>
struct ArgumentHolder {
  using ArgLocalType = typename CallbackParamTraits<ArgType>::LocalType;

  ArgLocalType value;
  bool ok;

  ArgumentHolder(Arguments* args, const InvokerOptions& invoker_options)
      : ok(GetNextArgument(args, invoker_options, index == 0, &value)) {
    if (!ok)
      ThrowConversionError(args, invoker_options, index);
  }
};

// Class template for converting arguments from JavaScript to C++ and running
// the callback with them.
template <typename IndicesType, typename... ArgTypes>
class Invoker;

template <size_t... indices, typename... ArgTypes>
class Invoker<std::index_sequence<indices...>, ArgTypes...>
    : public ArgumentHolder<indices, ArgTypes>... {
 public:
  // Invoker<> inherits from ArgumentHolder<> for each argument.
  // C++ has always been strict about the class initialization order,
  // so it is guaranteed ArgumentHolders will be initialized (and thus, will
  // extract arguments from Arguments) in the right order.
  Invoker(Arguments* args, const InvokerOptions& invoker_options)
      : ArgumentHolder<indices, ArgTypes>(args, invoker_options)...,
        args_(args) {}

  bool IsOK() {
    return And(ArgumentHolder<indices, ArgTypes>::ok...);
  }

  template <typename ReturnType>
  void DispatchToCallback(
      base::RepeatingCallback<ReturnType(ArgTypes...)> callback) {
    args_->Return(
        callback.Run(std::move(ArgumentHolder<indices, ArgTypes>::value)...));
  }

  // In C++, you can declare the function foo(void), but you can't pass a void
  // expression to foo. As a result, we must specialize the case of Callbacks
  // that have the void return type.
  void DispatchToCallback(base::RepeatingCallback<void(ArgTypes...)> callback) {
    callback.Run(std::move(ArgumentHolder<indices, ArgTypes>::value)...);
  }

 private:
  static bool And() { return true; }
  template <typename... T>
  static bool And(bool arg1, T... args) {
    return arg1 && And(args...);
  }

  raw_ptr<Arguments> args_;
};

// DispatchToCallback converts all the JavaScript arguments to C++ types and
// invokes the base::RepeatingCallback.
template <typename Sig>
struct Dispatcher {};

template <typename ReturnType, typename... ArgTypes>
struct Dispatcher<ReturnType(ArgTypes...)> {
  static void DispatchToCallbackImpl(Arguments* args) {
    v8::Local<v8::External> v8_holder;
    CHECK(args->GetData(&v8_holder));
    CallbackHolderBase* holder_base = reinterpret_cast<CallbackHolderBase*>(
        v8_holder->Value());

    typedef CallbackHolder<ReturnType(ArgTypes...)> HolderT;
    HolderT* holder = static_cast<HolderT*>(holder_base);

    using Indices = std::index_sequence_for<ArgTypes...>;
    Invoker<Indices, ArgTypes...> invoker(args, holder->invoker_options);
    if (invoker.IsOK())
      invoker.DispatchToCallback(holder->callback);
  }

  static void DispatchToCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    Arguments args(info);
    DispatchToCallbackImpl(&args);
  }

  static void DispatchToCallbackForProperty(
      v8::Local<v8::Name>,
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    Arguments args(info);
    DispatchToCallbackImpl(&args);
  }
};

}  // namespace internal

// CreateFunctionTemplate creates a v8::FunctionTemplate that will create
// JavaScript functions that execute a provided C++ function or
// base::RepeatingCallback. JavaScript arguments are automatically converted via
// gin::Converter, as is the return value of the C++ function, if any.
// |invoker_options| contains additional parameters. If it contains a
// holder_type, it will be used to provide a useful conversion error if the
// holder is the first argument. If not provided, a generic invocation error
// will be used.
//
// NOTE: V8 caches FunctionTemplates for a lifetime of a web page for its own
// internal reasons, thus it is generally a good idea to cache the template
// returned by this function.  Otherwise, repeated method invocations from JS
// will create substantial memory leaks. See http://crbug.com/463487.
template <typename Sig>
v8::Local<v8::FunctionTemplate> CreateFunctionTemplate(
    v8::Isolate* isolate,
    base::RepeatingCallback<Sig> callback,
    InvokerOptions invoker_options = {}) {
  typedef internal::CallbackHolder<Sig> HolderT;
  HolderT* holder =
      new HolderT(isolate, std::move(callback), std::move(invoker_options));

  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
      isolate, &internal::Dispatcher<Sig>::DispatchToCallback,
      ConvertToV8<v8::Local<v8::External>>(isolate, holder->GetHandle(isolate)),
      v8::Local<v8::Signature>(), 0, v8::ConstructorBehavior::kThrow);
  return tmpl;
}

// CreateDataPropertyCallback creates a v8::AccessorNameGetterCallback and
// corresponding data value that will hold and execute the provided
// base::RepeatingCallback, using automatic conversions similar to
// |CreateFunctionTemplate|.
//
// It is expected that these will be passed to v8::Template::SetLazyDataProperty
// or another similar function.
template <typename Sig>
std::pair<v8::AccessorNameGetterCallback, v8::Local<v8::Value>>
CreateDataPropertyCallback(v8::Isolate* isolate,
                           base::RepeatingCallback<Sig> callback,
                           InvokerOptions invoker_options = {}) {
  typedef internal::CallbackHolder<Sig> HolderT;
  HolderT* holder =
      new HolderT(isolate, std::move(callback), std::move(invoker_options));
  return {&internal::Dispatcher<Sig>::DispatchToCallbackForProperty,
          ConvertToV8<v8::Local<v8::External>>(isolate,
                                               holder->GetHandle(isolate))};
}

}  // namespace gin

#endif  // GIN_FUNCTION_TEMPLATE_H_
