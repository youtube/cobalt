// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"

#include <excpt.h>
#include <ktmw32.h>
#include <ntstatus.h>
#include <windows.h>

#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/hooking_dll.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

// PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY.EnableUserShadowStackStrictMode
// is only defined starting 10.0.20226.0.
#define CET_STRICT_MODE_MASK (1 << 4)
// PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY.CetDynamicApisOutOfProcOnly is
// only defined starting 10.0.20226.0.
#define CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_MASK (1 << 8)

namespace {

//------------------------------------------------------------------------------
// NonSystemFont test helper function.
//
// 1. Pick font file and set up sandbox to allow read access to it.
// 2. Trigger test child process (with or without mitigation enabled).
//------------------------------------------------------------------------------
void TestWin10NonSystemFont(bool is_success_test) {
  base::FilePath font_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_WINDOWS_FONTS, &font_path));
  // Arial font should always be available
  font_path = font_path.Append(L"arial.ttf");

  sandbox::TestRunner runner;
  EXPECT_TRUE(runner.AddFsRule(sandbox::Semantics::kFilesAllowReadonly,
                               font_path.value().c_str()));

  if (!is_success_test) {
    sandbox::TargetPolicy* policy = runner.GetPolicy();
    // Turn on the non-system font disable mitigation.
    EXPECT_EQ(policy->GetConfig()->SetProcessMitigations(
                  sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE),
              sandbox::SBOX_ALL_OK);
  }

  std::wstring test_command = L"CheckWin10FontLoad \"";
  test_command += font_path.value().c_str();
  test_command += L"\"";

  EXPECT_EQ((is_success_test ? sandbox::SBOX_TEST_SUCCEEDED
                             : sandbox::SBOX_TEST_FAILED),
            runner.RunTest(test_command.c_str()));
}

//------------------------------------------------------------------------------
// ForceMsSigned test helper function.
// - LoadLibrary fails with ERROR_INVALID_IMAGE_HASH if this mitigation is
//   enabled and the target is not appropriately signed.
// - Acquire the global g_hooking_dll_mutex mutex before calling
//   (as we meddle with a shared system resource).
// - Note: Do not use ASSERTs in this function, as a global mutex is held.
//
// Trigger test child process (with or without mitigation enabled).
//------------------------------------------------------------------------------
void TestWin10MsSigned(int expected,
                       bool enable_mitigation,
                       bool delayed,
                       bool use_ms_signed_binary,
                       bool add_dll_permission,
                       bool add_directory_permission) {
  sandbox::TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  if (enable_mitigation) {
    // Enable the ForceMsSigned mitigation.
    if (delayed) {
      EXPECT_EQ(config->SetDelayedProcessMitigations(
                    sandbox::MITIGATION_FORCE_MS_SIGNED_BINS),
                sandbox::SBOX_ALL_OK);
    } else {
      EXPECT_EQ(config->SetProcessMitigations(
                    sandbox::MITIGATION_FORCE_MS_SIGNED_BINS),
                sandbox::SBOX_ALL_OK);
    }
  }

  // Choose the appropriate DLL and make sure the sandbox allows access to it.
  base::FilePath dll_path;
  if (use_ms_signed_binary) {
    EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &dll_path));
    dll_path = dll_path.Append(L"gdi32.dll");
  } else {
    EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &dll_path));
    dll_path = dll_path.Append(hooking_dll::g_hook_dll_file);

    if (add_dll_permission) {
      EXPECT_EQ(sandbox::SBOX_ALL_OK,
                config->AddRule(sandbox::SubSystem::kSignedBinary,
                                sandbox::Semantics::kSignedAllowLoad,
                                dll_path.value().c_str()));
    }
    if (add_directory_permission) {
      base::FilePath exe_path;
      EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
      EXPECT_EQ(sandbox::SBOX_ALL_OK,
                config->AddRule(
                    sandbox::SubSystem::kSignedBinary,
                    sandbox::Semantics::kSignedAllowLoad,
                    exe_path.DirName().AppendASCII("*.dll").value().c_str()));
    }
  }
  EXPECT_TRUE(runner.AddFsRule(sandbox::Semantics::kFilesAllowReadonly,
                               dll_path.value().c_str()));
  // Set up test string.
  std::wstring test = L"TestDllLoad \"";
  test += dll_path.value().c_str();
  test += L"\"";

  // Note: ERROR_INVALID_IMAGE_HASH is being displayed in a system pop-up when
  //       the DLL load is attempted for delayed mitigations, but the value
  //       returned from the test process itself is SBOX_TEST_FAILED.
  EXPECT_EQ(expected, runner.RunTest(test.c_str()));
}

}  // namespace

