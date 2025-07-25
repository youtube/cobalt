// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/truncate.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operations/test_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/file_system/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/kitty/and/puppy/happy");
const int64_t kTruncateLength = 64;

}  // namespace

class FileSystemProviderOperationsTruncateTest : public testing::Test {
 protected:
  FileSystemProviderOperationsTruncateTest() {}
  ~FileSystemProviderOperationsTruncateTest() override {}

  void SetUp() override {
    MountOptions mount_options(kFileSystemId, "" /* display_name */);
    mount_options.writable = true;
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, mount_options, base::FilePath(), false /* configurable */,
        true /* watchable */, extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsTruncateTest, Execute) {
  using extensions::api::file_system_provider::TruncateRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Truncate truncate(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                    kTruncateLength,
                    base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(truncate.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnTruncateRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  TruncateRequestedOptions options;
  ASSERT_TRUE(
      TruncateRequestedOptions::Populate(options_as_value->GetDict(), options));
  EXPECT_EQ(kFileSystemId, options.file_system_id);
  EXPECT_EQ(kRequestId, options.request_id);
  EXPECT_EQ(kFilePath, options.file_path);
  EXPECT_EQ(kTruncateLength, static_cast<double>(options.length));
}

TEST_F(FileSystemProviderOperationsTruncateTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Truncate truncate(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                    kTruncateLength,
                    base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_FALSE(truncate.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsTruncateTest, Execute_ReadOnly) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  const ProvidedFileSystemInfo read_only_file_system_info(
      kExtensionId, MountOptions(kFileSystemId, "" /* display_name */),
      base::FilePath() /* mount_path */, false /* configurable */,
      true /* watchable */, extensions::SOURCE_FILE, IconSet());

  Truncate truncate(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                    kTruncateLength,
                    base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_FALSE(truncate.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsTruncateTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Truncate truncate(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                    kTruncateLength,
                    base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(truncate.Execute(kRequestId));

  truncate.OnSuccess(kRequestId, RequestValue(), false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, callback_log[0]);
}

TEST_F(FileSystemProviderOperationsTruncateTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  Truncate truncate(&dispatcher, file_system_info_, base::FilePath(kFilePath),
                    kTruncateLength,
                    base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(truncate.Execute(kRequestId));

  truncate.OnError(kRequestId, RequestValue(),
                   base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, callback_log[0]);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash
