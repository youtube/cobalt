// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "client/crashpad_client.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "starboard/common/mutex.h"
#include "third_party/lss/lss.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/linux/exception_handler_client.h"
#include "util/linux/exception_information.h"
#include "util/linux/scoped_pr_set_dumpable.h"
#include "util/linux/scoped_pr_set_ptracer.h"
#include "util/linux/socket.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/double_fork_and_exec.h"
#include "util/posix/signals.h"
// TODO(b/201538792): resolve conflict between mini_chromium and base functions.
#ifdef LogMessage
#define LOG_MESSAGE_DEFINED
#undef LogMessage
#endif
#include "third_party/crashpad/wrapper/proto/crashpad_annotations.pb.h"
#ifdef LOG_MESSAGE_DEFINED
#define LogMessage MLogMessage
#endif

namespace crashpad {

namespace {

#if defined(STARBOARD)
constexpr char kEvergreenInfoKey[] = "evergreen-information";
constexpr char kAnnotationKey[] = "annotation=%s";
#endif

std::string FormatArgumentInt(const std::string& name, int value) {
  return base::StringPrintf("--%s=%d", name.c_str(), value);
}

std::string FormatArgumentAddress(const std::string& name, const void* addr) {
  return base::StringPrintf("--%s=%p", name.c_str(), addr);
}

#if defined(STARBOARD)
std::string FormatArgumentString(const std::string& name,
                                 const std::string& value) {
  return base::StringPrintf("--%s=%s", name.c_str(), value.c_str());
}

bool UpdateAnnotation(std::string& annotation,
                            const std::string key,
                            const std::string& new_value) {
  if (new_value.empty()) {
    return false;
  }
  // The annotation is in the format --key=value
  if (annotation.compare(2, key.size(), key) == 0) {
    annotation = FormatArgumentString(key, new_value);
    LOG(INFO) << "Updated annotation: " << annotation;
    return true;
  }
  return false;
}

void AddAnnotation(std::vector<std::string>& argv_strings,
                         const std::string& key,
                         const std::string& new_value) {
  std::string v = FormatArgumentString(key, new_value);
  argv_strings.push_back(v);
  LOG(INFO) << "Added annotation: " << v;
}
#endif

#if defined(OS_ANDROID)

std::vector<std::string> BuildAppProcessArgs(
    const std::string& class_name,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
#if defined(ARCH_CPU_64_BITS)
  static constexpr char kAppProcess[] = "/system/bin/app_process64";
#else
  static constexpr char kAppProcess[] = "/system/bin/app_process32";
#endif

  std::vector<std::string> argv;
  argv.push_back(kAppProcess);
  argv.push_back("/system/bin");
  argv.push_back("--application");
  argv.push_back(class_name);

  std::vector<std::string> handler_argv =
      BuildHandlerArgvStrings(base::FilePath(kAppProcess),
                              database,
                              metrics_dir,
                              url,
                              annotations,
                              arguments);

  if (socket != kInvalidFileHandle) {
    handler_argv.push_back(FormatArgumentInt("initial-client-fd", socket));
  }

  argv.insert(argv.end(), handler_argv.begin(), handler_argv.end());
  return argv;
}

std::vector<std::string> BuildArgsToLaunchWithLinker(
    const std::string& handler_trampoline,
    const std::string& handler_library,
    bool is_64_bit,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv;
  if (is_64_bit) {
    argv.push_back("/system/bin/linker64");
  } else {
    argv.push_back("/system/bin/linker");
  }
  argv.push_back(handler_trampoline);
  argv.push_back(handler_library);

  std::vector<std::string> handler_argv = BuildHandlerArgvStrings(
      base::FilePath(), database, metrics_dir, url, annotations, arguments);

  if (socket != kInvalidFileHandle) {
    handler_argv.push_back(FormatArgumentInt("initial-client-fd", socket));
  }

  argv.insert(argv.end(), handler_argv.begin() + 1, handler_argv.end());
  return argv;
}

#endif  // OS_ANDROID

// A base class for Crashpad signal handler implementations.
class SignalHandler {
 public:
  // Returns the currently installed signal hander. May be `nullptr` if no
  // handler has been installed.
  static SignalHandler* Get() { return handler_; }

  // Disables any installed Crashpad signal handler for the calling thread. If a
  // crash signal is received, any previously installed (non-Crashpad) signal
  // handler will be restored and the signal reraised.
  static void DisableForThread() { disabled_for_thread_ = true; }

