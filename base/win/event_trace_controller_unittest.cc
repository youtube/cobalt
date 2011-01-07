// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for event trace controller.

#include <initguid.h>  // NOLINT.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/win/event_trace_controller.h"
#include "base/win/event_trace_provider.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::win::EtwTraceController;
using base::win::EtwTraceProvider;
using base::win::EtwTraceProperties;

const wchar_t kTestSessionName[] = L"TestLogSession";

// {0D236A42-CD18-4e3d-9975-DCEEA2106E05}
DEFINE_GUID(kTestProvider,
    0xd236a42, 0xcd18, 0x4e3d, 0x99, 0x75, 0xdc, 0xee, 0xa2, 0x10, 0x6e, 0x5);

DEFINE_GUID(kGuidNull,
    0x0000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0);

const ULONG kTestProviderFlags = 0xCAFEBABE;

class TestingProvider: public EtwTraceProvider {
 public:
  explicit TestingProvider(const GUID& provider_name)
      : EtwTraceProvider(provider_name) {
    callback_event_.Set(::CreateEvent(NULL, TRUE, FALSE, NULL));
  }

  void WaitForCallback() {
    ::WaitForSingleObject(callback_event_.Get(), INFINITE);
    ::ResetEvent(callback_event_.Get());
  }

 private:
  virtual void OnEventsEnabled() {
    ::SetEvent(callback_event_.Get());
  }
  virtual void PostEventsDisabled() {
    ::SetEvent(callback_event_.Get());
  }

  base::win::ScopedHandle callback_event_;

  DISALLOW_COPY_AND_ASSIGN(TestingProvider);
};

}  // namespace

TEST(EtwTraceTest, Cleanup) {
  // Clean up potential leftover sessions from previous unsuccessful runs.
  EtwTraceProperties ignore;
  EtwTraceController::Stop(kTestSessionName, &ignore);
}

TEST(EtwTracePropertiesTest, Initialization) {
  EtwTraceProperties prop;

  EVENT_TRACE_PROPERTIES* p = prop.get();
  EXPECT_NE(0u, p->Wnode.BufferSize);
  EXPECT_EQ(0u, p->Wnode.ProviderId);
  EXPECT_EQ(0u, p->Wnode.HistoricalContext);

  EXPECT_TRUE(kGuidNull == p->Wnode.Guid);
  EXPECT_EQ(0, p->Wnode.ClientContext);
  EXPECT_EQ(WNODE_FLAG_TRACED_GUID, p->Wnode.Flags);

  EXPECT_EQ(0, p->BufferSize);
  EXPECT_EQ(0, p->MinimumBuffers);
  EXPECT_EQ(0, p->MaximumBuffers);
  EXPECT_EQ(0, p->MaximumFileSize);
  EXPECT_EQ(0, p->LogFileMode);
  EXPECT_EQ(0, p->FlushTimer);
  EXPECT_EQ(0, p->EnableFlags);
  EXPECT_EQ(0, p->AgeLimit);

  EXPECT_EQ(0, p->NumberOfBuffers);
  EXPECT_EQ(0, p->FreeBuffers);
  EXPECT_EQ(0, p->EventsLost);
  EXPECT_EQ(0, p->BuffersWritten);
  EXPECT_EQ(0, p->LogBuffersLost);
  EXPECT_EQ(0, p->RealTimeBuffersLost);
  EXPECT_EQ(0, p->LoggerThreadId);
  EXPECT_NE(0u, p->LogFileNameOffset);
  EXPECT_NE(0u, p->LoggerNameOffset);
}

TEST(EtwTracePropertiesTest, Strings) {
  EtwTraceProperties prop;

  ASSERT_STREQ(L"", prop.GetLoggerFileName());
  ASSERT_STREQ(L"", prop.GetLoggerName());

  std::wstring name(1023, L'A');
  ASSERT_HRESULT_SUCCEEDED(prop.SetLoggerFileName(name.c_str()));
  ASSERT_HRESULT_SUCCEEDED(prop.SetLoggerName(name.c_str()));
  ASSERT_STREQ(name.c_str(), prop.GetLoggerFileName());
  ASSERT_STREQ(name.c_str(), prop.GetLoggerName());

  std::wstring name2(1024, L'A');
  ASSERT_HRESULT_FAILED(prop.SetLoggerFileName(name2.c_str()));
  ASSERT_HRESULT_FAILED(prop.SetLoggerName(name2.c_str()));
}

TEST(EtwTraceControllerTest, Initialize) {
  EtwTraceController controller;

  EXPECT_EQ(NULL, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}

TEST(EtwTraceControllerTest, StartRealTimeSession) {
  EtwTraceController controller;

  HRESULT hr = controller.StartRealtimeSession(kTestSessionName, 100 * 1024);
  if (hr == E_ACCESSDENIED) {
    VLOG(1) << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_TRUE(NULL != controller.session());
  EXPECT_STREQ(kTestSessionName, controller.session_name());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_EQ(NULL, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}

TEST(EtwTraceControllerTest, StartFileSession) {
  FilePath temp;

  ASSERT_HRESULT_SUCCEEDED(file_util::CreateTemporaryFile(&temp));

  EtwTraceController controller;
  HRESULT hr = controller.StartFileSession(kTestSessionName,
                                           temp.value().c_str());
  if (hr == E_ACCESSDENIED) {
    VLOG(1) << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_TRUE(NULL != controller.session());
  EXPECT_STREQ(kTestSessionName, controller.session_name());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_EQ(NULL, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}

TEST(EtwTraceControllerTest, EnableDisable) {
  TestingProvider provider(kTestProvider);

  EXPECT_EQ(ERROR_SUCCESS, provider.Register());
  EXPECT_EQ(NULL, provider.session_handle());

  EtwTraceController controller;
  HRESULT hr = controller.StartRealtimeSession(kTestSessionName, 100 * 1024);
  if (hr == E_ACCESSDENIED) {
    VLOG(1) << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(kTestProvider,
                           TRACE_LEVEL_VERBOSE, kTestProviderFlags));

  provider.WaitForCallback();

  EXPECT_EQ(TRACE_LEVEL_VERBOSE, provider.enable_level());
  EXPECT_EQ(kTestProviderFlags, provider.enable_flags());

  EXPECT_HRESULT_SUCCEEDED(controller.DisableProvider(kTestProvider));

  provider.WaitForCallback();

  EXPECT_EQ(0, provider.enable_level());
  EXPECT_EQ(0, provider.enable_flags());

  EXPECT_EQ(ERROR_SUCCESS, provider.Unregister());

  // Enable the provider again, before registering.
  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(kTestProvider,
                           TRACE_LEVEL_VERBOSE, kTestProviderFlags));

  // Register the provider again, the settings above
  // should take immediate effect.
  EXPECT_EQ(ERROR_SUCCESS, provider.Register());

  EXPECT_EQ(TRACE_LEVEL_VERBOSE, provider.enable_level());
  EXPECT_EQ(kTestProviderFlags, provider.enable_flags());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));

  provider.WaitForCallback();

  // Session should have wound down.
  EXPECT_EQ(0, provider.enable_level());
  EXPECT_EQ(0, provider.enable_flags());
}
