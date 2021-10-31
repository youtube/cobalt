// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_UTIL_DISPATCH_COMMAND_FACTORY_H_
#define COBALT_WEBDRIVER_UTIL_DISPATCH_COMMAND_FACTORY_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"

namespace cobalt {
namespace webdriver {
namespace util {
namespace internal {
namespace {
template <typename T>
struct ForwardType {
  typedef const T& value;
};

template <typename T>
struct ForwardType<T&> {
  typedef T& value;
};

template <typename T, size_t n>
struct ForwardType<T[n]> {
  typedef const T* value;
};

// See comment for CallbackParamTraits<T[n]>.
template <typename T>
struct ForwardType<T[]> {
  typedef const T* value;
};

// unique_ptr-like move-only types.
template <typename T>
struct ForwardType<std::unique_ptr<T>> {
  typedef std::unique_ptr<T> value;
};
}  // namespace

// Convert a value to a base::Value to return it as a part of the WebDriver
// response object.
template <typename T>
std::unique_ptr<base::Value> ToValue(const T& value) {
  return T::ToValue(value);
}

template <typename T>
std::unique_ptr<base::Value> ToValue(const std::vector<T>& value) {
  std::unique_ptr<base::ListValue> list_value(new base::ListValue());
  for (int i = 0; i < value.size(); ++i) {
    list_value->Append(std::move(ToValue<T>(value[i])));
  }
  return std::unique_ptr<base::Value>(list_value.release());
}

// Partial template specialization for base::optional.
template <typename T>
std::unique_ptr<base::Value> ToValue(const base::Optional<T>& value) {
  if (value) {
    return ToValue<T>(*value);
  } else {
    return std::make_unique<base::Value>();
  }
}

// Template specialization for std::string.
template <>
std::unique_ptr<base::Value> ToValue(const std::string& value) {
  return std::unique_ptr<base::Value>(new base::Value(value));
}

// Template specialization for bool.
template <>
std::unique_ptr<base::Value> ToValue(const bool& value) {
  return std::unique_ptr<base::Value>(new base::Value(value));
}

template <typename R>
std::unique_ptr<base::Value> ToValue(const CommandResult<R>& command_result) {
  return ToValue(command_result.result());
}

template <>
std::unique_ptr<base::Value> ToValue(
    const CommandResult<void>& command_result) {
  return std::make_unique<base::Value>();
}

// Convert a base::Value to a base::Optional<T>. If value could not be converted
// base::nullopt will be returned.
template <typename T>
base::Optional<T> FromValue(const base::Value* value) {
  return T::FromValue(value);
}

template <>
base::Optional<GURL> FromValue(const base::Value* value) {
  const char kUrlKey[] = "url";
  std::string url;
  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString(kUrlKey, &url)) {
    return base::nullopt;
  }
  return GURL(url.c_str());
}

// Returns an appropriate response through the CommandResultHandler.
// On failure, the error response value is created and on success the
// result in the CommandResult instance is converted to a base::Value and
// returned.
template <typename R>
void ReturnResponse(const base::Optional<protocol::SessionId>& session_id,
                    const util::CommandResult<R>& command_result,
                    WebDriverDispatcher::CommandResultHandler* result_handler) {
  std::unique_ptr<base::Value> result;
  if (command_result.is_success()) {
    result = internal::ToValue(command_result);
  } else {
    result =
        protocol::Response::CreateErrorResponse(command_result.error_message());
  }
  result_handler->SendResult(session_id, command_result.status_code(),
                             std::move(result));
}

}  // namespace internal

// Helper class to create WebDriverDispatcher::DispatchCommandCallbacks that
// can be used with the WebDriverDispatcher::RegisterCommand function.
// Specify the XXXDriver class name as a template parameter.
template <class DriverClassT>
class DispatchCommandFactory
    : public base::RefCounted<DispatchCommandFactory<DriverClassT>> {
  // Max retries for the "can_retry" CommandResult case.
  static const int kMaxRetries = 5;

 public:
  // Typedef'd for less verbose code.
  typedef WebDriverDispatcher::CommandResultHandler CommandResultHandler;
  typedef WebDriverDispatcher::DispatchCommandCallback DispatchCommandCallback;
  typedef WebDriverDispatcher::PathVariableMap PathVariableMap;

  // Callback that takes PathVariableMap* and CommandResultHandler* as arguments
  // and returns a SessionDriver*.
  // Attempts to extract the sessionID from the PathVariableMap and finds the
  // session that this ID maps to. If no such session occurs, an error is sent
  // through the CommandResultHandler and NULL is returned.
  typedef base::Callback<SessionDriver*(const PathVariableMap* path_variables,
                                        CommandResultHandler*)>
      GetSessionCommand;

  // Callback that takes PathVariableMap* and CommandResultHandler* as arguments
  // and returns a DriverClass*.
  // If necessary extracts the id from the PathVariableMap and finds the driver
  // that this ID maps to. If no such driver occurs, an error is sent through
  // the CommandResultHandler and NULL is returned.
  typedef base::Callback<DriverClassT*(SessionDriver*,
                                       const PathVariableMap* path_variables,
                                       CommandResultHandler*)>
      GetDriverCommand;

  // Takes GetSessionCommand and GetDriverCommand callbacks. These will be
  // called when the DispatchCommandCallback is called to try to find the
  // correct XXXDriver based on the path variables.
  DispatchCommandFactory(const GetSessionCommand& get_session,
                         const GetDriverCommand& get_driver)
      : get_session_(get_session), get_driver_(get_driver) {}

  // Returns a DispatchCommandCallback that will call the specified
  // command_callback.
  // If the path variables successfully map to a DriverClass instance, it will
  // be passed as the argument to the command_callback.
  // If no such DriverClass instance can be found, the command_callback will not
  // be run. The results of the command will be sent through the
  // CommandResultHandler.
  template <typename R>
  DispatchCommandCallback GetCommandHandler(
      const base::Callback<util::CommandResult<R>(DriverClassT*)>&
          command_callback) {
    typedef CommandHandler<R> CommandHandler;
    return CommandHandler::GetCommandHandler(
        get_session_, get_driver_,
        base::Bind(&CommandHandler::RunCommand, command_callback));
  }

  // Returns a DispatchCommandCallback that will call the specified
  // command_callback with an argument.
  // If the path variables successfully map to a DriverClass instance, it will
  // be passed as the argument to the command_callback.
  // If the parameters passed to the command cannot be converted to an instance
  // of type A1, the command_callback will not be called an an appropriate
  // error will be returned through the CommandRequestHandler.
  // If no such DriverClass instance can be found, the command_callback will not
  // be run.
  template <typename R, typename A1>
  DispatchCommandCallback GetCommandHandler(
      const base::Callback<util::CommandResult<R>(DriverClassT*, const A1&)>&
          command_callback) {
    typedef CommandHandler<R> CommandHandler;
    return CommandHandler::GetCommandHandler(
        get_session_, get_driver_,
        base::Bind(&CommandHandler::template ExtractParameterAndRunCommand<A1>,
                   command_callback));
  }

 private:
  // Works with the DispatchCommandFactory to create a DispatchCommandCallback
  // for a particular return value R. Putting this into a nested template class
  // helps alleviate some of the headaches of working with C++ templates.
  template <typename R>
  class CommandHandler : public base::RefCounted<CommandHandler<R>> {
   public:
    // Wrapper around an actual command to run. It will extract arguments from
    // |parameters| if necessary and pass them to the command.
    // If parameters cannot be extracted, an appropriate error will be sent to
    // the CommandResultHandler.
    // Upon successful completion of the command, the result will be sent to
    // CommandResultHandler.
    typedef base::Callback<void(
        const base::Optional<protocol::SessionId>&, DriverClassT* driver,
        const base::Value* parameters, CommandResultHandler* result_handler)>
        RunCommandCallback;

    typedef base::Callback<util::CommandResult<R>(DriverClassT*)>
        DriverCommandFunction;
    // After binding the first argument, this function signature matches that of
    // RunCommandCallback.
    // The first argument is a base::Callback that runs a DriverClassT member
    // function with no parameters.
    static void RunCommand(
        const DriverCommandFunction& driver_command,
        const base::Optional<protocol::SessionId>& session_id,
        DriverClassT* driver, const base::Value* parameters,
        CommandResultHandler* result_handler) {
      // Ignore parameters.
      int retries = 0;
      util::CommandResult<R> command_result;
      do {
        command_result = driver_command.Run(driver);
      } while (command_result.can_retry() && (retries++ < kMaxRetries));
      internal::ReturnResponse(session_id, command_result, result_handler);
    }

    // After binding the first argument, this function signature matches that of
    // RunCommandCallback.
    // The first argument is a base::Callback that runs a DriverClassT member
    // function with one parameter.
    template <typename A1>
    static void ExtractParameterAndRunCommand(
        const base::Callback<util::CommandResult<R>(
            DriverClassT*, typename internal::ForwardType<A1>::value)>&
            driver_command,
        const base::Optional<protocol::SessionId>& session_id,
        DriverClassT* driver, const base::Value* parameters,
        CommandResultHandler* result_handler) {
      // Extract the parameter from |parameters|. If unsuccessful, return a
      // kInvalidParameters error. Otherwise, pass the extracted parameter to
      // the |driver_command|.
      using A1_NO_REF = typename std::remove_reference<A1>::type;
      base::Optional<A1_NO_REF> param =
          internal::FromValue<A1_NO_REF>(parameters);
      if (!param) {
        result_handler->SendInvalidRequestResponse(
            WebDriverDispatcher::CommandResultHandler::kInvalidParameters, "");
      } else {
        int retries = 0;
        util::CommandResult<R> command_result;
        do {
          command_result = driver_command.Run(driver, param.value());
        } while (command_result.can_retry() && (retries++ < kMaxRetries));
        internal::ReturnResponse(session_id, command_result, result_handler);
      }
    }

    // Create a new CommandHandler instance, and base::Bind to its HandleCommand
    // function.
    static DispatchCommandCallback GetCommandHandler(
        const GetSessionCommand& get_session_command,
        const GetDriverCommand& get_driver_command,
        const RunCommandCallback& run_command_callback) {
      scoped_refptr<CommandHandler> command_handler(new CommandHandler(
          get_session_command, get_driver_command, run_command_callback));
      return base::Bind(&CommandHandler::HandleCommand, command_handler);
    }

   private:
    // Simple constructor that sets each of the necessary callbacks.
    CommandHandler(const GetSessionCommand& get_session_callback,
                   const GetDriverCommand& get_driver_callback,
                   const RunCommandCallback& run_command_callback)
        : get_session_(get_session_callback),
          get_driver_(get_driver_callback),
          run_command_(run_command_callback) {}

    // This function matches the WebDriverDispatcher::DispatchCommandCallback
    // signature, and as such is appropriate to be passed to the
    // WebDriverDispatcher::RegisterCommand function.
    // Using the the callbacks stored in this CommandHandler, it will extract
    // the appropriate XXXDriver, and execute |run_command_| on it.
    void HandleCommand(const base::Value* value,
                       const PathVariableMap* path_variables,
                       std::unique_ptr<CommandResultHandler> result_handler) {
      SessionDriver* session =
          get_session_.Run(path_variables, result_handler.get());
      if (session) {
        DriverClassT* driver =
            get_driver_.Run(session, path_variables, result_handler.get());
        if (driver) {
          run_command_.Run(session->session_id(), driver, value,
                           result_handler.get());
        }
      }
    }

    GetSessionCommand get_session_;
    GetDriverCommand get_driver_;
    RunCommandCallback run_command_;
  };

  GetSessionCommand get_session_;
  GetDriverCommand get_driver_;
};

}  // namespace util
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_UTIL_DISPATCH_COMMAND_FACTORY_H_