namespace sandbox {

//------------------------------------------------------------------------------
// Exported functions called by child test processes.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Common test function for checking that a policy was enabled.
// - Use enum TestPolicy defined in integration_tests_common.h to specify which
//   policy to check - passed as arg1.
//------------------------------------------------------------------------------
SBOX_TESTS_COMMAND int CheckPolicy(int argc, wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_INVALID_PARAMETER;
  int test = ::_wtoi(argv[0]);
  if (!test)
    return SBOX_TEST_INVALID_PARAMETER;

  switch (test) {
    //--------------------------------------------------
    // MITIGATION_DEP
    // MITIGATION_DEP_NO_ATL_THUNK
    //--------------------------------------------------
    case (TESTPOLICY_DEP): {
#if !defined(_WIN64)
      // DEP - always enabled on 64-bit.
      PROCESS_MITIGATION_DEP_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(), ProcessDEPPolicy,
                                        &policy, sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.Enable || !policy.Permanent)
        return SBOX_TEST_FAILED;
#endif  // !defined(_WIN64)
      break;
    }
    //--------------------------------------------------
    // MITIGATION_RELOCATE_IMAGE
    // MITIGATION_RELOCATE_IMAGE_REQUIRED
    //--------------------------------------------------
    case (TESTPOLICY_ASLR): {
      PROCESS_MITIGATION_ASLR_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessASLRPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.EnableForceRelocateImages || !policy.DisallowStrippedImages)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_STRICT_HANDLE_CHECKS
    //--------------------------------------------------
    case (TESTPOLICY_STRICTHANDLE): {
      PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessStrictHandleCheckPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.RaiseExceptionOnInvalidHandleReference ||
          !policy.HandleExceptionsPermanentlyEnabled) {
        return SBOX_TEST_FAILED;
      }

      break;
    }
    //--------------------------------------------------
    // MITIGATION_WIN32K_DISABLE
    //--------------------------------------------------
    case (TESTPOLICY_WIN32K): {
      PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessSystemCallDisablePolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.DisallowWin32kSystemCalls)
        return SBOX_TEST_FIRST_ERROR;

      // Check if we can call a Win32k API. Fail if it succeeds.
      if (::GetDC(nullptr))
        return SBOX_TEST_SECOND_ERROR;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_EXTENSION_POINT_DISABLE
    //--------------------------------------------------
    case (TESTPOLICY_EXTENSIONPOINT): {
      PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessExtensionPointDisablePolicy,
                                        &policy, sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.DisableExtensionPoints)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_DYNAMIC_CODE_DISABLE
    //--------------------------------------------------
    case (TESTPOLICY_DYNAMICCODE): {
      PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessDynamicCodePolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.ProhibitDynamicCode)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_NONSYSTEM_FONT_DISABLE
    //--------------------------------------------------
    case (TESTPOLICY_NONSYSFONT): {
      PROCESS_MITIGATION_FONT_DISABLE_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessFontDisablePolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.DisableNonSystemFonts)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_FORCE_MS_SIGNED_BINS
    //--------------------------------------------------
    case (TESTPOLICY_MSSIGNED): {
      PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessSignaturePolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.MicrosoftSignedOnly)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_IMAGE_LOAD_NO_REMOTE
    //--------------------------------------------------
    case (TESTPOLICY_LOADNOREMOTE): {
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessImageLoadPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.NoRemoteImages)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
    //--------------------------------------------------
    case (TESTPOLICY_LOADNOLOW): {
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessImageLoadPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.NoLowMandatoryLabelImages)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_DYNAMIC_CODE_DISABLE_WITH_OPT_OUT
    //--------------------------------------------------
    case (TESTPOLICY_DYNAMICCODEOPTOUT): {
      PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessDynamicCodePolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.ProhibitDynamicCode || !policy.AllowThreadOptOut)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_IMAGE_LOAD_PREFER_SYS32
    //--------------------------------------------------
    case (TESTPOLICY_LOADPREFERSYS32): {
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessImageLoadPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.PreferSystem32Images)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION
    //--------------------------------------------------
    case (TESTPOLICY_RESTRICTINDIRECTBRANCHPREDICTION): {
      // TODO(pennymac): No Policy defines available yet!
      // Can't use GetProcessMitigationPolicy() API to check if enabled at this
      // time.  If the creation of THIS process succeeded, then the call to
      // UpdateProcThreadAttribute() with this mitigation succeeded.
      break;
    }
    //--------------------------------------------------
    // MITIGATION_CET_DISABLED
    //--------------------------------------------------
    case (TESTPOLICY_CETDISABLED): {
      PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessUserShadowStackPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      // We wish to disable the policy.
      if (policy.EnableUserShadowStack)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_CET_ALLOW_DYNAMIC_APIS
    //--------------------------------------------------
    case (TESTPOLICY_CETDYNAMICAPIS): {
      PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessUserShadowStackPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }

      // CET should be enabled (we only run this test if CET is available).
      if (!policy.EnableUserShadowStack)
        return SBOX_TEST_FIRST_RESULT;

      // We wish to disable the setting.
      // (Once win sdk is updated to 10.0.20226.0
      // we can use CetDynamicApisOutOfProcOnly here.)
      if (policy.Flags & CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_MASK)
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_CET_STRICT_MODE
    //--------------------------------------------------
    case (TESTPOLICY_CETSTRICT): {
      PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessUserShadowStackPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }

      // CET should be enabled (we only run this test if CET is available).
      if (!policy.EnableUserShadowStack)
        return SBOX_TEST_FIRST_ERROR;

      // We wish to enable the setting.
      // (Once win sdk is updated to 10.0.20226.0 we can use
      // EnableUserShadowStackStrictMode here.)
      if (!(policy.Flags & CET_STRICT_MODE_MASK))
        return SBOX_TEST_FAILED;

      break;
    }
    //--------------------------------------------------
    // MITIGATION_KTM_COMPONENT_FILTER
    //--------------------------------------------------
    case (TESTPOLICY_KTMCOMPONENTFILTER): {
      // If the mitigation is enabled, creating a KTM should fail.
      SECURITY_ATTRIBUTES tm_attributes = {0};
      tm_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
      tm_attributes.lpSecurityDescriptor = nullptr;
      tm_attributes.bInheritHandle = false;
      base::win::ScopedHandle ktm;
      ktm.Set(CreateTransactionManager(&tm_attributes, nullptr,
                                       TRANSACTION_MANAGER_VOLATILE,
                                       TRANSACTION_MANAGER_COMMIT_DEFAULT));
      if (ktm.IsValid() || ::GetLastError() != ERROR_ACCESS_DENIED)
        return SBOX_TEST_FAILED;

      break;
    }