  void SetFirstChanceHandler(CrashpadClient::FirstChanceHandler handler) {
    first_chance_handler_ = handler;
  }

#if defined(STARBOARD)
  bool SendSanitizationInformation(SanitizationInformation sanitization_information) {
    sanitization_information_ = sanitization_information;
    return true;
  }
  bool SendEvergreenInfo(EvergreenInfo evergreen_info) {
    evergreen_info_ = evergreen_info;
    return SendEvergreenInfoImpl();
  }

  bool InsertAnnotation(const char* key, const char* value) {
    return InsertAnnotationImpl(key, value);
  }

#endif

  // The base implementation for all signal handlers, suitable for calling
  // directly to simulate signal delivery.
  bool HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    if (disabled_for_thread_) {
      return false;
    }

    if (first_chance_handler_ &&
        first_chance_handler_(
            signo, siginfo, static_cast<ucontext_t*>(context))) {
      return true;
    }

    exception_information_.siginfo_address =
        FromPointerCast<decltype(exception_information_.siginfo_address)>(
            siginfo);
    exception_information_.context_address =
        FromPointerCast<decltype(exception_information_.context_address)>(
            context);
    exception_information_.thread_id = sys_gettid();

    ScopedPrSetDumpable set_dumpable(false);
    HandleCrashImpl();
    return false;
  }

 protected:
  SignalHandler() = default;

  bool Install(const std::set<int>* unhandled_signals) {
    DCHECK(!handler_);
    handler_ = this;
    return Signals::InstallCrashHandlers(
        HandleOrReraiseSignal, 0, &old_actions_, unhandled_signals);
  }

#if defined(STARBOARD)
  const EvergreenInfo& GetEvergreenInfo() { return evergreen_info_; }
  const SanitizationInformation& GetSanitizationInformation() {
    return sanitization_information_;
  }
#endif

  const ExceptionInformation& GetExceptionInfo() {
    return exception_information_;
  }

#if defined(STARBOARD)
  virtual bool SendEvergreenInfoImpl() = 0;
  virtual bool InsertAnnotationImpl(const char* key, const char* value) = 0;
#endif

  virtual void HandleCrashImpl() = 0;

 private:
  // The signal handler installed at OS-level.
  static void HandleOrReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context) {
    if (handler_->HandleCrash(signo, siginfo, context)) {
      return;
    }
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, handler_->old_actions_.ActionForSignal(signo));
  }

  Signals::OldActions old_actions_ = {};
  ExceptionInformation exception_information_ = {};
  CrashpadClient::FirstChanceHandler first_chance_handler_ = nullptr;

#if defined(STARBOARD)
  EvergreenInfo evergreen_info_;
  SanitizationInformation sanitization_information_;
#endif

  static SignalHandler* handler_;

  static thread_local bool disabled_for_thread_;

  DISALLOW_COPY_AND_ASSIGN(SignalHandler);
};
SignalHandler* SignalHandler::handler_ = nullptr;
thread_local bool SignalHandler::disabled_for_thread_ = false;

// Launches a single use handler to snapshot this process.
class LaunchAtCrashHandler : public SignalHandler {
 public:
  static LaunchAtCrashHandler* Get() {
    static LaunchAtCrashHandler* instance = new LaunchAtCrashHandler();
    return instance;
  }

  bool Initialize(std::vector<std::string>* argv_in,
                  const std::vector<std::string>* envp,
                  const std::set<int>* unhandled_signals) {
    argv_strings_.swap(*argv_in);

    if (envp) {
      envp_strings_ = *envp;
      StringVectorToCStringVector(envp_strings_, &envp_);
      set_envp_ = true;
    }

    argv_strings_.push_back(FormatArgumentAddress("trace-parent-with-exception",
                                                  &GetExceptionInfo()));

#if defined(STARBOARD)
    argv_strings_.push_back("--handler-started-at-crash");
#endif

    StringVectorToCStringVector(argv_strings_, &argv_);
    return Install(unhandled_signals);
  }

#if defined(STARBOARD)
  bool SendEvergreenInfoImpl() override {
    starboard::ScopedLock scoped_lock(argv_lock_);

    bool updated = false;
    for (auto& s : argv_strings_) {
      if (s.compare(2, strlen(kEvergreenInfoKey), kEvergreenInfoKey) == 0) {
        s = FormatArgumentAddress(kEvergreenInfoKey, &GetEvergreenInfo());
        LOG(INFO) << "Updated evergreen info: " << s;
        updated = true;
        break;
      }
    }
    if (!updated) {
      std::string v =
          FormatArgumentAddress(kEvergreenInfoKey, &GetEvergreenInfo());
      argv_strings_.push_back(v);
      LOG(INFO) << "Added evergreen info: " << v;
    }

    StringVectorToCStringVector(argv_strings_, &argv_);

    return true;
  }

