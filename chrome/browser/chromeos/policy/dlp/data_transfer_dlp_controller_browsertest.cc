// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace policy {

namespace {

constexpr char kClipboardText1[] = "Hello World";
constexpr char16_t kClipboardText116[] = u"Hello World";
constexpr char16_t kClipboardText2[] = u"abcdef";

constexpr char kMailUrl[] = "https://mail.google.com";
constexpr char kDocsUrl[] = "https://docs.google.com";
constexpr char kExampleUrl[] = "https://example.com";

constexpr char kRuleName1[] = "rule #1";
constexpr char kRuleId1[] = "testid1";
const DlpRulesManager::RuleMetadata kRuleMetadata1(kRuleName1, kRuleId1);

constexpr char kRuleName2[] = "rule #2";
constexpr char kRuleId2[] = "testid2";

class FakeClipboardNotifier : public DlpClipboardNotifier {
 public:
  views::Widget* GetWidget() { return widget_.get(); }

  void ProceedPressed(const ui::DataTransferEndpoint& data_dst,
                      base::RepeatingCallback<void()> reporting_cb) {
    DlpClipboardNotifier::ProceedPressed(data_dst, std::move(reporting_cb),
                                         GetWidget());
  }

  void BlinkProceedPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::BlinkProceedPressed(data_dst, GetWidget());
  }

  void CancelWarningPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::CancelWarningPressed(data_dst, GetWidget());
  }
};

class FakeDlpController : public DataTransferDlpController,
                          public views::WidgetObserver {
 public:
  FakeDlpController(const DlpRulesManager& dlp_rules_manager,
                    FakeClipboardNotifier* helper)
      : DataTransferDlpController(dlp_rules_manager), helper_(helper) {
    DCHECK(helper);
  }

  ~FakeDlpController() override {
    auto* widget = helper_->GetWidget();
    if (widget) {
      widget->RemoveObserver(this);
    }
  }

  void NotifyBlockedPaste(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) override {
    helper_->NotifyBlockedAction(data_src, data_dst);
  }

  void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst,
                   base::RepeatingCallback<void()> reporting_cb) override {
    helper_->WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
  }

  void SetBlinkQuitCallback(base::RepeatingClosure cb) {
    blink_quit_cb_ = std::move(cb);
  }

  void WarnOnBlinkPaste(const ui::DataTransferEndpoint* const data_src,
                        const ui::DataTransferEndpoint* const data_dst,
                        content::WebContents* web_contents,
                        base::OnceCallback<void(bool)> paste_cb) override {
    blink_data_dst_.emplace(*data_dst);
    helper_->WarnOnBlinkPaste(data_src, data_dst, web_contents,
                              std::move(paste_cb));
    std::move(blink_quit_cb_).Run();
  }

  bool ShouldPasteOnWarn(
      const ui::DataTransferEndpoint* const data_dst) override {
    if (force_paste_on_warn_) {
      return true;
    }
    return helper_->DidUserApproveDst(data_dst);
  }

  bool ObserveWidget() {
    auto* widget = helper_->GetWidget();
    if (widget && !widget->HasObserver(this)) {
      widget->AddObserver(this);
      return true;
    }
    return false;
  }

  void ReportWarningProceededEvent(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst,
      const std::string& src_pattern,
      const std::string& dst_pattern,
      const DlpRulesManager::RuleMetadata& rule_metadata,
      bool is_clipboard_event) {
    DataTransferDlpController::ReportWarningProceededEvent(
        base::OptionalFromPtr(data_src), base::OptionalFromPtr(data_dst),
        src_pattern, dst_pattern, is_clipboard_event, rule_metadata);
  }

  raw_ptr<FakeClipboardNotifier> helper_ = nullptr;
  absl::optional<ui::DataTransferEndpoint> blink_data_dst_;
  base::RepeatingClosure blink_quit_cb_ = base::DoNothing();
  bool force_paste_on_warn_ = false;

 protected:
  base::TimeDelta GetSkipReportingTimeout() override {
    // Override with a very high value to ensure that tests are passing on slow
    // debug builds.
    return base::Milliseconds(1000);
  }
};

class MockDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state)
      : DlpRulesManagerImpl(local_state) {}
  ~MockDlpRulesManager() override = default;

  MOCK_CONST_METHOD0(GetReportingManager, DlpReportingManager*());
};

void SetClipboardText(std::u16string text,
                      std::unique_ptr<ui::DataTransferEndpoint> source) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                   source ? std::move(source) : nullptr);
  writer.WriteText(text);
}