    case (TESTPOLICY_PREANDPOSTSTARTUP): {
      // Both policies should be set now.
      PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
      if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                        ProcessImageLoadPolicy, &policy,
                                        sizeof(policy))) {
        return SBOX_TEST_NOT_FOUND;
      }
      if (!policy.NoLowMandatoryLabelImages)
        return SBOX_TEST_FAILED;

      if (!policy.PreferSystem32Images)
        return SBOX_TEST_FAILED;

      break;
    }

    default:
      return SBOX_TEST_INVALID_PARAMETER;
  }

  return SBOX_TEST_SUCCEEDED;
}

// ForceMsSigned tests:
// Try to load the DLL given in arg1.
SBOX_TESTS_COMMAND int TestDllLoad(int argc, wchar_t** argv) {
  if (argc < 1 || !argv[0])
    return SBOX_TEST_INVALID_PARAMETER;

  std::wstring dll = argv[0];
  base::ScopedNativeLibrary test_dll((base::FilePath(dll)));
  if (test_dll.is_valid())
    return SBOX_TEST_SUCCEEDED;

  // Note: GetLastError() does not get an accurate failure code
  //       at this point.
  return SBOX_TEST_FAILED;
}

// This test attempts a non-system font load.
//
// Arg1: Full path to font file to try loading.
SBOX_TESTS_COMMAND int CheckWin10FontLoad(int argc, wchar_t** argv) {
  if (argc < 1)
    return SBOX_TEST_INVALID_PARAMETER;

  // Open font file passed in as an argument.
  base::File file(base::FilePath(argv[0]),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    // Failed to open the font file passed in.
    return SBOX_TEST_NOT_FOUND;

  std::vector<char> font_data;
  int64_t len = file.GetLength();
  if (len < 0)
    return SBOX_TEST_NOT_FOUND;
  font_data.resize(len);

  int read = file.Read(0, &font_data[0], base::checked_cast<int>(len));
  file.Close();

  if (read != len)
    return SBOX_TEST_NOT_FOUND;

  DWORD font_count = 0;
  HANDLE font_handle = ::AddFontMemResourceEx(
      &font_data[0], static_cast<DWORD>(font_data.size()), nullptr,
      &font_count);

  if (font_handle) {
    ::RemoveFontMemResourceEx(font_handle);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_FAILED;
}

// Common helper test for CreateProcess.
// Arg1: The process commandline to create.
// Arg2: Will process end on its own:
//  - "true" for a process expected to finish on its own (wait for it).
//  - "false" for a process that will require terminating.
// Arg3: [OPTIONAL] Expected or forced exit code:
//  - If arg2 is "true", this is the expected exit code (default 0).
//  - If arg2 is "false", terminate the process with this exit code (default 0).
//
// ***Make sure you've enabled basic process creation in the
// test sandbox settings via:
// sandbox::TargetPolicy::SetJobLevel(),
// sandbox::TargetPolicy::SetTokenLevel(),
// and TestRunner::SetDisableCsrss().
SBOX_TESTS_COMMAND int TestChildProcess(int argc, wchar_t** argv) {
  if (argc < 2 || argc > 3)
    return SBOX_TEST_INVALID_PARAMETER;

  bool process_finishes = true;
  std::wstring arg2 = argv[1];
  if (arg2.compare(L"false") == 0)
    process_finishes = false;

  int desired_exit_code = 0;
  if (argc == 3) {
    desired_exit_code = wcstoul(argv[2], nullptr, 0);
  }

  std::wstring cmd = argv[0];
  base::LaunchOptions options = base::LaunchOptionsForTest();
  base::Process setup_proc = base::LaunchProcess(cmd.c_str(), options);

  if (setup_proc.IsValid()) {
    if (process_finishes) {
      // Wait for process exit and compare exit code.
      int exit_code = 1;
      if (!setup_proc.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code)) {
        // Unexpected failure.
        setup_proc.Terminate(0, false);
        return SBOX_TEST_TIMED_OUT;
      }
      if (exit_code != desired_exit_code)
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    } else {
      // Terminate process with requested exit code.
      setup_proc.Terminate(desired_exit_code, false);
      return SBOX_TEST_SUCCEEDED;
    }
  }
  // Process failed to be created.
  // Note: GetLastError from CreateProcess returns 5, "ERROR_ACCESS_DENIED".
  // Validate the NoChildProcessCreation policy is applied.
  PROCESS_MITIGATION_CHILD_PROCESS_POLICY policy = {};
  if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                    ProcessChildProcessPolicy, &policy,
                                    sizeof(policy))) {
    return SBOX_TEST_NOT_FOUND;
  }
  if (!policy.NoChildProcessCreation) {
    return SBOX_TEST_FIRST_ERROR;
  } else {
    return SBOX_TEST_SECOND_ERROR;
  }
}

