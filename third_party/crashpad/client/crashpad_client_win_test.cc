// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "test/test_paths.h"
#include "test/scoped_temp_dir.h"
#include "test/win/win_multiprocess.h"
#include "test/win/win_multiprocess_with_temp_dir.h"
#include "util/win/scoped_handle.h"
#include "util/win/termination_codes.h"

namespace crashpad {
namespace test {
namespace {

void StartAndUseHandler(const base::FilePath& temp_dir) {
  std::vector<char> content_path(kSbFileMaxPath + 1);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                           content_path.size()));
  base::FilePath handler_path = base::FilePath(base::UTF8ToUTF16(content_path.data()))
      .Append(FILE_PATH_LITERAL("crashpad_handler.exe"));

  CrashpadClient client;
  ASSERT_TRUE(client.StartHandler(handler_path,
                                  temp_dir,
                                  base::FilePath(),
                                  "",
                                  std::map<std::string, std::string>(),
                                  std::vector<std::string>(),
                                  true,
                                  true));
  ASSERT_TRUE(client.WaitForHandlerStart(INFINITE));
}

class StartWithInvalidHandles final : public WinMultiprocessWithTempDir {
 public:
  StartWithInvalidHandles() : WinMultiprocessWithTempDir() {}
  ~StartWithInvalidHandles() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
    SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);

    StartAndUseHandler(GetTempDirPath());

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
    SetStdHandle(STD_ERROR_HANDLE, original_stderr);
  }
};

// Second instance of this process crashes in Starboard/UWP
// configuration. Multiprocess tests are DISABLED.
#if defined(STARBOARD) && defined(WINDOWS_UWP)
#define MAYBE_StartWithInvalidHandles \
  DISABLED_StartWithInvalidHandles
#else
#define MAYBE_StartWithInvalidHandles StartWithInvalidHandles
#endif
TEST(CrashpadClient, MAYBE_StartWithInvalidHandles) {
  WinMultiprocessWithTempDir::Run<StartWithInvalidHandles>();
}

class StartWithSameStdoutStderr final : public WinMultiprocessWithTempDir {
 public:
  StartWithSameStdoutStderr() : WinMultiprocessWithTempDir() {}
  ~StartWithSameStdoutStderr() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, original_stderr);

    StartAndUseHandler(GetTempDirPath());

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
  }
};

#if defined(STARBOARD) && defined(WINDOWS_UWP)
#define MAYBE_StartWithSameStdoutStderr \
  DISABLED_StartWithSameStdoutStderr
#else
#define MAYBE_StartWithSameStdoutStderr StartWithSameStdoutStderr
#endif
TEST(CrashpadClient, MAYBE_StartWithSameStdoutStderr) {
  WinMultiprocessWithTempDir::Run<StartWithSameStdoutStderr>();
}

void StartAndUseBrokenHandler(CrashpadClient* client) {
  ScopedTempDir temp_dir;
  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("fake_handler_that_crashes_at_startup.exe"));
  ASSERT_TRUE(client->StartHandler(handler_path,
                                  temp_dir.path(),
                                  base::FilePath(),
                                  "",
                                  std::map<std::string, std::string>(),
                                  std::vector<std::string>(),
                                  false,
                                  true));
}

class HandlerLaunchFailureCrash : public WinMultiprocess {
 public:
  HandlerLaunchFailureCrash() : WinMultiprocess() {}

 private:
  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(crashpad::kTerminationCodeCrashNoDump);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    StartAndUseBrokenHandler(&client);
    __debugbreak();
    exit(0);
  }
};

#if defined(ADDRESS_SANITIZER) || ( defined(STARBOARD) && defined(WINDOWS_UWP) )
// https://crbug.com/845011
#define MAYBE_HandlerLaunchFailureCrash DISABLED_HandlerLaunchFailureCrash
#else
#define MAYBE_HandlerLaunchFailureCrash HandlerLaunchFailureCrash
#endif
TEST(CrashpadClient, MAYBE_HandlerLaunchFailureCrash) {
  WinMultiprocess::Run<HandlerLaunchFailureCrash>();
}

class HandlerLaunchFailureDumpAndCrash : public WinMultiprocess {
 public:
  HandlerLaunchFailureDumpAndCrash() : WinMultiprocess() {}

 private:
  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(crashpad::kTerminationCodeCrashNoDump);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    StartAndUseBrokenHandler(&client);
    // We don't need to fill this out as we're only checking that we're
    // terminated with the correct failure code.
    EXCEPTION_POINTERS info = {};
    client.DumpAndCrash(&info);
    exit(0);
  }
};

#if defined(ADDRESS_SANITIZER) || ( defined(STARBOARD) && defined(WINDOWS_UWP) )
// https://crbug.com/845011
#define MAYBE_HandlerLaunchFailureDumpAndCrash \
  DISABLED_HandlerLaunchFailureDumpAndCrash
#else
#define MAYBE_HandlerLaunchFailureDumpAndCrash HandlerLaunchFailureDumpAndCrash
#endif
TEST(CrashpadClient, MAYBE_HandlerLaunchFailureDumpAndCrash) {
  WinMultiprocess::Run<HandlerLaunchFailureDumpAndCrash>();
}

class HandlerLaunchFailureDumpWithoutCrash : public WinMultiprocess {
 public:
  HandlerLaunchFailureDumpWithoutCrash() : WinMultiprocess() {}

 private:
  void WinMultiprocessParent() override {
    // DumpWithoutCrash() normally blocks indefinitely. There's no return value,
    // but confirm that it exits cleanly because it'll return right away if the
    // handler didn't start.
    SetExpectedChildExitCode(0);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    StartAndUseBrokenHandler(&client);
    // We don't need to fill this out as we're only checking that we're
    // terminated with the correct failure code.
    CONTEXT context = {};
    client.DumpWithoutCrash(context);
    exit(0);
  }
};

#if defined(STARBOARD) && defined(WINDOWS_UWP)
#define MAYBE_HandlerLaunchFailureDumpWithoutCrash \
  DISABLED_HandlerLaunchFailureDumpWithoutCrash
#else
#define MAYBE_HandlerLaunchFailureDumpWithoutCrash \
  HandlerLaunchFailureDumpWithoutCrash
#endif
TEST(CrashpadClient, MAYBE_HandlerLaunchFailureDumpWithoutCrash) {
  WinMultiprocess::Run<HandlerLaunchFailureDumpWithoutCrash>();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
