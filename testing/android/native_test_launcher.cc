// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to logcat) markers identifying the
// START/END/CRASH of the test suite, FAILURE/SUCCESS of individual tests etc.
// These markers are read by the test runner script to generate test results.
// It injects an event listener in gtest to detect various test stages and
// installs signal handlers to detect crashes.

#include <android/log.h>
#include <signal.h>
#include <stdio.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "gtest/gtest.h"
#include "testing/android/jni/chrome_native_test_activity_jni.h"

#if defined(REGISTER_NET_UNITTESTS_JNI)
#include "net/android/net_jni_registrar.h"
#endif

#if defined(REGISTER_CONTENT_UNITTESTS_JNI)
#include "content/app/android/content_jni_registrar.h"
#endif

// The main function of the program to be wrapped as a test apk.
extern int main(int argc, char** argv);

namespace {

// The list of signals which are considered to be crashes.
const int kExceptionSignals[] = {
  SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, -1
};

struct sigaction g_old_sa[NSIG];

// This function runs in a compromised context. It should not allocate memory.
void SignalHandler(int sig, siginfo_t *info, void *reserved)
{
  // Output the crash marker.
  __android_log_write(ANDROID_LOG_ERROR, "chromium", "[ CRASHED      ]");
  g_old_sa[sig].sa_sigaction(sig, info, reserved);
}

void InstallHandlers() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_SIGINFO;

  for (unsigned int i = 0; kExceptionSignals[i] != -1; ++i) {
    sigaction(kExceptionSignals[i], &sa, &g_old_sa[kExceptionSignals[i]]);
  }
}

void ParseArgsFromString(const std::string& command_line,
                         std::vector<std::string>* args) {
  StringTokenizer tokenizer(command_line, kWhitespaceASCII);
  tokenizer.set_quote_chars("\"");
  while (tokenizer.GetNext()) {
    std::string token;
    RemoveChars(tokenizer.token(), "\"", &token);
    args->push_back(token);
  }
}

void ParseArgsFromCommandLineFile(std::vector<std::string>* args) {
  // The test runner script writes the command line file in
  // "/data/local/tmp".
  static const char kCommandLineFilePath[] =
      "/data/local/tmp/chrome-native-tests-command-line";
  FilePath command_line(kCommandLineFilePath);
  std::string command_line_string;
  if (file_util::ReadFileToString(command_line, &command_line_string)) {
    ParseArgsFromString(command_line_string, args);
  }
}

void ArgsToArgv(const std::vector<std::string>& args,
                std::vector<char*>* argv) {
  // We need to pass in a non-const char**.
  int argc = args.size();
  argv->resize(argc);
  for (int i = 0; i < argc; ++i)
    (*argv)[i] = const_cast<char*>(args[i].c_str());
}

// As we are the native side of an Android app, we don't have any 'console', so
// gtest's standard output goes nowhere.
// Instead, we inject an "EventListener" in gtest and then we print the results
// using LOG, which goes to adb logcat.
class AndroidLogPrinter : public ::testing::EmptyTestEventListener {
 public:
  void Init(int* argc, char** argv);

  // EmptyTestEventListener
  virtual void OnTestProgramStart(
      const ::testing::UnitTest& unit_test) OVERRIDE;
  virtual void OnTestStart(const ::testing::TestInfo& test_info) OVERRIDE;
  virtual void OnTestPartResult(
      const ::testing::TestPartResult& test_part_result) OVERRIDE;
  virtual void OnTestEnd(const ::testing::TestInfo& test_info) OVERRIDE;
  virtual void OnTestProgramEnd(const ::testing::UnitTest& unit_test) OVERRIDE;
};

void AndroidLogPrinter::Init(int* argc, char** argv) {
  // InitGoogleTest must be called befure we add ourselves as a listener.
  ::testing::InitGoogleTest(argc, argv);
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  // Adds a listener to the end.  Google Test takes the ownership.
  listeners.Append(this);
}

void AndroidLogPrinter::OnTestProgramStart(
    const ::testing::UnitTest& unit_test) {
  std::string msg = StringPrintf("[ START      ] %d",
                                 unit_test.test_to_run_count());
  LOG(ERROR) << msg;
}

void AndroidLogPrinter::OnTestStart(const ::testing::TestInfo& test_info) {
  std::string msg = StringPrintf("[ RUN      ] %s.%s",
                                 test_info.test_case_name(), test_info.name());
  LOG(ERROR) << msg;
}

void AndroidLogPrinter::OnTestPartResult(
    const ::testing::TestPartResult& test_part_result) {
  std::string msg = StringPrintf(
      "%s in %s:%d\n%s\n",
      test_part_result.failed() ? "*** Failure" : "Success",
      test_part_result.file_name(),
      test_part_result.line_number(),
      test_part_result.summary());
  LOG(ERROR) << msg;
}

void AndroidLogPrinter::OnTestEnd(const ::testing::TestInfo& test_info) {
  std::string msg = StringPrintf("%s %s.%s",
      test_info.result()->Failed() ? "[  FAILED  ]" : "[       OK ]",
      test_info.test_case_name(), test_info.name());
  LOG(ERROR) << msg;
}

void AndroidLogPrinter::OnTestProgramEnd(
    const ::testing::UnitTest& unit_test) {
  std::string msg = StringPrintf("[ END      ] %d",
         unit_test.successful_test_count());
  LOG(ERROR) << msg;
}

}  // namespace

// This method is called on a separate java thread so that we won't trigger
// an ANR.
static void RunTests(JNIEnv* env,
                     jobject obj,
                     jstring jfiles_dir,
                     jobject app_context) {
  base::AtExitManager exit_manager;

  static const char* const kInitialArgv[] = { "ChromeTestActivity" };
  CommandLine::Init(arraysize(kInitialArgv), kInitialArgv);

  // Set the application context in base.
  base::android::ScopedJavaLocalRef<jobject> scoped_context(
      env, env->NewLocalRef(app_context));
  base::android::InitApplicationContext(scoped_context);

  base::android::RegisterJni(env);

  // A unittest target may define these macros to register JNI dependencies.
  // These should only be defined if the target also depends on the
  // corresponding registrar.
#if defined(REGISTER_NET_UNITTESTS_JNI)
  net::android::RegisterJni(env);
#endif

#if defined(REGISTER_CONTENT_UNITTESTS_JNI)
  content::android::RegisterJni(env);
#endif

  FilePath files_dir(base::android::ConvertJavaStringToUTF8(env, jfiles_dir));
  // A few options, such "--gtest_list_tests", will just use printf directly
  // and won't use the "AndroidLogPrinter". Redirect stdout to a known file.
  FilePath stdout_path(files_dir.Append(FilePath("stdout.txt")));
  freopen(stdout_path.value().c_str(), "w", stdout);

  std::vector<std::string> args;
  ParseArgsFromCommandLineFile(&args);

  // We need to pass in a non-const char**.
  std::vector<char*> argv;
  ArgsToArgv(args, &argv);

  int argc = argv.size();
  // This object is owned by gtest.
  AndroidLogPrinter* log = new AndroidLogPrinter();
  log->Init(&argc, &argv[0]);

  main(argc, &argv[0]);
}

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // Install signal handlers to detect crashes.
  InstallHandlers();

  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterNativesImpl(env)) {
    return -1;
  }

  return JNI_VERSION_1_4;
}