//------------------------------------------------------------------------------
// Exported Mitigation Tests
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// DEP (MITIGATION_DEP and MITIGATION_DEP_NO_ATL_THUNK)
// >= Win8 x86
//------------------------------------------------------------------------------

#if !defined(_WIN64)
// DEP is always enabled on 64-bit.  Only test on x86.

// This test validates that setting the MITIGATION_DEP*
// mitigations enables the setting on a process.
TEST(ProcessMitigationsTest, CheckDepWin8PolicySuccess) {
  DWORD flags;
  BOOL permanent;
  ASSERT_TRUE(::GetProcessDEPPolicy(::GetCurrentProcess(), &flags, &permanent));
  // If DEP is enabled permanently these tests are meaningless. Just ignore them
  // for this system.
  if (permanent)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_DEP);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_DEP |
                                          MITIGATION_DEP_NO_ATL_THUNK),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(config2->SetDelayedProcessMitigations(MITIGATION_DEP |
                                                  MITIGATION_DEP_NO_ATL_THUNK),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

#endif  // !defined(_WIN64)

//------------------------------------------------------------------------------
// ASLR
// - MITIGATION_RELOCATE_IMAGE
// - MITIGATION_RELOCATE_IMAGE_REQUIRED
// - MITIGATION_BOTTOM_UP_ASLR
// - MITIGATION_HIGH_ENTROPY_ASLR
// >= Win8
//------------------------------------------------------------------------------

TEST(ProcessMitigationsTest, CheckWin8AslrPolicySuccess) {
  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_ASLR);

//---------------------------------------------
// Only test in release for now.
// TODO(pennymac): overhaul ASLR, crbug/834907.
//---------------------------------------------
#if defined(NDEBUG)
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(
                MITIGATION_RELOCATE_IMAGE | MITIGATION_RELOCATE_IMAGE_REQUIRED |
                MITIGATION_BOTTOM_UP_ASLR | MITIGATION_HIGH_ENTROPY_ASLR),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
#endif  // defined(NDEBUG)
}

//------------------------------------------------------------------------------
//  Strict handle checks (MITIGATION_STRICT_HANDLE_CHECKS)
// >= Win8
//------------------------------------------------------------------------------

TEST(ProcessMitigationsTest, CheckWin8StrictHandlePolicySuccess) {
  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_STRICTHANDLE);

  //---------------------------------
  // 1) Test setting post-startup.
  // ** Can only be set post-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(
      config->SetDelayedProcessMitigations(MITIGATION_STRICT_HANDLE_CHECKS),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

//------------------------------------------------------------------------------
// Disable non-system font loads (MITIGATION_NONSYSTEM_FONT_DISABLE)
// >= Win10
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownPolicySuccess) {
  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_NONSYSFONT);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_NONSYSTEM_FONT_DISABLE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(
      config2->SetDelayedProcessMitigations(MITIGATION_NONSYSTEM_FONT_DISABLE),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates that we can load a non-system font if the
// MITIGATION_NON_SYSTEM_FONTS_DISABLE mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownLoadSuccess) {
  TestWin10NonSystemFont(true /* is_success_test */);
}