  bool InsertAnnotationImpl(const char* key, const char* value) override {
    starboard::ScopedLock scoped_lock(argv_lock_);

    std::string formatted_key = base::StringPrintf(kAnnotationKey, key);
    bool updated_annotation = false;
    for (auto& s : argv_strings_) {
      if (UpdateAnnotation(s, formatted_key, value)) {
        updated_annotation = true;
      }
    }

    if (!updated_annotation) {
      AddAnnotation(argv_strings_, formatted_key, value);
    }

    StringVectorToCStringVector(argv_strings_, &argv_);

    return true;
  }
#endif

  void HandleCrashImpl() override {
    ScopedPrSetPtracer set_ptracer(sys_getpid(), /* may_log= */ false);

    pid_t pid = fork();
    if (pid < 0) {
      return;
    }
    if (pid == 0) {
      if (set_envp_) {
        execve(argv_[0],
               const_cast<char* const*>(argv_.data()),
               const_cast<char* const*>(envp_.data()));
      } else {
        execv(argv_[0], const_cast<char* const*>(argv_.data()));
      }
      _exit(EXIT_FAILURE);
    }

    int status;
    waitpid(pid, &status, 0);
  }

 private:
  LaunchAtCrashHandler() = default;

  ~LaunchAtCrashHandler() = delete;

  std::vector<std::string> argv_strings_;
  std::vector<const char*> argv_;
#if defined(STARBOARD)
  // Protects access to both argv_strings_ and argv_.
  starboard::Mutex argv_lock_;
#endif
  std::vector<std::string> envp_strings_;
  std::vector<const char*> envp_;
  bool set_envp_ = false;

  DISALLOW_COPY_AND_ASSIGN(LaunchAtCrashHandler);
};

class RequestCrashDumpHandler : public SignalHandler {
 public:
  static RequestCrashDumpHandler* Get() {
    static RequestCrashDumpHandler* instance = new RequestCrashDumpHandler();
    return instance;
  }

  // pid < 0 indicates the handler pid should be determined by communicating
  // over the socket.
  // pid == 0 indicates it is not necessary to set the handler as this process'
  // ptracer. e.g. if the handler has CAP_SYS_PTRACE or if this process is in a
  // user namespace and the handler's uid matches the uid of the process that
  // created the namespace.
  // pid > 0 directly indicates what the handler's pid is expected to be, so
  // retrieving this information from the handler is not necessary.
  bool Initialize(ScopedFileHandle sock,
                  pid_t pid,
                  const std::set<int>* unhandled_signals) {
    ExceptionHandlerClient client(sock.get(), true);
    if (pid < 0) {
      ucred creds;
      if (!client.GetHandlerCredentials(&creds)) {
        return false;
      }
      pid = creds.pid;
    }
    if (pid > 0 && prctl(PR_SET_PTRACER, pid, 0, 0, 0) != 0) {
      PLOG(WARNING) << "prctl";
      // TODO(jperaza): If this call to set the ptracer failed, it might be
      // possible to try again just before a dump request, in case the
      // environment has changed. Revisit ExceptionHandlerClient::SetPtracer()
      // and consider saving the result of this call in ExceptionHandlerClient
      // or as a member in this signal handler. ExceptionHandlerClient hasn't
      // been responsible for maintaining state and a new ExceptionHandlerClient
      // has been constructed as a local whenever a client needs to communicate
      // with the handler. ExceptionHandlerClient lifetimes and ownership will
      // need to be reconsidered if it becomes responsible for state.
    }
    sock_to_handler_.reset(sock.release());
    handler_pid_ = pid;
    return Install(unhandled_signals);
  }

  bool GetHandlerSocket(int* sock, pid_t* pid) {
    if (!sock_to_handler_.is_valid()) {
      return false;
    }
    if (sock) {
      *sock = sock_to_handler_.get();
    }
    if (pid) {
      *pid = handler_pid_;
    }
    return true;
  }

#if defined(STARBOARD)
  char* GetSerializedAnnotations() {
    return serialized_annotations_.data();
  }

  bool SendEvergreenInfoImpl() override {
    ExceptionHandlerClient client(sock_to_handler_.get(), true);
    ExceptionHandlerProtocol::ClientInformation info = {};
    info.evergreen_information_address =
        FromPointerCast<VMAddress>(&GetEvergreenInfo());
    client.SendEvergreenInfo(info);
    return true;
  }

