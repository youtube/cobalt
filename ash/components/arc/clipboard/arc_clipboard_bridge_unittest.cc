// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/clipboard/arc_clipboard_bridge.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/clipboard.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_clipboard_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace arc {

namespace {

constexpr char kSampleText[] = "Copy&Paste 复制和粘贴 コピペ";
constexpr char16_t kSampleText16[] = u"Copy&Paste 复制和粘贴 コピペ";
constexpr char kSampleHtml[] = "<span>Copy&amp;Paste</span><span>コピペ</span>";
constexpr char16_t kSampleHtml16[] =
    u"<span>Copy&amp;Paste</span><span>コピペ</span>";

MATCHER_P(ClipDataMatcher, expected, "") {
  EXPECT_EQ(expected->representations.size(), arg->representations.size());
  for (size_t i = 0; i < expected->representations.size(); ++i) {
    EXPECT_EQ(expected->representations[i]->mime_type,
              arg->representations[i]->mime_type)
        << "index=" << i;
    EXPECT_EQ(expected->representations[i]->value->get_text(),
              arg->representations[i]->value->get_text())
        << "index=" << i;
  }
  return true;
}

ui::Clipboard* GetClipboard() {
  return ui::Clipboard::GetForCurrentThread();
}

mojom::ClipDataPtr CreateClipData(const std::string& mime_type,
                                  const std::string& text) {
  mojom::ClipDataPtr clip_data = mojom::ClipData::New();
  clip_data->representations.push_back(mojom::ClipRepresentation::New(
      mime_type, mojom::ClipValue::NewText(text)));
  return clip_data;
}

class ArcClipboardBridgeTest : public testing::Test {
 public:
  ArcClipboardBridgeTest() = default;

  ArcClipboardBridgeTest(const ArcClipboardBridgeTest&) = delete;
  ArcClipboardBridgeTest& operator=(const ArcClipboardBridgeTest&) = delete;

  ~ArcClipboardBridgeTest() override = default;

  void SetUp() override {
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    clipboard_bridge_ = std::make_unique<ArcClipboardBridge>(
        nullptr /* context */, arc_bridge_service_.get());
    clipboard_instance_ = std::make_unique<arc::FakeClipboardInstance>();
    arc_bridge_service_->clipboard()->SetInstance(clipboard_instance_.get());
    WaitForInstanceReady(arc_bridge_service_->clipboard());
  }

  void TearDown() override {
    arc_bridge_service_->clipboard()->CloseInstance(clipboard_instance_.get());
    clipboard_instance_.reset();
    clipboard_bridge_.reset();
    arc_bridge_service_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcClipboardBridge> clipboard_bridge_;
  std::unique_ptr<FakeClipboardInstance> clipboard_instance_;
};

TEST_F(ArcClipboardBridgeTest, GetClipContent_PlainText) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(kSampleText16);
  }

  mojom::ClipDataPtr expected_clip_data =
      CreateClipData(ui::kMimeTypeText, kSampleText);
  base::MockCallback<ArcClipboardBridge::GetClipContentCallback> callback;
  EXPECT_CALL(std::move(callback),
              Run(ClipDataMatcher(expected_clip_data.get())))
      .Times(1);

  clipboard_bridge_->GetClipContent(callback.Get());
}

TEST_F(ArcClipboardBridgeTest, GetClipContent_Html) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(kSampleHtml16, std::string(),
                     ui::ClipboardContentType::kSanitized);
  }

  mojom::ClipDataPtr expected_clip_data =
      CreateClipData(ui::kMimeTypeHTML, kSampleHtml);
  base::MockCallback<ArcClipboardBridge::GetClipContentCallback> callback;
  EXPECT_CALL(std::move(callback),
              Run(ClipDataMatcher(expected_clip_data.get())))
      .Times(1);

  clipboard_bridge_->GetClipContent(callback.Get());
}

TEST_F(ArcClipboardBridgeTest, SetClipContent_PlainText) {
  mojom::ClipDataPtr clip_data = CreateClipData(ui::kMimeTypeText, kSampleText);

  clipboard_bridge_->SetClipContent(std::move(clip_data));

  std::vector<std::u16string> mime_types;
  GetClipboard()->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste,
                                     /* data_dst = */ nullptr, &mime_types);
  ASSERT_EQ(1u, mime_types.size());
  EXPECT_EQ(ui::kMimeTypeText, base::UTF16ToUTF8(mime_types[0]));

  std::u16string result;
  GetClipboard()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                           /* data_dst = */ nullptr, &result);
  EXPECT_EQ(kSampleText, base::UTF16ToUTF8(result));
}

TEST_F(ArcClipboardBridgeTest, SetClipContent_Html) {
  mojom::ClipDataPtr clip_data = CreateClipData(ui::kMimeTypeHTML, kSampleHtml);

  clipboard_bridge_->SetClipContent(std::move(clip_data));

  std::vector<std::u16string> mime_types;
  GetClipboard()->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste,
                                     /* data_dst = */ nullptr, &mime_types);
  ASSERT_EQ(1u, mime_types.size());
  EXPECT_EQ(ui::kMimeTypeHTML, base::UTF16ToUTF8(mime_types[0]));

  std::u16string markup16;
  std::string url;
  uint32_t fragment_start, fragment_end;
  GetClipboard()->ReadHTML(ui::ClipboardBuffer::kCopyPaste,
                           /* data_dst = */ nullptr, &markup16, &url,
                           &fragment_start, &fragment_end);
  std::u16string result =
      markup16.substr(fragment_start, fragment_end - fragment_start);
  EXPECT_EQ(kSampleHtml, base::UTF16ToUTF8(result));
}

TEST_F(ArcClipboardBridgeTest, OnHostClipboardUpdated) {
  EXPECT_EQ(0, clipboard_instance_->num_host_clipboard_updated());

  clipboard_bridge_->OnClipboardDataChanged();

  EXPECT_EQ(1, clipboard_instance_->num_host_clipboard_updated());
}

}  // namespace
}  // namespace arc