// This test validates that setting the MITIGATION_NON_SYSTEM_FONTS_DISABLE
// mitigation prevents the loading of a non-system font.
TEST(ProcessMitigationsTest, CheckWin10NonSystemFontLockDownLoadFailure) {
  TestWin10NonSystemFont(false /* is_success_test */);
}

//------------------------------------------------------------------------------
// Force MS Signed Binaries (MITIGATION_FORCE_MS_SIGNED_BINS)
// >= Win10 TH2
//
// (Note: the signing options for "MS store-signed" and "MS, store, or WHQL"
//  are not supported or tested by the sandbox at the moment.)
//------------------------------------------------------------------------------

// This test validates that setting the MITIGATION_FORCE_MS_SIGNED_BINS
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10MsSignedPolicySuccessDelayed) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_MSSIGNED);

//---------------------------------
// 1) Test setting post-startup.
// **Only test if NOT component build, otherwise component DLLs are not signed
//   by MS and prevent process setup.
// **Only test post-startup, otherwise this test executable has dependencies
//   on DLLs that are not signed by MS and they prevent process startup.
//---------------------------------
#if !defined(COMPONENT_BUILD)
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(
      config2->SetDelayedProcessMitigations(MITIGATION_FORCE_MS_SIGNED_BINS),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
#endif  // !defined(COMPONENT_BUILD)
}

// This test validates that setting the MITIGATION_FORCE_MS_SIGNED_BINS
// mitigation enables the setting on a process when non-delayed.

// Disabled due to crbug.com/1081080
TEST(ProcessMitigationsTest, DISABLED_CheckWin10MsSignedPolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_MSSIGNED);

  //---------------------------------
  // 1) Test setting post-startup.
  // **Only test if NOT component build, otherwise component DLLs are not signed
  //   by MS and prevent process setup.
  // **Only test post-startup, otherwise this test executable has dependencies
  //   on DLLs that are not signed by MS and they prevent process startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(config2->SetProcessMitigations(MITIGATION_FORCE_MS_SIGNED_BINS),
            SBOX_ALL_OK);
  // In a component build, the DLLs must be allowed to load.
#if defined(COMPONENT_BUILD)
  base::FilePath exe_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  // Allow all *.dll in current directory to load.
  EXPECT_EQ(sandbox::SBOX_ALL_OK,
            config2->AddRule(
                sandbox::SubSystem::kSignedBinary,
                sandbox::Semantics::kSignedAllowLoad,
                exe_path.DirName().AppendASCII("*.dll").value().c_str()));
#endif  // defined(COMPONENT_BUILD)

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}
// This test validates that we can load an unsigned DLL if the
// MITIGATION_FORCE_MS_SIGNED_BINS mitigation is NOT set.
TEST(ProcessMitigationsTest, CheckWin10MsSigned_Success) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  TestWin10MsSigned(sandbox::SBOX_TEST_SUCCEEDED /* expected */,
                    false /* enable_mitigation */,
                    false /* delayed */,
                    false /* use_ms_signed_binary */,
                    false /* add_dll_permission */,
                    false /* add_directory_permission */);
}

// This test validates that setting the MITIGATION_FORCE_MS_SIGNED_BINS
// mitigation prevents the loading of an unsigned DLL.
TEST(ProcessMitigationsTest, CheckWin10MsSigned_Failure) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  TestWin10MsSigned(sandbox::SBOX_TEST_FAILED /* expected */,
                    true /* enable_mitigation */,
                    true /* delayed */,
                    false /* use_ms_signed_binary */,
                    false /* add_dll_permission */,
                    false /* add_directory_permission */);
}

// ASAN doesn't initialize early enough for the intercepts in NtCreateSection to
// be able to use std::unique_ptr, so disable pre-launch CIG on ASAN builds.
#if !defined(ADDRESS_SANITIZER)
#define MAYBE_CheckWin10MsSigned_FailurePreSpawn \
  CheckWin10MsSigned_FailurePreSpawn
#else
#define MAYBE_CheckWin10MsSigned_FailurePreSpawn \
  DISABLED_CheckWin10MsSigned_FailurePreSpawn
#endif

// This test validates that setting the MITIGATION_FORCE_MS_SIGNED_BINS
// mitigation allows the loading of an unsigned DLL if intercept in place.

