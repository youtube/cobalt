// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

namespace {

using ::base::test::RunOnceClosure;
using ::blink::WebDocument;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebInputElement;
using ::blink::WebString;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Property;

constexpr CallTimerState kCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kUpdateFormCache,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};

class FormAutocompleteTest : public ChromeRenderViewTest {
 public:
  FormAutocompleteTest() = default;
  FormAutocompleteTest(const FormAutocompleteTest&) = delete;
  FormAutocompleteTest& operator=(const FormAutocompleteTest&) = delete;
  ~FormAutocompleteTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // We only use the fake driver for main frame
    // because our test cases only involve the main frame.
    blink::AssociatedInterfaceProvider* remote_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillDriver::Name_,
        base::BindRepeating(&FormAutocompleteTest::BindAutofillDriver,
                            base::Unretained(this)));

    focus_test_utils_ = std::make_unique<test::FocusTestUtils>(
        base::BindRepeating(&FormAutocompleteTest::ExecuteJavaScriptForTests,
                            base::Unretained(this)));
  }

  void BindAutofillDriver(mojo::ScopedInterfaceEndpointHandle handle) {
    autofill_driver_.BindPendingReceiver(std::move(handle));
  }

  void SimulateElementClick(const WebElement element) {
    SimulatePointClick(element.BoundsInWidget().CenterPoint());
  }

  // Simulates receiving a message from the browser to fill a form.
  // Blocks until the form is autofilled.
  void SimulateFillForm(std::string_view form_id = "myForm",
                        const base::flat_map<std::u16string, std::u16string>&
                            fill_values_by_id = {{u"fname", u"John"},
                                                 {u"lname", u"Smith"}}) {
    std::optional<FormData> form = form_util::ExtractFormData(
        GetMainFrame()->GetDocument(),
        GetMainFrame()
            ->GetDocument()
            .GetElementById(WebString::FromUtf8(form_id))
            .To<blink::WebFormElement>(),
        *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy,
        /*button_titles_cache=*/nullptr);

    ASSERT_TRUE(form);
    SimulateFillForm(*form, fill_values_by_id);
  }

  void SimulateFillForm(const FormData& form_data,
                        const base::flat_map<std::u16string, std::u16string>&
                            fill_values_by_id = {{u"fname", u"John"},
                                                 {u"lname", u"Smith"}}) {
    WebDocument document = GetMainFrame()->GetDocument();
    WebFormControlElement fname_element =
        document.GetElementById(WebString("fname")).To<WebFormControlElement>();

    ASSERT_TRUE(fname_element);
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    SimulateElementClick(fname_element);

    std::vector<FormFieldData::FillData> fields_for_filling;
    for (const FormFieldData& field : form_data.fields()) {
      if (const std::u16string* fill_value =
              base::FindOrNull(fill_values_by_id, field.id_attribute())) {
        FormFieldData::FillData fill_data(field);
        fill_data.value = *fill_value;
        fill_data.is_autofilled = true;
        fields_for_filling.push_back(std::move(fill_data));
      }
    }

    base::RunLoop run_loop;
    EXPECT_CALL(
        autofill_driver_,
        DidAutofillForm(Property(&FormData::global_id, form_data.global_id())))
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    autofill_agent_->ApplyFieldsAction(mojom::FormActionType::kFill,
                                       mojom::ActionPersistence::kFill,
                                       fields_for_filling, FillId::Create(),
                                       /*supports_refill=*/false);
    std::move(run_loop).Run();
  }

  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  NiceMock<test::MockAutofillDriver> autofill_driver_;
  std::unique_ptr<test::FocusTestUtils> focus_test_utils_;
};

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form
// having non-fillable fields.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventsAfterAutofill) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "<label>Middle Name:</label><input id='mname' name='2'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  SimulateFillForm();

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element.
TEST_F(FormAutocompleteTest,
       VerifyFocusAndBlurEventsAfterAutofillWithFocusedElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  SimulateFillForm();

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form having
// single field.
TEST_F(FormAutocompleteTest,
       VerifyFocusAndBlurEventAfterAutofillWithFocusedElementForSingleElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  SimulateFillForm();

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Tests that a field is added to the form between the times of triggering
// and executing the filling.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventAfterElementAdded) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  std::optional<FormData> form = form_util::ExtractFormData(
      GetMainFrame()->GetDocument(),
      GetMainFrame()
          ->GetDocument()
          .GetElementById("myForm")
          .To<blink::WebFormElement>(),
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy,
      /*button_titles_cache=*/nullptr);
  ASSERT_TRUE(form);
  FormData data = *form;
  // Simulate that the form was modified between parsing and executing the fill.
  // The element is inserted at the beginning of the form to verify that
  // everything works correctly even if `renderer_id`s of the `<input>`
  // elements are not in ascending order.
  ExecuteJavaScriptForTests(
      "document.getElementById('fname').insertAdjacentHTML('beforebegin', "
      "'<label>Zip code:</label><input id=\"zip_code\"/>');");
  SimulateFillForm(data);

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that a field is removed from the form between the times of
// triggering and executing the filling.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventAfterElementRemoved) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  std::optional<FormData> form = form_util::ExtractFormData(
      GetMainFrame()->GetDocument(),
      GetMainFrame()
          ->GetDocument()
          .GetElementById("myForm")
          .To<blink::WebFormElement>(),
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy,
      /*button_titles_cache=*/nullptr);

  ASSERT_TRUE(form);
  ExecuteJavaScriptForTests("document.getElementById('lname').remove()");
  SimulateFillForm(*form);

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Unit test for AutofillAgent::AcceptDataListSuggestion.
TEST_F(FormAutocompleteTest, AcceptDataListSuggestion) {
  LoadHTML(
      "<html>"
      "<input id='empty' type='email' multiple />"
      "<input id='multi_one' type='email' multiple value='one@example.com'/>"
      "<input id='multi_two' type='email' multiple"
      "  value='one@example.com,two@example.com'/>"
      "<input id='multi_trailing' type='email' multiple"
      "  value='one@example.com,two@example.com,'/>"
      "<input id='not_multi' type='email'"
      "  value='one@example.com,two@example.com,'/>"
      "<input id='not_email' type='text' multiple"
      "  value='one@example.com,two@example.com,'/>"
      "</html>");
  WebDocument document = GetMainFrame()->GetDocument();

  // Each case tests a different field value with the same suggestion.
  const std::u16string kSuggestion = u"suggestion@example.com";
  struct TestCase {
    std::string id;
    std::string expected;
  } cases[] = {
      // Empty text field; expect to populate with suggestion.
      {"empty", "suggestion@example.com"},
      // Single entry; expect to replace with suggestion.
      {"multi_one", "suggestion@example.com"},
      // Two comma-separated entries; expect to replace second with suggestion.
      {"multi_two", "one@example.com,suggestion@example.com"},
      // Two comma-separated entries with trailing comma; expect to append
      // suggestion.
      {"multi_trailing",
       "one@example.com,two@example.com,suggestion@example.com"},
      // Do not apply this logic for a non-multiple or non-email field.
      {"not_multi", "suggestion@example.com"},
      {"not_email", "suggestion@example.com"},
  };

  for (const auto& c : cases) {
    WebElement element = document.GetElementById(WebString::FromUtf8(c.id));
    ASSERT_TRUE(element);
    WebInputElement input_element = element.To<WebInputElement>();
    SimulateElementClick(input_element);

    autofill_agent_->AcceptDataListSuggestion(
        form_util::GetFieldRendererId(input_element), kSuggestion);
    EXPECT_EQ(c.expected, input_element.Value().Utf8()) << "Case id: " << c.id;
  }
}

TEST_F(FormAutocompleteTest, SelectControlChanged) {
  LoadHTML(
      "<html>"
      "<form>"
      "<select id='color'><option value='red'>red</option><option "
      "value='blue'>blue</option></select>"
      "</form>"
      "</html>");

  std::string change_value =
      "var color = document.getElementById('color');"
      "color.selectedIndex = 1;";

  base::RunLoop run_loop;
  EXPECT_CALL(autofill_driver_,
              SelectControlSelectionChanged(
                  Property(&FormData::fields,
                           ElementsAre(test::FormFieldDescriptionEq(
                               {.name = u"color", .value = u"blue"}))),
                  _))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  // The click simulation is necessary to give the frame transient user
  // activation, otherwise the select value-change event will be ignored by the
  // agent.
  SimulateElementClick(
      GetMainFrame()->GetDocument().GetElementById(blink::WebString("color")));
  ExecuteJavaScriptForTests(change_value.c_str());
  std::move(run_loop).Run();
}

}  // namespace

}  // namespace autofill
