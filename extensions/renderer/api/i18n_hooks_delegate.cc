// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/i18n_hooks_delegate.h"

#include <vector>

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/message_bundle.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/shared_l10n_map.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

constexpr char kGetMessage[] = "i18n.getMessage";
constexpr char kGetUILanguage[] = "i18n.getUILanguage";
constexpr char kDetectLanguage[] = "i18n.detectLanguage";

// Max number of languages to detect.
const int kCldNumLangs = 3;

// CLD3 minimum reliable byte threshold. Predictions for inputs below this size
// in bytes will be considered unreliable.
const int kCld3MinimumByteThreshold = 50;

struct DetectedLanguage {
  DetectedLanguage(const std::string& language, int percentage)
      : language(language), percentage(percentage) {}

  // Returns a new v8::Local<v8::Value> representing the serialized form of
  // this DetectedLanguage object.
  v8::Local<v8::Value> ToV8(v8::Isolate* isolate) const;

  std::string language;
  int percentage;
};

// LanguageDetectionResult object that holds detected langugae reliability and
// array of DetectedLanguage
struct LanguageDetectionResult {
  LanguageDetectionResult() {}
  LanguageDetectionResult(const LanguageDetectionResult&) = delete;
  LanguageDetectionResult& operator=(const LanguageDetectionResult&) = delete;
  ~LanguageDetectionResult() {}

  // Returns a new v8::Local<v8::Value> representing the serialized form of
  // this Result object.
  v8::Local<v8::Value> ToV8(v8::Local<v8::Context> context) const;

  // CLD detected language reliability
  bool is_reliable = false;

  // Array of detectedLanguage of size 1-3. The null is returned if
  // there were no languages detected
  std::vector<DetectedLanguage> languages;
};

v8::Local<v8::Value> DetectedLanguage::ToV8(v8::Isolate* isolate) const {
  return gin::DataObjectBuilder(isolate)
      .Set("language", language)
      .Set("percentage", percentage)
      .Build();
}

v8::Local<v8::Value> LanguageDetectionResult::ToV8(
    v8::Local<v8::Context> context) const {
  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(isolate->GetCurrentContext() == context);

  v8::Local<v8::Array> v8_languages = v8::Array::New(isolate, languages.size());
  for (uint32_t i = 0; i < languages.size(); ++i) {
    bool success =
        v8_languages->CreateDataProperty(context, i, languages[i].ToV8(isolate))
            .ToChecked();
    DCHECK(success) << "CreateDataProperty() should never fail.";
  }
  return gin::DataObjectBuilder(isolate)
      .Set("isReliable", is_reliable)
      .Set("languages", v8_languages.As<v8::Value>())
      .Build();
}