// Disabled due to crbug.com/1081080. This test was previously disabled on ASAN
// builds, so if re-enabling remember to test that behaviour.
TEST(ProcessMitigationsTest, DISABLED_CheckWin10MsSignedWithIntercept_Success) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Expect success; Enable mitigation; Use non MS-signed binary.
#if defined(COMPONENT_BUILD)
  // In a component build, add the directory to the allowed list.
  TestWin10MsSigned(sandbox::SBOX_TEST_SUCCEEDED /* expected */,
                    true /* enable_mitigation */,
                    false /* delayed */,
                    false /* use_ms_signed_binary */,
                    true /* add_dll_permission */,
                    true /* add_directory_permission */);
#else
  TestWin10MsSigned(sandbox::SBOX_TEST_SUCCEEDED /* expected */,
                    true /* enable_mitigation */,
                    false /* delayed */,
                    false /* use_ms_signed_binary */,
                    true /* add_dll_permission */,
                    false /* add_directory_permission */);
#endif  // defined(COMPONENT_BUILD)
}

// This test validates that setting the MITIGATION_FORCE_MS_SIGNED_BINS
// mitigation pre-load prevents the loading of an unsigned DLL.
TEST(ProcessMitigationsTest, MAYBE_CheckWin10MsSigned_FailurePreSpawn) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  // Other code in base/process relies on this invariant.
  static_assert(
      base::win::kStatusInvalidImageHashExitCode == STATUS_INVALID_IMAGE_HASH,
      "Invalid hash exit code does not match between base and sandbox.");

#if defined(COMPONENT_BUILD)
  // In a component build, the executable will fail to start-up because
  // imports e.g. base.dll cannot be resolved.
  int expected = STATUS_INVALID_IMAGE_HASH;
#else
  // In a non-component build, the process will start, but the unsigned
  // DLL will fail to load inside the test itself.
  int expected = sandbox::SBOX_TEST_FAILED;
#endif

  TestWin10MsSigned(expected /* expected */,
                    true /* enable_mitigation */,
                    false /* delayed */,
                    false /* use_ms_signed_binary */,
                    false /* add_dll_permission */,
                    false /* add_directory_permission */);
}

// This test validates that we can load a signed Microsoft DLL if the
// MITIGATION_FORCE_MS_SIGNED_BINS mitigation is NOT set.  Very basic
// sanity test.
TEST(ProcessMitigationsTest, CheckWin10MsSigned_MsBaseline) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  TestWin10MsSigned(sandbox::SBOX_TEST_SUCCEEDED /* expected */,
                    false /* enable_mitigation */,
                    false /* delayed */,
                    true /* use_ms_signed_binary */,
                    false /* add_dll_permission */,
                    false /* add_directory_permission */);
}

// This test validates that setting the MITIGATION_FORCE_MS_SIGNED_BINS
// mitigation still allows the load of an MS-signed DLL.
TEST(ProcessMitigationsTest, CheckWin10MsSigned_MsSuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  ScopedTestMutex mutex(hooking_dll::g_hooking_dll_mutex);

  TestWin10MsSigned(sandbox::SBOX_TEST_SUCCEEDED /* expected */,
                    true /* enable_mitigation */,
                    true /* delayed */,
                    true /* use_ms_signed_binary */,
                    false /* add_dll_permission */,
                    false /* add_directory_permission */);
}

//------------------------------------------------------------------------------
// Disable child process creation.
// - JobLevel <= JobLevel::kLimitedUser (on < WIN10_TH2).
// - JobLevel <= JobLevel::kLimitedUser which also triggers setting
//   PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY to
//   PROCESS_CREATION_CHILD_PROCESS_RESTRICTED in
//   BrokerServicesBase::SpawnTarget (on >= WIN10_TH2).
//------------------------------------------------------------------------------

// This test validates that we can spawn a child process if
// MITIGATION_CHILD_PROCESS_CREATION_RESTRICTED mitigation is
// not set.
TEST(ProcessMitigationsTest, CheckChildProcessSuccess) {
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  // Set a policy that would normally allow for process creation.
  EXPECT_EQ(SBOX_ALL_OK, config->SetJobLevel(JobLevel::kInteractive, 0));
  EXPECT_EQ(SBOX_ALL_OK,
            config->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED));
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess \"";
  test_command += cmd.value().c_str();
  test_command += L"\" false";

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates that setting the
// MITIGATION_CHILD_PROCESS_CREATION_RESTRICTED mitigation prevents
// the spawning of child processes.
TEST(ProcessMitigationsTest, CheckChildProcessFailure) {
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  // Now set the job level to be <= JobLevel::kLimitedUser
  // and ensure we can no longer create a child process.
  EXPECT_EQ(SBOX_ALL_OK, config->SetJobLevel(JobLevel::kLimitedUser, 0));
  EXPECT_EQ(SBOX_ALL_OK,
            config->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED));
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess \"";
  test_command += cmd.value().c_str();
  test_command += L"\" false";

  // ProcessChildProcessPolicy introduced in RS3.
  if (base::win::GetVersion() >= base::win::Version::WIN10_RS3) {
    EXPECT_EQ(SBOX_TEST_SECOND_ERROR, runner.RunTest(test_command.c_str()));
  } else {
    EXPECT_EQ(SBOX_TEST_NOT_FOUND, runner.RunTest(test_command.c_str()));
  }
}

