// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_service.mojom.h"
#include "content/test/sandbox_status.test-mojom.h"
#include "media/gpu/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using sandbox::mojom::Sandbox;
using sandbox::policy::SandboxLinux;

namespace {

std::vector<Sandbox> GetSandboxTypesToTest() {
  std::vector<Sandbox> types;
  // We need the standard sandbox config to run this test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kNoSandbox)) {
    return types;
  }

  for (Sandbox t = Sandbox::kNoSandbox; t <= Sandbox::kMaxValue;
       t = static_cast<Sandbox>(static_cast<int>(t) + 1)) {
    // These sandbox types can't be spawned in a utility process.
    if (t == Sandbox::kRenderer || t == Sandbox::kGpu ||
        t == Sandbox::kZygoteIntermediateSandbox) {
      continue;
    }
    types.push_back(t);
  }
  return types;
}

}  // namespace

namespace content {

constexpr char kTestProcessName[] = "sandbox_test_process";

class UtilityProcessSandboxBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<Sandbox> {
 public:
  UtilityProcessSandboxBrowserTest() = default;
  ~UtilityProcessSandboxBrowserTest() override = default;

 protected:
  void RunUtilityProcess() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    done_closure_ =
        base::BindOnce(&UtilityProcessSandboxBrowserTest::DoneRunning,
                       base::Unretained(this), run_loop.QuitClosure());
    EXPECT_TRUE(UtilityProcessHost::Start(
        UtilityProcessHost::Options()
            .WithSandboxType(GetParam())
            .WithName(u"SandboxTestProcess")
            .WithMetricsName(kTestProcessName)
            .WithBoundReceiverOnChildProcessForTesting(
                service_.BindNewPipeAndPassReceiver())
            .Pass()));

    service_->GetSandboxStatus(
        base::BindOnce(&UtilityProcessSandboxBrowserTest::OnGotSandboxStatus,
                       base::Unretained(this)));

    run_loop.Run();
  }

 private:
  void OnGotSandboxStatus(int32_t sandbox_status) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Aside from kNoSandbox, every utility process launched explicitly with a
    // sandbox type should always end up with a sandbox.
    switch (GetParam()) {
      case Sandbox::kNoSandbox:
        EXPECT_EQ(sandbox_status, 0);
        break;

      case Sandbox::kCdm:
#if BUILDFLAG(ENABLE_PPAPI)
      case Sandbox::kPpapi:
#endif
      case Sandbox::kOnDeviceModelExecution:
      case Sandbox::kPrintCompositor:
      case Sandbox::kService:
      case Sandbox::kServiceWithJit:
      case Sandbox::kUtility: {
        constexpr int kExpectedFullSandboxFlags =
            SandboxLinux::kPIDNS | SandboxLinux::kNetNS |
            SandboxLinux::kSeccompBPF | SandboxLinux::kYama |
            SandboxLinux::kSeccompTSYNC | SandboxLinux::kUserNS;
        EXPECT_EQ(sandbox_status, kExpectedFullSandboxFlags);
        break;
      }

      case Sandbox::kAudio:
#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
      case Sandbox::kHardwareVideoDecoding:
      case Sandbox::kHardwareVideoEncoding:
#endif
#if BUILDFLAG(IS_CHROMEOS)
      case Sandbox::kIme:
      case Sandbox::kTts:
      case Sandbox::kNearby:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      case Sandbox::kLibassistant:
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_LINUX)
      case Sandbox::kVideoEffects:
      case Sandbox::kOnDeviceTranslation:
#endif
      case Sandbox::kNetwork:
      case Sandbox::kPrintBackend:
      case Sandbox::kScreenAI:
      case Sandbox::kSpeechRecognition: {
        constexpr int kExpectedPartialSandboxFlags =
            SandboxLinux::kSeccompBPF | SandboxLinux::kYama |
            SandboxLinux::kSeccompTSYNC;
        EXPECT_EQ(sandbox_status, kExpectedPartialSandboxFlags);
        break;
      }

      case Sandbox::kGpu:
      case Sandbox::kRenderer:
      case Sandbox::kZygoteIntermediateSandbox:
        NOTREACHED();
    }

    service_.reset();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  void DoneRunning(base::OnceClosure quit_closure) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::move(quit_closure).Run();
  }

  mojo::Remote<mojom::SandboxStatusService> service_;
  base::OnceClosure done_closure_;
};

IN_PROC_BROWSER_TEST_P(UtilityProcessSandboxBrowserTest, VerifySandboxType) {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
  if (GetParam() == Sandbox::kHardwareVideoDecoding) {
    // TODO(b/195769334): On Linux, this test fails with
    // Sandbox::kHardwareVideoDecoding because the pre-sandbox hook needs Ozone
    // which is not available in the utility process that this test starts. We
    // need to remove the Ozone dependency and re-enable this test.
    //
    GTEST_SKIP();
  }

  if (GetParam() == Sandbox::kHardwareVideoEncoding) {
    // TODO(b/248540499): On Linux, this test fails with
    // Sandbox::kHardwareVideoEncoding because the pre-sandbox hook needs Ozone
    // which is not available in the utility process that this test starts. We
    // need to remove the Ozone dependency and re-enable this test.
    GTEST_SKIP();
  }
#endif
  RunUtilityProcess();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UtilityProcessSandboxBrowserTest,
    testing::ValuesIn(GetSandboxTypesToTest()),
    [](const testing::TestParamInfo<
        UtilityProcessSandboxBrowserTest::ParamType>& info) {
      auto name = sandbox::policy::StringFromUtilitySandboxType(info.param);
      name[0] = base::ToUpperASCII(name[0]);
      return name;
    });

// In some configurations (e.g. Linux ASAN) GetSandboxTypesToTest() returns an
// empty list. Suppress runtime warnings about unparameterized tests. See
// https://crbug.com/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(UtilityProcessSandboxBrowserTest);

}  // namespace content