  bool InsertAnnotationImpl(const char* key, const char* value) override {
    starboard::ScopedLock scoped_lock(annotations_lock_);

    std::string key_str(key);
    std::string value_str(value);

    if (strcmp(key, "ver") == 0) {
      annotations_.set_ver(value_str);
    } else if (strcmp(key, "prod") == 0) {
      annotations_.set_prod(value_str);
    } else if (strcmp(key, "user_agent_string") == 0) {
      annotations_.set_user_agent_string(value_str);
    } else {
      (*annotations_.mutable_runtime_annotations())[key_str] = value_str;
    }

    serialized_annotations_.resize(annotations_.ByteSize());
    annotations_.SerializeToArray(serialized_annotations_.data(),
        annotations_.ByteSize());

    ExceptionHandlerClient client(sock_to_handler_.get(), true);
    ExceptionHandlerProtocol::ClientInformation info = {};
    info.serialized_annotations_address = FromPointerCast<VMAddress>(
        GetSerializedAnnotations());
    info.serialized_annotations_size = serialized_annotations_.size();
    client.SendAnnotations(info);

    return true;
  }

#endif

  void HandleCrashImpl() override {
    ExceptionHandlerProtocol::ClientInformation info = {};
    info.exception_information_address =
        FromPointerCast<VMAddress>(&GetExceptionInfo());
#if defined(STARBOARD)
    info.sanitization_information_address =
        FromPointerCast<VMAddress>(&GetSanitizationInformation());
    info.serialized_annotations_address = FromPointerCast<VMAddress>(
        GetSerializedAnnotations());
    info.serialized_annotations_size = serialized_annotations_.size();
    info.evergreen_information_address =
        FromPointerCast<VMAddress>(&GetEvergreenInfo());
    info.handler_start_type = ExceptionHandlerProtocol::kStartAtLaunch;
#endif

#if defined(OS_CHROMEOS)
    info.crash_loop_before_time = crash_loop_before_time_;
#endif

    ExceptionHandlerClient client(sock_to_handler_.get(), true);
    client.RequestCrashDump(info);
  }

#if defined(OS_CHROMEOS)
  void SetCrashLoopBefore(uint64_t crash_loop_before_time) {
    crash_loop_before_time_ = crash_loop_before_time;
  }
#endif

 private:
  RequestCrashDumpHandler() = default;

  ~RequestCrashDumpHandler() = delete;

  ScopedFileHandle sock_to_handler_;
  pid_t handler_pid_ = -1;
#if defined(STARBOARD)
  crashpad::wrapper::CrashpadAnnotations annotations_;
  std::vector<char> serialized_annotations_;

  // Protects access to both annotations_ and serialized_annotations_;
  starboard::Mutex annotations_lock_;
#endif

#if defined(OS_CHROMEOS)
  // An optional UNIX timestamp passed to us from Chrome.
  // This will pass to crashpad_handler and then to Chrome OS crash_reporter.
  // This should really be a time_t, but it's basically an opaque value (we
  // don't anything with it except pass it along).
  uint64_t crash_loop_before_time_ = 0;
#endif

  DISALLOW_COPY_AND_ASSIGN(RequestCrashDumpHandler);
};

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    bool restartable,
    bool asynchronous_start) {
  DCHECK(!asynchronous_start);

  ScopedFileHandle client_sock, handler_sock;
  if (!UnixCredentialSocket::CreateCredentialSocketpair(&client_sock,
                                                        &handler_sock)) {
    return false;
  }

  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments);

  argv.push_back(FormatArgumentInt("initial-client-fd", handler_sock.get()));
  argv.push_back("--shared-client-connection");
  if (!DoubleForkAndExec(argv, nullptr, handler_sock.get(), false, nullptr)) {
    return false;
  }

  pid_t handler_pid = -1;
  if (!IsRegularFile(base::FilePath("/proc/sys/kernel/yama/ptrace_scope"))) {
    handler_pid = 0;
  }

  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->Initialize(
      std::move(client_sock), handler_pid, &unhandled_signals_);
}

#if defined(OS_ANDROID) || defined(OS_LINUX)
// static
bool CrashpadClient::GetHandlerSocket(int* sock, pid_t* pid) {
  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->GetHandlerSocket(sock, pid);
}

bool CrashpadClient::SetHandlerSocket(ScopedFileHandle sock, pid_t pid) {
  auto signal_handler = RequestCrashDumpHandler::Get();
  return signal_handler->Initialize(std::move(sock), pid, &unhandled_signals_);
}
#endif  // OS_ANDROID || OS_LINUX

#if defined(OS_ANDROID)