// This test validates that when the sandboxed target within a job spawns a
// child process and the target process exits abnormally, the broker correctly
// handles the JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS message.
// Because this involves spawning a child process from the target process and is
// very similar to the above CheckChildProcess* tests, this test is here rather
// than elsewhere closer to the other Job tests.
TEST(ProcessMitigationsTest, CheckChildProcessAbnormalExit) {
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  // Set a policy that would normally allow for process creation.
  EXPECT_EQ(SBOX_ALL_OK, config->SetJobLevel(JobLevel::kInteractive, 0));
  EXPECT_EQ(SBOX_ALL_OK,
            config->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED));
  runner.SetDisableCsrss(false);

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess \"";
  test_command += cmd.value().c_str();
  test_command += L"\" false ";
  test_command += std::to_wstring(STATUS_ACCESS_VIOLATION);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

//------------------------------------------------------------------------------
// Restrict indirect branch prediction
// (MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION)
// >= Win10 RS3
//------------------------------------------------------------------------------

// This test validates that setting the
// MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION mitigation enables the setting
// on a process.
TEST(ProcessMitigationsTest,
     CheckWin10RestrictIndirectBranchPredictionPolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS3)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_RESTRICTINDIRECTBRANCHPREDICTION);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(
                MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //    ** Post-startup not supported.  Must be enabled on creation.
  //---------------------------------
}

//------------------------------------------------------------------------------
// Hardware shadow stack / Control(flow) Enforcement Technology / CETCOMPAT
// (MITIGATION_CET_DISABLED)
// >= Win10 2004
//------------------------------------------------------------------------------

// This test validates that setting the
// MITIGATION_CET_DISABLED mitigation disables CET in child processes. The test
// only makes sense where the parent was launched with CET enabled, hence we
// bail out early on systems that do not support CET.
TEST(ProcessMitigationsTest, CetDisablePolicy) {
  if (base::win::GetVersion() < base::win::Version::WIN10_20H1)
    return;

  // Verify policy is available and set for this process (i.e. CET is
  // enabled via IFEO or through the CETCOMPAT bit on the executable).
  PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY uss_policy;
  if (!::GetProcessMitigationPolicy(GetCurrentProcess(),
                                    ProcessUserShadowStackPolicy, &uss_policy,
                                    sizeof(uss_policy))) {
    return;
  }

  if (!uss_policy.EnableUserShadowStack)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_CETDISABLED);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_CET_DISABLED),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //    ** Post-startup not supported.  Must be enabled on creation.
  //---------------------------------
}

// This test validates that setting the
// MITIGATION_CET_ALLOW_DYNAMIC_APIS enables CET with in-process dynamic apis
// allowed for the child process. The test only makes sense where the parent was
// launched with CET enabled, hence we bail out early on systems that do not
// support CET.
TEST(ProcessMitigationsTest, CetAllowDynamicApis) {
  if (base::win::GetVersion() < base::win::Version::WIN10_20H1)
    return;

  // Verify policy is available and set for this process (i.e. CET is
  // enabled via IFEO or through the CETCOMPAT bit on the executable).
  PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY uss_policy;
  if (!::GetProcessMitigationPolicy(GetCurrentProcess(),
                                    ProcessUserShadowStackPolicy, &uss_policy,
                                    sizeof(uss_policy))) {
    return;
  }

  if (!uss_policy.EnableUserShadowStack)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_CETDYNAMICAPIS);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_CET_ALLOW_DYNAMIC_APIS),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //    ** Post-startup not supported.  Must be enabled on creation.
  //---------------------------------
}

// This test validates that setting the MITIGATION_CET_STRICT_MODE enables CET
// in strict mode. The test only makes sense where the parent was launched with
// CET enabled, hence we bail out early on systems that do not support CET.
TEST(ProcessMitigationsTest, CetStrictMode) {
  if (base::win::GetVersion() < base::win::Version::WIN10_20H1)
    return;

  // Verify policy is available and set for this process (i.e. CET is
  // enabled via IFEO or through the CETCOMPAT bit on the executable).
  PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY uss_policy;
  if (!::GetProcessMitigationPolicy(GetCurrentProcess(),
                                    ProcessUserShadowStackPolicy, &uss_policy,
                                    sizeof(uss_policy))) {
    return;
  }

  if (!uss_policy.EnableUserShadowStack)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_CETSTRICT);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_CET_STRICT_MODE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //    ** Post-startup not supported.  Must be enabled on creation.
  //---------------------------------
}

