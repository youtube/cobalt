// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/test/multiprocess_test.h"
#include "base/win/scoped_process_information.h"
#include "testing/multiprocess_func_list.h"

class ScopedProcessInformationTest : public base::MultiProcessTest {
 protected:
  void DoCreateProcess(const std::string& main_id,
                       PROCESS_INFORMATION* process_handle);
};

MULTIPROCESS_TEST_MAIN(ReturnSeven) {
  return 7;
}

MULTIPROCESS_TEST_MAIN(ReturnNine) {
  return 9;
}

void ScopedProcessInformationTest::DoCreateProcess(
    const std::string& main_id, PROCESS_INFORMATION* process_handle) {
  std::wstring cmd_line =
      this->MakeCmdLine(main_id, false).GetCommandLineString();
  STARTUPINFO startup_info = {};
  startup_info.cb = sizeof(startup_info);

  EXPECT_TRUE(::CreateProcess(NULL,
                              const_cast<wchar_t*>(cmd_line.c_str()),
                              NULL, NULL, false, 0, NULL, NULL,
                              &startup_info, process_handle));
}

TEST_F(ScopedProcessInformationTest, TakeProcess) {
  base::win::ScopedProcessInformation process_info;
  DoCreateProcess("ReturnSeven", process_info.Receive());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(process_info.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(7, exit_code);
  ASSERT_TRUE(process_info.IsValid());
  ASSERT_EQ(0u, process_info.process_id());
  ASSERT_TRUE(process_info.process_handle() == NULL);
  ASSERT_NE(0u, process_info.thread_id());
  ASSERT_FALSE(process_info.thread_handle() == NULL);
}

TEST_F(ScopedProcessInformationTest, TakeThread) {
  base::win::ScopedProcessInformation process_info;
  DoCreateProcess("ReturnSeven", process_info.Receive());
  ASSERT_TRUE(::CloseHandle(process_info.TakeThreadHandle()));
  ASSERT_TRUE(process_info.IsValid());
  ASSERT_NE(0u, process_info.process_id());
  ASSERT_FALSE(process_info.process_handle() == NULL);
  ASSERT_EQ(0u, process_info.thread_id());
  ASSERT_TRUE(process_info.thread_handle() == NULL);
}

TEST_F(ScopedProcessInformationTest, TakeBoth) {
  base::win::ScopedProcessInformation process_info;
  DoCreateProcess("ReturnSeven", process_info.Receive());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(process_info.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(7, exit_code);
  ASSERT_TRUE(::CloseHandle(process_info.TakeThreadHandle()));
  ASSERT_FALSE(process_info.IsValid());
  ASSERT_EQ(0u, process_info.process_id());
  ASSERT_TRUE(process_info.process_handle() == NULL);
  ASSERT_EQ(0u, process_info.thread_id());
  ASSERT_TRUE(process_info.thread_handle() == NULL);
}

TEST_F(ScopedProcessInformationTest, TakeNothing) {
  base::win::ScopedProcessInformation process_info;
  DoCreateProcess("ReturnSeven", process_info.Receive());
  ASSERT_TRUE(process_info.IsValid());
  ASSERT_NE(0u, process_info.thread_id());
  ASSERT_FALSE(process_info.thread_handle() == NULL);
  ASSERT_NE(0u, process_info.process_id());
  ASSERT_FALSE(process_info.process_handle() == NULL);
}

TEST_F(ScopedProcessInformationTest, TakeWholeStruct) {
  base::win::ScopedProcessInformation process_info;
  DoCreateProcess("ReturnSeven", process_info.Receive());
  base::win::ScopedProcessInformation other;
  *other.Receive() = process_info.Take();

  ASSERT_FALSE(process_info.IsValid());
  ASSERT_EQ(0u, process_info.process_id());
  ASSERT_TRUE(process_info.process_handle() == NULL);
  ASSERT_EQ(0u, process_info.thread_id());
  ASSERT_TRUE(process_info.thread_handle() == NULL);

  // Validate that what was taken is good.
  ASSERT_NE(0u, other.thread_id());
  ASSERT_NE(0u, other.process_id());
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(other.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(7, exit_code);
  ASSERT_TRUE(::CloseHandle(other.TakeThreadHandle()));
}

TEST_F(ScopedProcessInformationTest, Duplicate) {
  base::win::ScopedProcessInformation process_info;
  DoCreateProcess("ReturnSeven", process_info.Receive());
  base::win::ScopedProcessInformation duplicate;
  duplicate.DuplicateFrom(process_info);

  ASSERT_TRUE(process_info.IsValid());
  ASSERT_NE(0u, process_info.process_id());
  ASSERT_EQ(duplicate.process_id(), process_info.process_id());
  ASSERT_NE(0u, process_info.thread_id());
  ASSERT_EQ(duplicate.thread_id(), process_info.thread_id());

  // Validate that we have separate handles that are good.
  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(process_info.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(7, exit_code);

  exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(duplicate.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(7, exit_code);

  ASSERT_TRUE(::CloseHandle(process_info.TakeThreadHandle()));
  ASSERT_TRUE(::CloseHandle(duplicate.TakeThreadHandle()));
}

TEST_F(ScopedProcessInformationTest, Swap) {
  base::win::ScopedProcessInformation seven_to_nine_info;
  DoCreateProcess("ReturnSeven", seven_to_nine_info.Receive());
  base::win::ScopedProcessInformation nine_to_seven_info;
  DoCreateProcess("ReturnNine", nine_to_seven_info.Receive());

  HANDLE seven_process = seven_to_nine_info.process_handle();
  DWORD seven_process_id = seven_to_nine_info.process_id();
  HANDLE seven_thread = seven_to_nine_info.thread_handle();
  DWORD seven_thread_id = seven_to_nine_info.thread_id();
  HANDLE nine_process = nine_to_seven_info.process_handle();
  DWORD nine_process_id = nine_to_seven_info.process_id();
  HANDLE nine_thread = nine_to_seven_info.thread_handle();
  DWORD nine_thread_id = nine_to_seven_info.thread_id();

  seven_to_nine_info.Swap(&nine_to_seven_info);

  ASSERT_EQ(seven_process, nine_to_seven_info.process_handle());
  ASSERT_EQ(seven_process_id, nine_to_seven_info.process_id());
  ASSERT_EQ(seven_thread, nine_to_seven_info.thread_handle());
  ASSERT_EQ(seven_thread_id, nine_to_seven_info.thread_id());
  ASSERT_EQ(nine_process, seven_to_nine_info.process_handle());
  ASSERT_EQ(nine_process_id, seven_to_nine_info.process_id());
  ASSERT_EQ(nine_thread, seven_to_nine_info.thread_handle());
  ASSERT_EQ(nine_thread_id, seven_to_nine_info.thread_id());

  int exit_code = 0;
  ASSERT_TRUE(base::WaitForExitCode(seven_to_nine_info.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(9, exit_code);

  ASSERT_TRUE(base::WaitForExitCode(nine_to_seven_info.TakeProcessHandle(),
                                    &exit_code));
  ASSERT_EQ(7, exit_code);

}

TEST_F(ScopedProcessInformationTest, InitiallyInvalid) {
  base::win::ScopedProcessInformation process_info;
  ASSERT_FALSE(process_info.IsValid());
}
