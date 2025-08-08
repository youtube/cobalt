// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"

#import "base/values.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/ios/common/javascript_feature_util.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

namespace {
constexpr char kScriptName[] = "bottom_sheet";
constexpr char kScriptMessageName[] = "BottomSheetMessage";
}  // namespace

absl::optional<std::string>
AutofillBottomSheetJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageName;
}

void AutofillBottomSheetJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  AutofillBottomSheetTabHelper* helper =
      AutofillBottomSheetTabHelper::FromWebState(web_state);
  if (helper) {
    helper->OnFormMessageReceived(message);
  }
}

// static
AutofillBottomSheetJavaScriptFeature*
AutofillBottomSheetJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AutofillBottomSheetJavaScriptFeature> instance;
  return instance.get();
}

AutofillBottomSheetJavaScriptFeature::AutofillBottomSheetJavaScriptFeature()
    : web::JavaScriptFeature(
          ContentWorldForAutofillJavascriptFeatures(),
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

AutofillBottomSheetJavaScriptFeature::~AutofillBottomSheetJavaScriptFeature() =
    default;

void AutofillBottomSheetJavaScriptFeature::AttachListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    web::WebFrame* frame) {
  // TODO(crbug.com/1383214): Properly handle WebFrame destruction.
  if (!frame) {
    return;
  }
  base::Value::List renderer_id_list =
      base::Value::List::with_capacity(renderer_ids.size());
  for (auto renderer_id : renderer_ids) {
    renderer_id_list.Append(static_cast<int>(renderer_id.value()));
  }
  base::Value::List parameters;
  parameters.Append(std::move(renderer_id_list));
  CallJavaScriptFunction(frame, "bottomSheet.attachListeners", parameters);
}

void AutofillBottomSheetJavaScriptFeature::DetachListeners(
    const std::set<autofill::FieldRendererId>& renderer_ids,
    web::WebFrame* frame,
    bool refocus) {
  // TODO(crbug.com/1383214): Properly handle WebFrame destruction.
  if (!frame) {
    return;
  }
  base::Value::List renderer_id_list =
      base::Value::List::with_capacity(renderer_ids.size());
  for (auto renderer_id : renderer_ids) {
    renderer_id_list.Append(static_cast<int>(renderer_id.value()));
  }
  base::Value::List parameters;
  parameters.Append(std::move(renderer_id_list));
  parameters.Append(refocus);
  CallJavaScriptFunction(frame, "bottomSheet.detachListeners", parameters);
}