TEST(ProcessMitigationsTest, CheckWin10KernelTransactionManagerMitigation) {
  const auto& ver = base::win::OSInfo::GetInstance()->version_number();

  // This feature is enabled starting in KB5005101
  if (ver.build < 19041 || (ver.build < 19044 && ver.patch < 1202))
    return;

  std::wstring test_policy_command = L"CheckPolicy ";
  test_policy_command += std::to_wstring(TESTPOLICY_KTMCOMPONENTFILTER);
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();
  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_KTM_COMPONENT),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_policy_command.c_str()));
}

TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoRemotePolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_LOADNOREMOTE);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_IMAGE_LOAD_NO_REMOTE),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(
      config2->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_REMOTE),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

//---------------
// This test validates that setting the MITIGATION_IMAGE_LOAD_NO_LOW_LABEL
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadNoLowLabelPolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_TH2)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_LOADNOLOW);

  //---------------------------------
  // 1) Test setting pre-startup.
  //---------------------------------
  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
            SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(
      config2->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates that setting the MITIGATION_IMAGE_LOAD_PREFER_SYS32
// mitigation enables the setting on a process.
TEST(ProcessMitigationsTest, CheckWin10ImageLoadPreferSys32PolicySuccess) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += std::to_wstring(TESTPOLICY_LOADPREFERSYS32);

  //---------------------------------
  // 1) Test setting pre-startup.
  //   ** Currently disabled.  All PreferSys32 tests start to explode on
  //   >= Win10 1703/RS2 when this mitigation is set pre-startup.
  //   Child process creation works fine, but when ::ResumeThread() is called,
  //   there is a fatal error: "Entry point ucnv_convertEx_60 could not be
  //   located in the DLL ... sbox_integration_tests.exe."
  //   This is a character conversion function in a ucnv (unicode) DLL.
  //   Potentially the loader is finding a different version of this DLL that
  //   we have a dependency on in System32... but it doesn't match up with
  //   what we build against???!
  //---------------------------------

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  TestRunner runner2;
  sandbox::TargetConfig* config2 = runner2.GetPolicy()->GetConfig();

  EXPECT_EQ(
      config2->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_PREFER_SYS32),
      SBOX_ALL_OK);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner2.RunTest(test_command.c_str()));
}

// This test validates setting a pre-startup mitigation and a post startup
// mitigation on the same windows policy works in release and crashes in debug.
TEST(ProcessMitigationsTest, SetPreAndPostStartupSamePolicy_ImageLoad) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  std::wstring test_command = L"CheckPolicy ";
  test_command += base::NumberToWString(TESTPOLICY_PREANDPOSTSTARTUP);

  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  //---------------------------------
  // 1) Test setting pre-startup.
  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_IMAGE_LOAD_NO_LOW_LABEL),
            SBOX_ALL_OK);

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  EXPECT_EQ(
      config->SetDelayedProcessMitigations(MITIGATION_IMAGE_LOAD_PREFER_SYS32),
      SBOX_ALL_OK);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates setting a pre-startup mitigation and a post startup
// mitigation on the same windows policy works in release and crashes in debug.
TEST(ProcessMitigationsTest, SetPreAndPostStartupSamePolicy_ProcessDep) {
  std::wstring test_command = L"CheckPolicy ";
  test_command += base::NumberToWString(TESTPOLICY_DEP);

  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  //---------------------------------
  // 1) Test setting pre-startup.
  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_DEP), SBOX_ALL_OK);

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  EXPECT_EQ(config->SetDelayedProcessMitigations(MITIGATION_DEP_NO_ATL_THUNK),
            SBOX_ALL_OK);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

// This test validates setting a pre-startup mitigation and a post startup
// mitigation on the same windows policy works in release and crashes in debug.
TEST(ProcessMitigationsTest, SetPreAndPostStartupSamePolicy_ASLR) {
  std::wstring test_command = L"CheckPolicy ";
  test_command += base::NumberToWString(TESTPOLICY_ASLR);

  TestRunner runner;
  sandbox::TargetConfig* config = runner.GetPolicy()->GetConfig();

  //---------------------------------
  // 1) Test setting pre-startup.
  EXPECT_EQ(config->SetProcessMitigations(MITIGATION_BOTTOM_UP_ASLR |
                                          MITIGATION_HIGH_ENTROPY_ASLR),
            SBOX_ALL_OK);

  //---------------------------------
  // 2) Test setting post-startup.
  //---------------------------------
  EXPECT_EQ(config->SetDelayedProcessMitigations(
                MITIGATION_RELOCATE_IMAGE | MITIGATION_RELOCATE_IMAGE_REQUIRED),
            SBOX_ALL_OK);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(test_command.c_str()));
}

}  // namespace sandbox