// On Widget Closing, a task for NativeWidgetAura::CloseNow() gets posted. This
// task runs after the widget is destroyed which leads to a crash, that's why
// we need to flush the message loop.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class DataTransferDlpBrowserTest : public InProcessBrowserTest {
 public:
  DataTransferDlpBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&DataTransferDlpBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());

    reporting_manager_ = std::make_unique<DlpReportingManager>();
    SetReportQueueForReportingManager(
        reporting_manager_.get(), events_,
        base::SequencedTaskRunner::GetCurrentDefault());
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

    dlp_controller_ =
        std::make_unique<FakeDlpController>(*rules_manager_, &helper_);
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto mock_rules_manager =
        std::make_unique<testing::NiceMock<MockDlpRulesManager>>(
            g_browser_process->local_state());
    rules_manager_ = mock_rules_manager.get();
    return mock_rules_manager;
  }

  void TearDownOnMainThread() override {
    dlp_controller_.reset();
    reporting_manager_.reset();
  }

  void SetupTextfield() {
    // Create a widget containing a single, focusable textfield.
    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    widget_->Init(std::move(params));
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->SetAccessibleName(u"Textfield");
    textfield_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    // Show the widget.
    widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    widget_->Show();

    views::test::WidgetActivationWaiter waiter(widget_.get(), true);
    widget_->Show();
    waiter.Wait();

    ASSERT_TRUE(widget_->IsActive());

    // Focus the textfield and confirm initial state.
    textfield_->RequestFocus();
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_TRUE(textfield_->GetText().empty());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        widget_->GetNativeWindow()->GetRootWindow());
  }

  raw_ptr<MockDlpRulesManager, DanglingUntriaged> rules_manager_;
  std::unique_ptr<DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events_;
  FakeClipboardNotifier helper_;
  std::unique_ptr<FakeDlpController> dlp_controller_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::Textfield, DanglingUntriaged> textfield_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, EmptyPolicy) {
  SetClipboardText(kClipboardText116, nullptr);

  ui::DataTransferEndpoint data_dst((GURL("https://google.com")));
  std::u16string result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
  EXPECT_EQ(kClipboardText116, result);
}

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, BlockDestination) {
  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule1(kRuleName1, "Block Gmail", kRuleId1);
    rule1.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        dlp::kClipboardRestriction, dlp::kBlockLevel);
    dlp_test_util::DlpRule rule2(kRuleName2, "Allow Gmail for work purposes",
                                 kRuleId2);
    rule2.AddSrcUrl(kMailUrl).AddDstUrl(kDocsUrl).AddRestriction(
        dlp::kClipboardRestriction, dlp::kAllowLevel);

    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule1.Create());
    update->Append(rule2.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  ui::DataTransferEndpoint data_dst1((GURL(kMailUrl)));
  std::u16string result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(kClipboardText116, result1);

  ui::DataTransferEndpoint data_dst2((GURL(kDocsUrl)));
  std::u16string result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(kClipboardText116, result2);

  ui::DataTransferEndpoint data_dst3((GURL(kExampleUrl)));
  std::u16string result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(std::u16string(), result3);
  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1, DlpRulesManager::Level::kBlock)));

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kExampleUrl))));

  ui::DataTransferEndpoint data_dst4((GURL(kMailUrl)));
  std::u16string result4;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result4);
  EXPECT_EQ(kClipboardText116, result4);

  FlushMessageLoop();
}

// TODO(https://issuetracker.google.com/issues/260517406) flaky test
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, DISABLED_WarnDestination) {
  base::WeakPtr<views::Widget> widget;

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        dlp::kClipboardRestriction, dlp::kWarnLevel);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  SetupTextfield();
  // Initiate a paste on textfield_.
  event_generator_->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator_->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  widget = helper_.GetWidget()->GetWeakPtr();
  EXPECT_FALSE(widget->IsClosed());
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1, DlpRulesManager::Level::kWarn)));

  auto data_src = std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl)));

  // Accept warning.
  ui::DataTransferEndpoint default_endpoint(ui::EndpointType::kDefault);
  auto reporting_cb = base::BindRepeating(
      &FakeDlpController::ReportWarningProceededEvent,
      base::Unretained(dlp_controller_.get()), data_src.get(),
      &default_endpoint, kMailUrl, "*", kRuleMetadata1, true);
  helper_.ProceedPressed(default_endpoint, std::move(reporting_cb));
  EXPECT_TRUE(!widget || widget->IsClosed());

  EXPECT_EQ(kClipboardText116, textfield_->GetText());

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1)));

  SetClipboardText(kClipboardText2, std::make_unique<ui::DataTransferEndpoint>(
                                        (GURL(kMailUrl))));

  // Initiate a paste on textfield_.
  textfield_->SetText(std::u16string());
  textfield_->RequestFocus();
  event_generator_->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator_->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  widget = helper_.GetWidget()->GetWeakPtr();
  EXPECT_FALSE(widget->IsClosed());
  EXPECT_EQ(events_.size(), 3u);
  EXPECT_THAT(events_[2],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1, DlpRulesManager::Level::kWarn)));

  // Initiate a paste on nullptr data_dst.
  std::u16string result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, nullptr, &result);
  EXPECT_TRUE(!widget || widget->IsClosed());

  EXPECT_EQ(std::u16string(), result);
  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  widget = helper_.GetWidget()->GetWeakPtr();
  EXPECT_FALSE(widget->IsClosed());
  ASSERT_EQ(events_.size(), 3u);

  FlushMessageLoop();
}