void InitDetectedLanguages(
    const std::vector<chrome_lang_id::NNetLanguageIdentifier::Result>&
        lang_results,
    LanguageDetectionResult* result) {
  std::vector<DetectedLanguage>* detected_languages = &result->languages;
  DCHECK(detected_languages->empty());
  bool* is_reliable = &result->is_reliable;

  // is_reliable is set to "true", so that the reliability can be calculated by
  // &&'ing the reliability of each predicted language.
  *is_reliable = true;
  for (const auto& lang_result : lang_results) {
    const std::string& language_code = lang_result.language;

    // If a language is kUnknown, then the remaining ones are also kUnknown.
    if (language_code == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
      break;
    }

    // The list of languages supported by CLD3 is saved in kLanguageNames
    // in the following file:
    // //src/third_party/cld_3/src/src/task_context_params.cc
    // Among the entries in this list are transliterated languages
    // (called xx-Latn) which don't belong to the spec ISO639-1 used by
    // the previous model, CLD2. Thus, to maintain backwards compatibility,
    // xx-Latn predictions are ignored for now.
    if (base::EndsWith(language_code, "-Latn",
                       base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    *is_reliable = *is_reliable && lang_result.is_reliable;
    const int percent = static_cast<int>(100 * lang_result.proportion);
    detected_languages->emplace_back(language_code, percent);
  }

  if (detected_languages->empty())
    *is_reliable = false;
}

// Returns the localized method for the given |message_name| and
// substitutions. This can result in a synchronous IPC being sent to the browser
// for the first call related to an extension in this process.
v8::Local<v8::Value> GetI18nMessage(const std::string& message_name,
                                    const std::string& extension_id,
                                    v8::Local<v8::Value> v8_substitutions,
                                    v8::Local<v8::Value> v8_options,
                                    IPC::Sender* message_sender,
                                    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();

  std::string message = SharedL10nMap::GetInstance().GetMessage(
      extension_id, message_name, message_sender);

  std::vector<std::string> substitutions;
  // For now, we just suppress all errors, but that's really not the best.
  // See https://crbug.com/807769.
  v8::TryCatch try_catch(isolate);
  if (v8_substitutions->IsArray()) {
    // chrome.i18n.getMessage("message_name", ["more", "params"]);
    v8::Local<v8::Array> placeholders = v8_substitutions.As<v8::Array>();
    uint32_t count = placeholders->Length();
    if (count > 9)
      return v8::Undefined(isolate);

    for (uint32_t i = 0; i < count; ++i) {
      v8::Local<v8::Value> placeholder;
      if (!placeholders->Get(context, i).ToLocal(&placeholder))
        return v8::Undefined(isolate);
      // Note: this tries to convert each entry to a JS string, which can fail.
      // If it does, String::Utf8Value() catches the error and doesn't surface
      // it to the calling script (though the call may still be observable,
      // since this goes through an object's toString() method). If it fails,
      // we just silently ignore the value.
      v8::String::Utf8Value string_value(isolate, placeholder);
      if (*string_value)
        substitutions.push_back(*string_value);
    }
  } else if (v8_substitutions->IsString()) {
    // chrome.i18n.getMessage("message_name", "one param");
    substitutions.push_back(gin::V8ToString(isolate, v8_substitutions));
  }
  // TODO(devlin): We currently just ignore any non-string, non-array values
  // for substitutions, but the type is documented as 'any'. We should either
  // enforce type more heavily, or throw an error here.

  if (v8_options->IsObject()) {
    v8::Local<v8::Object> options = v8_options.As<v8::Object>();
    v8::Local<v8::Value> key =
        v8::String::NewFromUtf8(isolate, "escapeLt").ToLocalChecked();
    v8::Local<v8::Value> html;
    if (options->Get(context, key).ToLocal(&html) && html->IsBoolean() &&
        html.As<v8::Boolean>()->Value()) {
      base::ReplaceChars(message, "<", "&lt;", &message);
    }
  }

  // NOTE: We call ReplaceStringPlaceholders even if |substitutions| is empty
  // because we substitute $$ to be $ (in order to display a dollar sign in a
  // message). See https://crbug.com/127243.
  message = base::ReplaceStringPlaceholders(message, substitutions, nullptr);
  return gin::StringToV8(isolate, message);
}

// Returns the detected language for the sample |text|.
v8::Local<v8::Value> DetectTextLanguage(v8::Local<v8::Context> context,
                                        const std::string& text) {
  chrome_lang_id::NNetLanguageIdentifier nnet_lang_id(/*min_num_bytes=*/0,
                                                      /*max_num_bytes=*/512);
  std::vector<chrome_lang_id::NNetLanguageIdentifier::Result> lang_results =
      nnet_lang_id.FindTopNMostFreqLangs(text, kCldNumLangs);

  // is_reliable is set to false if we believe the input is too short to be
  // accurately identified by the current model.
  if (text.size() < kCld3MinimumByteThreshold) {
    for (auto& result : lang_results)
      result.is_reliable = false;
  }

  LanguageDetectionResult result;

  // Populate LanguageDetectionResult with prediction reliability, languages,
  // and the corresponding percentages.
  InitDetectedLanguages(lang_results, &result);
  return result.ToV8(context);
}

}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

I18nHooksDelegate::I18nHooksDelegate() {}
I18nHooksDelegate::~I18nHooksDelegate() = default;

RequestResult I18nHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  using Handler = RequestResult (I18nHooksDelegate::*)(
      ScriptContext*, const APISignature::V8ParseResult&);
  static constexpr struct {
    Handler handler;
    base::StringPiece method;
  } kHandlers[] = {
      {&I18nHooksDelegate::HandleGetMessage, kGetMessage},
      {&I18nHooksDelegate::HandleGetUILanguage, kGetUILanguage},
      {&I18nHooksDelegate::HandleDetectLanguage, kDetectLanguage},
  };

  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler)
    return RequestResult(RequestResult::NOT_HANDLED);

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }

  return (this->*handler)(script_context, parse_result);
}

RequestResult I18nHooksDelegate::HandleGetMessage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const std::vector<v8::Local<v8::Value>>& arguments = *parse_result.arguments;
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  DCHECK(script_context->extension());
  DCHECK(arguments[0]->IsString());

  IPC::Sender* message_sender = nullptr;
  if (script_context->IsForServiceWorker()) {
    message_sender = WorkerThreadDispatcher::Get();
  } else {
    message_sender = script_context->GetRenderFrame();
  }

  v8::Local<v8::Value> message = GetI18nMessage(
      gin::V8ToString(script_context->isolate(), arguments[0]),
      script_context->extension()->id(), arguments[1], arguments[2],
      message_sender, script_context->v8_context());

  RequestResult result(RequestResult::HANDLED);
  result.return_value = message;
  return result;
}

RequestResult I18nHooksDelegate::HandleGetUILanguage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  RequestResult result(RequestResult::HANDLED);
  const std::string lang = base::i18n::GetConfiguredLocale();
  DCHECK(!lang.empty());

  result.return_value = gin::StringToSymbol(script_context->isolate(), lang);
  return result;
}

RequestResult I18nHooksDelegate::HandleDetectLanguage(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const std::vector<v8::Local<v8::Value>>& arguments = *parse_result.arguments;
  DCHECK(arguments[0]->IsString());

  v8::Local<v8::Context> v8_context = script_context->v8_context();

  v8::Local<v8::Value> detected_languages = DetectTextLanguage(
      v8_context, gin::V8ToString(script_context->isolate(), arguments[0]));

  v8::Local<v8::Value> response_args[] = {detected_languages};
  RequestResult result(RequestResult::HANDLED);
  if (parse_result.async_type == binding::AsyncResponseType::kCallback) {
    DCHECK(arguments[1]->IsFunction());
    JSRunner::Get(v8_context)
        ->RunJSFunction(arguments[1].As<v8::Function>(), v8_context,
                        std::size(response_args), response_args);
  } else {
    DCHECK_EQ(binding::AsyncResponseType::kPromise, parse_result.async_type);
    auto promise_resolver =
        v8::Promise::Resolver::New(v8_context).ToLocalChecked();
    promise_resolver->Resolve(v8_context, response_args[0]).FromJust();
    result.return_value = promise_resolver->GetPromise();
  }

  return result;
}

}  // namespace extensions
