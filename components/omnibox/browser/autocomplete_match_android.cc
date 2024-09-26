// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal_jni_wrapper.h"
#include "components/omnibox/browser/clipboard_provider.h"
#include "components/omnibox/browser/jni_headers/AutocompleteMatch_jni.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/query_tiles/android/tile_conversion_bridge.h"
#include "url/android/gurl_android.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;

// static
jclass AutocompleteMatch::GetClazz(JNIEnv* env) {
  return org_chromium_components_omnibox_AutocompleteMatch_clazz(env);
}

ScopedJavaLocalRef<jobject> AutocompleteMatch::GetOrCreateJavaObject(
    JNIEnv* env) const {
  // Short circuit if we already built the match.
  if (java_match_)
    return ScopedJavaLocalRef<jobject>(*java_match_);

  std::vector<int> contents_class_offsets;
  std::vector<int> contents_class_styles;
  for (auto contents_class_item : contents_class) {
    contents_class_offsets.push_back(contents_class_item.offset);
    contents_class_styles.push_back(contents_class_item.style);
  }

  std::vector<int> description_class_offsets;
  std::vector<int> description_class_styles;
  for (auto description_class_item : description_class) {
    description_class_offsets.push_back(description_class_item.offset);
    description_class_styles.push_back(description_class_item.style);
  }

  base::android::ScopedJavaLocalRef<jobject> janswer;
  if (answer)
    janswer = answer->CreateJavaObject();
  ScopedJavaLocalRef<jstring> j_image_dominant_color;
  ScopedJavaLocalRef<jstring> j_post_content_type;
  ScopedJavaLocalRef<jbyteArray> j_post_content;
  std::string clipboard_image_data;

  if (!image_dominant_color.empty()) {
    j_image_dominant_color = ConvertUTF8ToJavaString(env, image_dominant_color);
  }

  if (post_content && !post_content->first.empty() &&
      !post_content->second.empty()) {
    j_post_content_type = ConvertUTF8ToJavaString(env, post_content->first);
    j_post_content = ToJavaByteArray(env, post_content->second);
  }

  if (search_terms_args.get()) {
    clipboard_image_data = search_terms_args->image_thumbnail_content;
  }

  ScopedJavaLocalRef<jobject> j_query_tiles =
      query_tiles::TileConversionBridge::CreateJavaTiles(env, query_tiles);

  std::vector<std::u16string> suggest_titles;
  suggest_titles.reserve(suggest_tiles.size());
  std::vector<base::android::ScopedJavaLocalRef<jobject>> suggest_urls;
  suggest_urls.reserve(suggest_tiles.size());
  // Note: vector<bool> is a specialized version of vector that behaves
  // differently, storing values as individual bits. This makes it impossible
  // for us to use it to represent tile.is_search on the Java side.
  std::vector<int> suggest_types;
  suggest_types.reserve(suggest_tiles.size());
  for (const auto& tile : suggest_tiles) {
    suggest_titles.push_back(tile.title);
    suggest_urls.push_back(url::GURLAndroid::FromNativeGURL(env, tile.url));
    suggest_types.push_back(tile.is_search);
  }

  std::vector<int> temp_subtypes(subtypes.begin(), subtypes.end());

  java_match_ = std::make_unique<ScopedJavaGlobalRef<jobject>>(
      Java_AutocompleteMatch_build(
          env, reinterpret_cast<intptr_t>(this), type,
          ToJavaIntArray(env, temp_subtypes), IsSearchType(type), relevance,
          transition, ConvertUTF16ToJavaString(env, contents),
          ToJavaIntArray(env, contents_class_offsets),
          ToJavaIntArray(env, contents_class_styles),
          ConvertUTF16ToJavaString(env, description),
          ToJavaIntArray(env, description_class_offsets),
          ToJavaIntArray(env, description_class_styles), janswer,
          ConvertUTF16ToJavaString(env, fill_into_edit),
          url::GURLAndroid::FromNativeGURL(env, destination_url),
          url::GURLAndroid::FromNativeGURL(env, image_url),
          j_image_dominant_color, SupportsDeletion(), j_post_content_type,
          j_post_content, suggestion_group_id.value_or(omnibox::GROUP_INVALID),
          j_query_tiles, ToJavaByteArray(env, clipboard_image_data),
          has_tab_match.value_or(false),
          ToJavaArrayOfStrings(env, suggest_titles),
          url::GURLAndroid::ToJavaArrayOfGURLs(env, suggest_urls),
          ToJavaIntArray(env, suggest_types),
          ToJavaOmniboxActionsList(env, actions)));

  return ScopedJavaLocalRef<jobject>(*java_match_);
}

