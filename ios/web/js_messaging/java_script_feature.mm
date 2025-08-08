// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/java_script_feature.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/web/js_messaging/java_script_content_world.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/page_script_util.h"
#import "ios/web/js_messaging/web_frame_internal.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns a JavaScript safe string based on `script_filename`. This is used as
// a unique identifier for a given script and passed to
// `MakeScriptInjectableOnce` which ensures JS isn't executed multiple times due
// to duplicate injection.
NSString* InjectionTokenForScript(NSString* script_filename) {
  NSMutableCharacterSet* valid_characters =
      [NSMutableCharacterSet alphanumericCharacterSet];
  [valid_characters addCharactersInString:@"$_"];
  NSCharacterSet* invalid_characters = valid_characters.invertedSet;
  NSString* token =
      [[script_filename componentsSeparatedByCharactersInSet:invalid_characters]
          componentsJoinedByString:@""];
  DCHECK_GT(token.length, 0ul);
  return token;
}

}  // namespace

namespace web {

#pragma mark - JavaScriptFeature::FeatureScript

JavaScriptFeature::FeatureScript
JavaScriptFeature::FeatureScript::CreateWithFilename(
    const std::string& filename,
    InjectionTime injection_time,
    TargetFrames target_frames,
    ReinjectionBehavior reinjection_behavior,
    const PlaceholderReplacementsCallback& replacements_callback) {
  NSString* injection_token =
      InjectionTokenForScript(base::SysUTF8ToNSString(filename));
  return JavaScriptFeature::FeatureScript(
      filename, /*script=*/absl::nullopt, injection_token, injection_time,
      target_frames, reinjection_behavior, replacements_callback);
}

JavaScriptFeature::FeatureScript
JavaScriptFeature::FeatureScript::CreateWithString(
    const std::string& script,
    InjectionTime injection_time,
    TargetFrames target_frames,
    ReinjectionBehavior reinjection_behavior,
    const PlaceholderReplacementsCallback& replacements_callback) {
  NSString* unique_id = [[NSProcessInfo processInfo] globallyUniqueString];
  NSString* injection_token = InjectionTokenForScript(unique_id);
  return JavaScriptFeature::FeatureScript(
      /*filename=*/absl::nullopt, script, injection_token, injection_time,
      target_frames, reinjection_behavior, replacements_callback);
}

JavaScriptFeature::FeatureScript::FeatureScript(
    absl::optional<std::string> filename,
    absl::optional<std::string> script,
    NSString* injection_token,
    InjectionTime injection_time,
    TargetFrames target_frames,
    ReinjectionBehavior reinjection_behavior,
    const PlaceholderReplacementsCallback& replacements_callback)
    : script_filename_(filename),
      script_(script),
      injection_token_(injection_token),
      injection_time_(injection_time),
      target_frames_(target_frames),
      reinjection_behavior_(reinjection_behavior),
      replacements_callback_(replacements_callback) {}

JavaScriptFeature::FeatureScript::FeatureScript(const FeatureScript&) = default;

JavaScriptFeature::FeatureScript& JavaScriptFeature::FeatureScript::operator=(
    const FeatureScript&) = default;

JavaScriptFeature::FeatureScript::FeatureScript(FeatureScript&&) = default;

JavaScriptFeature::FeatureScript& JavaScriptFeature::FeatureScript::operator=(
    FeatureScript&&) = default;

JavaScriptFeature::FeatureScript::~FeatureScript() = default;

NSString* JavaScriptFeature::FeatureScript::GetScriptString() const {
  NSString* script = nil;
  if (script_) {
    script = base::SysUTF8ToNSString(script_.value());
  } else {
    CHECK(script_filename_);
    script = GetPageScript(base::SysUTF8ToNSString(*script_filename_));
  }

  if (reinjection_behavior_ ==
      ReinjectionBehavior::kReinjectOnDocumentRecreation) {
    return ReplacePlaceholders(script);
  }
  // WKUserScript instances will automatically be re-injected by WebKit when the
  // document is re-created, even though the JavaScript context will not be
  // re-created. So the script needs to be wrapped in `MakeScriptInjectableOnce`
  // so that is is not re-injected.
  return MakeScriptInjectableOnce(injection_token_,
                                  ReplacePlaceholders(script));
}

NSString* JavaScriptFeature::FeatureScript::ReplacePlaceholders(
    NSString* script) const {
  if (replacements_callback_.is_null())
    return script;

  PlaceholderReplacements replacements = replacements_callback_.Run();
  if (!replacements)
    return script;

  for (NSString* key in replacements) {
    script = [script stringByReplacingOccurrencesOfString:key
                                               withString:replacements[key]];
  }

  return script;
}

#pragma mark - JavaScriptFeature

JavaScriptFeature::JavaScriptFeature(ContentWorld supported_world)
    : supported_world_(supported_world), weak_factory_(this) {}

JavaScriptFeature::JavaScriptFeature(
    ContentWorld supported_world,
    std::vector<const FeatureScript> feature_scripts)
    : supported_world_(supported_world),
      scripts_(feature_scripts),
      weak_factory_(this) {}

JavaScriptFeature::JavaScriptFeature(
    ContentWorld supported_world,
    std::vector<const FeatureScript> feature_scripts,
    std::vector<const JavaScriptFeature*> dependent_features)
    : supported_world_(supported_world),
      scripts_(feature_scripts),
      dependent_features_(dependent_features),
      weak_factory_(this) {}

JavaScriptFeature::~JavaScriptFeature() = default;

ContentWorld JavaScriptFeature::GetSupportedContentWorld() const {
  return supported_world_;
}

WebFramesManager* JavaScriptFeature::GetWebFramesManager(WebState* web_state) {
  return web_state->GetWebFramesManager(GetSupportedContentWorld());
}

const std::vector<const JavaScriptFeature::FeatureScript>
JavaScriptFeature::GetScripts() const {
  return scripts_;
}

const std::vector<const JavaScriptFeature*>
JavaScriptFeature::GetDependentFeatures() const {
  return dependent_features_;
}

absl::optional<std::string> JavaScriptFeature::GetScriptMessageHandlerName()
    const {
  return absl::nullopt;
}

absl::optional<JavaScriptFeature::ScriptMessageHandler>
JavaScriptFeature::GetScriptMessageHandler() const {
  if (!GetScriptMessageHandlerName()) {
    return absl::nullopt;
  }

  return base::BindRepeating(&JavaScriptFeature::ScriptMessageReceived,
                             weak_factory_.GetMutableWeakPtr());
}

void JavaScriptFeature::ScriptMessageReceived(WebState* web_state,
                                              const ScriptMessage& message) {}

bool JavaScriptFeature::CallJavaScriptFunction(
    WebFrame* web_frame,
    const std::string& function_name,
    const base::Value::List& parameters) {
  DCHECK(web_frame);

  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(web_frame->GetBrowserState());
  DCHECK(feature_manager);

  JavaScriptContentWorld* content_world =
      feature_manager->GetContentWorldForFeature(this);
  DCHECK(content_world);

  return web_frame->GetWebFrameInternal()->CallJavaScriptFunctionInContentWorld(
      function_name, parameters, content_world);
}

bool JavaScriptFeature::CallJavaScriptFunction(
    WebFrame* web_frame,
    const std::string& function_name,
    const base::Value::List& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  DCHECK(web_frame);

  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(web_frame->GetBrowserState());
  DCHECK(feature_manager);

  JavaScriptContentWorld* content_world =
      feature_manager->GetContentWorldForFeature(this);
  DCHECK(content_world);

  return web_frame->GetWebFrameInternal()->CallJavaScriptFunctionInContentWorld(
      function_name, parameters, content_world, std::move(callback), timeout);
}

}  // namespace web
