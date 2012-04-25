// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/test/test_suite.h"
#include "testing/android/jni/chrome_native_test_activity_jni.h"
#include "gtest/gtest.h"

// GTest's main function.
extern int main(int argc, char** argv);

namespace {

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

void ParseArgsFromCommandLineFile(const FilePath& internal_data_path,
                                  std::vector<std::string>* args) {
  static const char kCommandLineFile[] = "chrome-native-tests-command-line";
  FilePath command_line(internal_data_path.Append(FilePath(kCommandLineFile)));
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

void LibraryLoadedOnMainThread(JNIEnv* env) {
  static const char* const kInitialArgv[] = { "ChromeTestActivity" };

  {
    // We need a test suite to be created before we do any tracing or
    // logging: it creates a global at_exit_manager and initializes
    // internal gtest data structures based on the command line.
    // It needs to be scoped as it also resets the CommandLine.
    std::vector<std::string> args;
    FilePath path("/data/user/0/org.chromium.native_test/files/");
    ParseArgsFromCommandLineFile(path, &args);
    std::vector<char*> argv;
    ArgsToArgv(args, &argv);
    base::TestSuite test_suite(argv.size(), &argv[0]);
  }

  CommandLine::Init(arraysize(kInitialArgv), kInitialArgv);

  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE,
                       logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count
  VLOG(0) << "Chromium logging enabled: level = " << logging::GetMinLogLevel()
          << ", default verbosity = " << logging::GetVlogVerbosity();
  base::android::RegisterPathUtils(env);
}

}  // namespace

// This method is called on a separate java thread so that we won't trigger
// an ANR.
static void RunTests(JNIEnv* env, jobject obj, jstring jfiles_dir) {
  FilePath files_dir(base::android::ConvertJavaStringToUTF8(env, jfiles_dir));
  // A few options, such "--gtest_list_tests", will just use printf directly
  // and won't use the "AndroidLogPrinter". Redirect stdout to a known file.
  FilePath stdout_path(files_dir.Append(FilePath("stdout.txt")));
  freopen(stdout_path.value().c_str(), "w", stdout);

  std::vector<std::string> args;
  ParseArgsFromCommandLineFile(files_dir, &args);

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
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterNativesImpl(env)) {
    return -1;
  }
  LibraryLoadedOnMainThread(env);
  return JNI_VERSION_1_4;
}