bool CrashpadClient::StartJavaHandlerAtCrash(
    const std::string& class_name,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv = BuildAppProcessArgs(class_name,
                                                      database,
                                                      metrics_dir,
                                                      url,
                                                      annotations,
                                                      arguments,
                                                      kInvalidFileHandle);

  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv, env, &unhandled_signals_);
}

// static
bool CrashpadClient::StartJavaHandlerForClient(
    const std::string& class_name,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv = BuildAppProcessArgs(
      class_name, database, metrics_dir, url, annotations, arguments, socket);
  return DoubleForkAndExec(argv, env, socket, false, nullptr);
}

bool CrashpadClient::StartHandlerWithLinkerAtCrash(
    const std::string& handler_trampoline,
    const std::string& handler_library,
    bool is_64_bit,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv =
      BuildArgsToLaunchWithLinker(handler_trampoline,
                                  handler_library,
                                  is_64_bit,
                                  database,
                                  metrics_dir,
                                  url,
                                  annotations,
                                  arguments,
                                  kInvalidFileHandle);
  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv, env, &unhandled_signals_);
}

// static
bool CrashpadClient::StartHandlerWithLinkerForClient(
    const std::string& handler_trampoline,
    const std::string& handler_library,
    bool is_64_bit,
    const std::vector<std::string>* env,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv =
      BuildArgsToLaunchWithLinker(handler_trampoline,
                                  handler_library,
                                  is_64_bit,
                                  database,
                                  metrics_dir,
                                  url,
                                  annotations,
                                  arguments,
                                  socket);
  return DoubleForkAndExec(argv, env, socket, false, nullptr);
}

#endif

bool CrashpadClient::StartHandlerAtCrash(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments);

  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv, nullptr, &unhandled_signals_);
}

// static
bool CrashpadClient::StartHandlerForClient(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv = BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments);

  argv.push_back(FormatArgumentInt("initial-client-fd", socket));

  return DoubleForkAndExec(argv, nullptr, socket, true, nullptr);
}

#if defined(STARBOARD)
// static
bool CrashpadClient::SendEvergreenInfoToHandler(EvergreenInfo evergreen_info) {
  if (!SignalHandler::Get()) {
    DLOG(ERROR) << "Crashpad isn't enabled";
    return false;
  }

  return SignalHandler::Get()->SendEvergreenInfo(evergreen_info);
}

bool CrashpadClient::InsertAnnotationForHandler(
    const char* key, const char* value) {
  if (!SignalHandler::Get()) {
    DLOG(ERROR) << "Crashpad isn't enabled";
    return false;
  }

  return SignalHandler::Get()->InsertAnnotation(key, value);
}

bool CrashpadClient::SendSanitizationInformationToHandler(
    SanitizationInformation sanitization_information) {
  if (!SignalHandler::Get()) {
    DLOG(ERROR) << "Crashpad isn't enabled";
    return false;
  }

  return SignalHandler::Get()->SendSanitizationInformation(sanitization_information);
}
#endif

// static
void CrashpadClient::DumpWithoutCrash(NativeCPUContext* context) {
  if (!SignalHandler::Get()) {
    DLOG(ERROR) << "Crashpad isn't enabled";
    return;
  }

#if defined(ARCH_CPU_ARMEL)
  memset(context->uc_regspace, 0, sizeof(context->uc_regspace));
#elif defined(ARCH_CPU_ARM64)
  memset(context->uc_mcontext.__reserved,
         0,
         sizeof(context->uc_mcontext.__reserved));
#endif

  siginfo_t siginfo;
  siginfo.si_signo = Signals::kSimulatedSigno;
  siginfo.si_errno = 0;
  siginfo.si_code = 0;
  SignalHandler::Get()->HandleCrash(
      siginfo.si_signo, &siginfo, reinterpret_cast<void*>(context));
}

// static
void CrashpadClient::CrashWithoutDump(const std::string& message) {
  SignalHandler::DisableForThread();
  LOG(FATAL) << message;
}

// static
void CrashpadClient::SetFirstChanceExceptionHandler(
    FirstChanceHandler handler) {
  DCHECK(SignalHandler::Get());
  SignalHandler::Get()->SetFirstChanceHandler(handler);
}

void CrashpadClient::SetUnhandledSignals(const std::set<int>& signals) {
  DCHECK(!SignalHandler::Get());
  unhandled_signals_ = signals;
}

#if defined(OS_CHROMEOS)
// static
void CrashpadClient::SetCrashLoopBefore(uint64_t crash_loop_before_time) {
  auto request_crash_dump_handler = RequestCrashDumpHandler::Get();
  request_crash_dump_handler->SetCrashLoopBefore(crash_loop_before_time);
}
#endif

}  // namespace crashpad