void AutocompleteMatch::UpdateJavaObjectNativeRef() {
  if (!java_match_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutocompleteMatch_updateNativeObjectRef(
      env, *java_match_, reinterpret_cast<intptr_t>(this));
}

void AutocompleteMatch::DestroyJavaObject() {
  if (!java_match_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutocompleteMatch_destroy(env, *java_match_);
  java_match_.reset();
}

void AutocompleteMatch::UpdateWithClipboardContent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_callback) {
  DCHECK(provider) << "No provider available";
  DCHECK(provider->type() == AutocompleteProvider::TYPE_CLIPBOARD)
      << "Invalid provider type: " << provider->type();

  ClipboardProvider* clipboard_provider =
      static_cast<ClipboardProvider*>(provider);
  clipboard_provider->UpdateClipboardMatchWithContent(
      this,
      base::BindOnce(&AutocompleteMatch::OnClipboardSuggestionContentUpdated,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void AutocompleteMatch::OnClipboardSuggestionContentUpdated(
    const base::android::JavaRef<jobject>& j_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  UpdateClipboardContent(env);
  RunRunnableAndroid(j_callback);
}

void AutocompleteMatch::UpdateMatchingJavaTab(
    const JavaObjectWeakGlobalRef& tab) {
  matching_java_tab_ = tab;

  // Default state is: we don't have a matching tab. If that default state has
  // changed, reflect it in the UI.
  // TODO(crbug.com/1266558): when Tab.java is relocated to Components, pass the
  // Tab object directly to Java. This is not possible right now due to
  // //components being explicitly denied to depend on //chrome targets.
  if (!java_match_ || !has_tab_match.value_or(false))
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutocompleteMatch_updateMatchingTab(env, *java_match_, true);
}

JavaObjectWeakGlobalRef AutocompleteMatch::GetMatchingJavaTab() const {
  return matching_java_tab_;
}

void AutocompleteMatch::UpdateClipboardContent(JNIEnv* env) {
  if (!java_match_)
    return;

  std::string clipboard_image_data;
  if (search_terms_args.get()) {
    clipboard_image_data = search_terms_args->image_thumbnail_content;
  }

  ScopedJavaLocalRef<jstring> j_post_content_type;
  ScopedJavaLocalRef<jbyteArray> j_post_content;
  if (post_content && !post_content->first.empty() &&
      !post_content->second.empty()) {
    j_post_content_type = ConvertUTF8ToJavaString(env, post_content->first);
    j_post_content = ToJavaByteArray(env, post_content->second);
  }

  Java_AutocompleteMatch_updateClipboardContent(
      env, *java_match_, ConvertUTF16ToJavaString(env, contents),
      url::GURLAndroid::FromNativeGURL(env, destination_url),
      j_post_content_type, j_post_content,
      ToJavaByteArray(env, clipboard_image_data));
}

void AutocompleteMatch::UpdateJavaDestinationUrl() {
  if (java_match_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutocompleteMatch_setDestinationUrl(
        env, *java_match_,
        url::GURLAndroid::FromNativeGURL(env, destination_url));
  }
}

void AutocompleteMatch::UpdateJavaAnswer() {
  if (java_match_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutocompleteMatch_setAnswer(
        env, *java_match_, answer ? answer->CreateJavaObject() : nullptr);
  }
}

void AutocompleteMatch::UpdateJavaDescription() {
  if (java_match_) {
    std::vector<int> description_class_offsets;
    std::vector<int> description_class_styles;
    for (auto description_class_item : description_class) {
      description_class_offsets.push_back(description_class_item.offset);
      description_class_styles.push_back(description_class_item.style);
    }
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutocompleteMatch_setDescription(
        env, *java_match_, ConvertUTF16ToJavaString(env, description),
        ToJavaIntArray(env, description_class_offsets),
        ToJavaIntArray(env, description_class_styles));
  }
}
