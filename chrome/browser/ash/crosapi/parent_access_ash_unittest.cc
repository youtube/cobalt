// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/crosapi/parent_access_ash.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

class FakeParentAccessDialogProvider : public ash::ParentAccessDialogProvider {
 public:
  ParentAccessDialogProvider::ShowError Show(
      parent_access_ui::mojom::ParentAccessParamsPtr params,
      ash::ParentAccessDialog::Callback callback) override {
    callback_ = std::move(callback);
    last_params_received_ = std::move(params);
    return next_show_error_;
  }

  void SetNextShowError(ash::ParentAccessDialogProvider::ShowError error) {
    next_show_error_ = error;
  }

  parent_access_ui::mojom::ParentAccessParamsPtr GetLastParamsReceived() {
    return std::move(last_params_received_);
  }

  void TriggerCallbackWithResult(
      std::unique_ptr<ash::ParentAccessDialog::Result> result) {
    std::move(callback_).Run(std::move(result));
  }

 private:
  ash::ParentAccessDialog::Callback callback_;
  parent_access_ui::mojom::ParentAccessParamsPtr last_params_received_;
  ash::ParentAccessDialogProvider::ShowError next_show_error_;
};

namespace {
constexpr char test_url[] = "http://example.com";
const std::u16string test_child_display_name = u"child display name";
const gfx::ImageSkia test_favicon =
    gfx::ImageSkia::CreateFrom1xBitmap(gfx::test::CreateBitmap(1, 2));
const std::u16string test_extension_name = u"extension";
}  // namespace

class ParentAccessAshTest : public testing::Test {
 public:
  ParentAccessAshTest() = default;
  ~ParentAccessAshTest() override = default;

  // testing::Test:
  void SetUp() override {
    parent_access_ash_ = std::make_unique<crosapi::ParentAccessAsh>();
    parent_access_ash_->BindReceiver(
        parent_access_remote_.BindNewPipeAndPassReceiver());
    dialog_provider_ = static_cast<FakeParentAccessDialogProvider*>(
        parent_access_ash_->SetDialogProviderForTest(
            std::make_unique<FakeParentAccessDialogProvider>()));
  }

  void TearDown() override { parent_access_ash_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<crosapi::mojom::ParentAccess> parent_access_remote_;
  std::unique_ptr<crosapi::ParentAccessAsh> parent_access_ash_;
  raw_ptr<FakeParentAccessDialogProvider, DanglingUntriaged | ExperimentalAsh>
      dialog_provider_;
};

// Tests that the correct parameters were passed through to the dialog for
// the GetWebsiteParentApproval method.
TEST_F(ParentAccessAshTest, GetWebsiteParentApprovalParams) {
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon, base::DoNothing());

  parent_access_ui::mojom::ParentAccessParamsPtr params =
      dialog_provider_->GetLastParamsReceived();

  // Check for the correct flow type.
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess);
  // Check for the correct URL.
  EXPECT_EQ(params->flow_type_params->get_web_approvals_params()->url,
            GURL(test_url));
  // Check for the correct child display name.
  EXPECT_EQ(
      params->flow_type_params->get_web_approvals_params()->child_display_name,
      test_child_display_name);

  // Encode the test bitmap.
  std::vector<uint8_t> favicon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*test_favicon.bitmap(), false,
                                        &favicon_bitmap);
  // Make sure it's there.
  EXPECT_EQ(
      params->flow_type_params->get_web_approvals_params()->favicon_png_bytes,
      favicon_bitmap);
}

TEST_F(ParentAccessAshTest, GetExtensionParentApprovalParams) {
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, false,
      base::DoNothing());

  parent_access_ui::mojom::ParentAccessParamsPtr params =
      dialog_provider_->GetLastParamsReceived();

  // Verify request params.
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kExtensionAccess);
  EXPECT_EQ(params->is_disabled, false);
  EXPECT_EQ(params->flow_type_params->get_extension_approvals_params()
                ->child_display_name,
            test_child_display_name);
  EXPECT_EQ(params->flow_type_params->get_extension_approvals_params()
                ->extension_name,
            test_extension_name);
  std::vector<uint8_t> icon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(
      *extensions::util::GetDefaultExtensionIcon().bitmap(), false,
      &icon_bitmap);
  EXPECT_EQ(params->flow_type_params->get_extension_approvals_params()
                ->icon_png_bytes,
            icon_bitmap);
}

TEST_F(ParentAccessAshTest, GetExtensionApprovalParamsForExtensionDisabled) {
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, true, base::DoNothing());

  parent_access_ui::mojom::ParentAccessParamsPtr params =
      dialog_provider_->GetLastParamsReceived();

  // Verify request params.
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kExtensionAccess);
  EXPECT_EQ(params->is_disabled, true);
}