class DataTransferDlpBlinkBrowserTest : public InProcessBrowserTest {
 public:
  DataTransferDlpBlinkBrowserTest() = default;
  DataTransferDlpBlinkBrowserTest(const DataTransferDlpBlinkBrowserTest&) =
      delete;
  DataTransferDlpBlinkBrowserTest& operator=(
      const DataTransferDlpBlinkBrowserTest&) = delete;
  ~DataTransferDlpBlinkBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    rules_manager_ = std::make_unique<::testing::NiceMock<MockDlpRulesManager>>(
        g_browser_process->local_state());

    reporting_manager_ = std::make_unique<DlpReportingManager>();
    SetReportQueueForReportingManager(
        reporting_manager_.get(), events_,
        base::SequencedTaskRunner::GetCurrentDefault());
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

    dlp_controller_ =
        std::make_unique<FakeDlpController>(*rules_manager_, &helper_);
  }

  void TearDownOnMainThread() override {
    dlp_controller_.reset();
    reporting_manager_.reset();
    rules_manager_.reset();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ::testing::AssertionResult ExecJs(content::WebContents* web_contents,
                                    const std::string& code) {
    return content::ExecJs(web_contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1);
  }

  content::EvalJsResult EvalJs(content::WebContents* web_contents,
                               const std::string& code) {
    return content::EvalJs(web_contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1);
  }

  std::unique_ptr<::testing::NiceMock<MockDlpRulesManager>> rules_manager_;
  std::unique_ptr<DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events_;
  FakeClipboardNotifier helper_;
  std::unique_ptr<FakeDlpController> dlp_controller_;
};

// TODO(https://issuetracker.google.com/issues/260517406) flaky test
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest,
                       DISABLED_ProceedOnWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        dlp::kClipboardRestriction, dlp::kWarnLevel);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  // Send paste event and wait till the notification is displayed.
  base::RunLoop run_loop;
  dlp_controller_->SetBlinkQuitCallback(run_loop.QuitClosure());
  GetActiveWebContents()->Paste();
  run_loop.Run();

  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  base::WeakPtr<views::Widget> widget = helper_.GetWidget()->GetWeakPtr();
  EXPECT_FALSE(widget->IsClosed());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1, DlpRulesManager::Level::kWarn)));

  ASSERT_TRUE(dlp_controller_->blink_data_dst_.has_value());
  helper_.BlinkProceedPressed(dlp_controller_->blink_data_dst_.value());

  EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1)));

  EXPECT_TRUE(!widget || widget->IsClosed());
}

// TODO(https://issuetracker.google.com/issues/260517406) flaky test
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest, DISABLED_CancelWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        dlp::kClipboardRestriction, dlp::kWarnLevel);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));
  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  // Send paste event and wait till the notification is displayed.
  base::RunLoop run_loop;
  dlp_controller_->SetBlinkQuitCallback(run_loop.QuitClosure());
  GetActiveWebContents()->Paste();
  run_loop.Run();

  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  base::WeakPtr<views::Widget> widget = helper_.GetWidget()->GetWeakPtr();
  EXPECT_FALSE(widget->IsClosed());
  ASSERT_TRUE(dlp_controller_->blink_data_dst_.has_value());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1, DlpRulesManager::Level::kWarn)));

  helper_.CancelWarningPressed(dlp_controller_->blink_data_dst_.value());

  EXPECT_EQ("", EvalJs(GetActiveWebContents(), "p"));
  EXPECT_EQ(events_.size(), 1u);

  EXPECT_TRUE(!widget || widget->IsClosed());
}

// TODO(https://issuetracker.google.com/issues/260517406) flaky test
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest,
                       DISABLED_ShouldProceedWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        dlp::kClipboardRestriction, dlp::kWarnLevel);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  dlp_controller_->force_paste_on_warn_ = true;
  GetActiveWebContents()->Paste();
  EXPECT_FALSE(dlp_controller_->ObserveWidget());
  EXPECT_FALSE(helper_.GetWidget());
  EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1)));
}

// Test case for crbug.com/1213143
// TODO(https://issuetracker.google.com/issues/260517406) flaky test
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest, DISABLED_Reporting) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    dlp_test_util::DlpRule rule(kRuleName1, "description", kRuleId1);
    rule.AddSrcUrl(kMailUrl).AddDstUrl("*").AddRestriction(
        dlp::kClipboardRestriction, dlp::kReportLevel);
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);
    update->Append(rule.Create());
  }

  SetClipboardText(
      kClipboardText116,
      std::make_unique<ui::DataTransferEndpoint>((GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebPointerProperties::Button::kLeft);

  GetActiveWebContents()->Paste();
  EXPECT_FALSE(dlp_controller_->ObserveWidget());
  EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  kRuleName1, kRuleId1, DlpRulesManager::Level::kReport)));
  // TODO(1276063): This EXPECT_GE is always true, because it is compared to 0.
  // The histogram sum may not have any samples when the time difference is very
  // small (almost 0), because UmaHistogramTimes requires the time difference to
  // be >= 1.
  EXPECT_GE(
      histogram_tester.GetTotalSum(GetDlpHistogramPrefix() +
                                   dlp::kDataTransferReportingTimeDiffUMA),
      0);
}

}  // namespace policy