// Makes sure the correct result is returned by the crosapi when the request is
// approved.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Approved) {
  base::RunLoop run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            // The request was approved.
            EXPECT_TRUE(result->is_approved());
            EXPECT_EQ(result->get_approved()->parent_access_token, "ABC123");
            EXPECT_EQ(
                result->get_approved()->parent_access_token_expire_timestamp,
                base::Time::FromSecondsSinceUnixEpoch(123456UL));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_result->parent_access_token = "ABC123";
  dialog_result->parent_access_token_expire_timestamp =
      base::Time::FromSecondsSinceUnixEpoch(123456UL);
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

// Makes sure the correct result is returned by the crosapi when the request
// is declined.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Declined) {
  base::RunLoop run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            // The request was declined.
            EXPECT_TRUE(result->is_declined());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kDeclined;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

// Makes sure an cancel result is returned by the crosapi when the request
// is canceled.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Canceled) {
  base::RunLoop run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            // The request had an was canceled.
            EXPECT_TRUE(result->is_canceled());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kCanceled;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

// Makes sure an error result is returned by the crosapi when the request
// had an error.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Error) {
  base::RunLoop run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            // The request had an unknown error.
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(result->get_error()->type,
                      crosapi::mojom::ParentAccessErrorResult::Type::kUnknown);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kError;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

// Makes sure only one ParentAccess request can exist at a time..
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_AlreadyVisible) {
  base::RunLoop successful_show_run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            // The request was approved.
            EXPECT_TRUE(result->is_approved());
            std::move(quit_closure).Run();
          },
          successful_show_run_loop.QuitClosure()));

  dialog_provider_->SetNextShowError(
      ash::ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible);

  // Show dialog again, should be blocked because it is already visible.
  base::RunLoop already_visible_run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            // There was an error showing the ParentAccessUI
            EXPECT_TRUE(result->is_error());
            // The error was that it is already visible.
            EXPECT_EQ(
                result->get_error()->type,
                crosapi::mojom::ParentAccessErrorResult::Type::kAlreadyVisible);
            std::move(quit_closure).Run();
          },
          already_visible_run_loop.QuitClosure()));

  // The second run loop completes.
  already_visible_run_loop.Run();

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));

  // The original run loop completes.
  successful_show_run_loop.Run();
}

// Makes sure regular users can't request parent access.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_NotAChildUser) {
  dialog_provider_->SetNextShowError(
      ash::ParentAccessDialogProvider::ShowError::kNotAChildUser);

  base::RunLoop run_loop;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(
                result->get_error()->type,
                crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ParentAccessAshTest, GetExtensionParentApproval_Canceled) {
  base::RunLoop run_loop;
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, true,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            EXPECT_TRUE(result->is_canceled());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kCanceled;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessAshTest, GetExtensionParentApproval_Error) {
  base::RunLoop run_loop;
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, true,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(result->get_error()->type,
                      crosapi::mojom::ParentAccessErrorResult::Type::kUnknown);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kError;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));
  run_loop.Run();
}

TEST_F(ParentAccessAshTest, GetExtensionParentApproval_AlreadyVisible) {
  base::RunLoop successful_show_run_loop;
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, true,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            EXPECT_TRUE(result->is_approved());
            std::move(quit_closure).Run();
          },
          successful_show_run_loop.QuitClosure()));

  dialog_provider_->SetNextShowError(
      ash::ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible);

  // Show dialog again, should be blocked because it is already visible.
  base::RunLoop already_visible_run_loop;
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, true,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(
                result->get_error()->type,
                crosapi::mojom::ParentAccessErrorResult::Type::kAlreadyVisible);
            std::move(quit_closure).Run();
          },
          already_visible_run_loop.QuitClosure()));

  already_visible_run_loop.Run();

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_provider_->TriggerCallbackWithResult(std::move(dialog_result));

  successful_show_run_loop.Run();
}

TEST_F(ParentAccessAshTest, GetExtensionParentApproval_NotAChildUser) {
  dialog_provider_->SetNextShowError(
      ash::ParentAccessDialogProvider::ShowError::kNotAChildUser);

  base::RunLoop run_loop;
  parent_access_ash_->GetExtensionParentApproval(
      test_extension_name, test_child_display_name,
      extensions::util::GetDefaultExtensionIcon(), {}, true,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             crosapi::mojom::ParentAccessResultPtr result) {
            EXPECT_TRUE(result->is_error());
            EXPECT_EQ(
                result->get_error()->type,
                crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}
