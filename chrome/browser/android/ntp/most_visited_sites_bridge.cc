// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/most_visited_sites_bridge.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/MostVisitedSitesBridge_jni.h"
#include "chrome/android/chrome_jni_headers/MostVisitedSites_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/section_type.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;
using base::android::ToJavaLongArray;
using ntp_tiles::MostVisitedSites;
using ntp_tiles::NTPTilesVector;
using ntp_tiles::SectionType;
using ntp_tiles::TileTitleSource;
using ntp_tiles::TileSource;
using ntp_tiles::TileVisualType;

namespace {

class JavaHomepageClient : public MostVisitedSites::HomepageClient {
 public:
  JavaHomepageClient(JNIEnv* env,
                     const JavaParamRef<jobject>& obj,
                     Profile* profile);

  JavaHomepageClient(const JavaHomepageClient&) = delete;
  JavaHomepageClient& operator=(const JavaHomepageClient&) = delete;

  bool IsHomepageTileEnabled() const override;
  GURL GetHomepageUrl() const override;
  void QueryHomepageTitle(TitleCallback title_callback) override;

 private:
  void OnTitleEntryFound(TitleCallback title_callback,
                         history::QueryURLResult result);

  ScopedJavaGlobalRef<jobject> client_;
  raw_ptr<Profile> profile_;

  // Used in loading titles.
  base::CancelableTaskTracker task_tracker_;
};

JavaHomepageClient::JavaHomepageClient(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       Profile* profile)
    : client_(env, obj), profile_(profile) {
  DCHECK(profile);
}

void JavaHomepageClient::QueryHomepageTitle(TitleCallback title_callback) {
  DCHECK(!title_callback.is_null());
  GURL url = GetHomepageUrl();
  if (url.is_empty()) {
    std::move(title_callback).Run(absl::nullopt);
    return;
  }
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    std::move(title_callback).Run(absl::nullopt);
    return;
  }
  // If the client is destroyed, the tracker will cancel this task automatically
  // and the callback will not be called. Therefore, base::Unretained works.
  history_service->QueryURL(
      url,
      /*want_visits=*/false,
      base::BindOnce(&JavaHomepageClient::OnTitleEntryFound,
                     base::Unretained(this), std::move(title_callback)),
      &task_tracker_);
}

void JavaHomepageClient::OnTitleEntryFound(TitleCallback title_callback,
                                           history::QueryURLResult result) {
  if (!result.success) {
    std::move(title_callback).Run(absl::nullopt);
    return;
  }
  std::move(title_callback).Run(result.row.title());
}

bool JavaHomepageClient::IsHomepageTileEnabled() const {
  return Java_HomepageClient_isHomepageTileEnabled(AttachCurrentThread(),
                                                   client_);
}

GURL JavaHomepageClient::GetHomepageUrl() const {
  base::android::ScopedJavaLocalRef<jstring> url =
      Java_HomepageClient_getHomepageUrl(AttachCurrentThread(), client_);
  if (url.is_null()) {
    return GURL();
  }
  return GURL(ConvertJavaStringToUTF8(url));
}

}  // namespace

class MostVisitedSitesBridge::JavaObserver : public MostVisitedSites::Observer {
 public:
  JavaObserver(JNIEnv* env, const JavaParamRef<jobject>& obj);

  JavaObserver(const JavaObserver&) = delete;
  JavaObserver& operator=(const JavaObserver&) = delete;

  void OnURLsAvailable(
      const std::map<SectionType, NTPTilesVector>& sections) override;

  void OnIconMadeAvailable(const GURL& site_url) override;

 private:
  ScopedJavaGlobalRef<jobject> observer_;
};

MostVisitedSitesBridge::JavaObserver::JavaObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj)
    : observer_(env, obj) {}

void MostVisitedSitesBridge::JavaObserver::OnURLsAvailable(
    const std::map<SectionType, NTPTilesVector>& sections) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<std::u16string> titles;
  std::vector<base::android::ScopedJavaLocalRef<jobject>> urls;
  std::vector<int> title_sources;
  std::vector<int> sources;
  std::vector<int> section_types;
  for (const auto& section : sections) {
    const NTPTilesVector& tiles = section.second;
    section_types.resize(section_types.size() + tiles.size(),
                         static_cast<int>(section.first));
    for (const auto& tile : tiles) {
      titles.emplace_back(tile.title);
      urls.emplace_back(url::GURLAndroid::FromNativeGURL(env, tile.url));
      title_sources.emplace_back(static_cast<int>(tile.title_source));
      sources.emplace_back(static_cast<int>(tile.source));
    }
  }
  Java_MostVisitedSitesBridge_onURLsAvailable(
      env, observer_, ToJavaArrayOfStrings(env, titles),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, urls),
      ToJavaIntArray(env, section_types),
      ToJavaIntArray(env, title_sources), ToJavaIntArray(env, sources));
}

void MostVisitedSitesBridge::JavaObserver::OnIconMadeAvailable(
    const GURL& site_url) {
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedSitesBridge_onIconMadeAvailable(
      env, observer_, url::GURLAndroid::FromNativeGURL(env, site_url));
}

MostVisitedSitesBridge::MostVisitedSitesBridge(Profile* profile)
    : most_visited_(ChromeMostVisitedSitesFactory::NewForProfile(profile)),
      profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());
}

MostVisitedSitesBridge::~MostVisitedSitesBridge() {}

void MostVisitedSitesBridge::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  delete this;
}

void MostVisitedSitesBridge::OnHomepageStateChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  most_visited_->RefreshTiles();
}

void MostVisitedSitesBridge::SetHomepageClient(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_client) {
  most_visited_->SetHomepageClient(
      std::make_unique<JavaHomepageClient>(env, j_client, profile_));
}

void MostVisitedSitesBridge::SetObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_observer,
    jint num_sites) {
  java_observer_ = std::make_unique<JavaObserver>(env, j_observer);
  most_visited_->AddMostVisitedURLsObserver(java_observer_.get(), num_sites);
}

void MostVisitedSitesBridge::AddOrRemoveBlockedUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_url,
    jboolean add_url) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_url);
  most_visited_->AddOrRemoveBlockedUrl(*url, add_url);
}

void MostVisitedSitesBridge::RecordPageImpression(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint jtiles_count) {
  ntp_tiles::metrics::RecordPageImpression(jtiles_count);
}

void MostVisitedSitesBridge::RecordTileImpression(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint jindex,
    jint jvisual_type,
    jint jicon_type,
    jint jtitle_source,
    jint jsource,
    const JavaParamRef<jobject>& jurl) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, jurl);
  TileTitleSource title_source = static_cast<TileTitleSource>(jtitle_source);
  TileSource source = static_cast<TileSource>(jsource);
  TileVisualType visual_type = static_cast<TileVisualType>(jvisual_type);
  favicon_base::IconType icon_type =
      static_cast<favicon_base::IconType>(jicon_type);

  ntp_tiles::metrics::RecordTileImpression(ntp_tiles::NTPTileImpression(
      jindex, source, title_source, visual_type, icon_type, *url));
}

void MostVisitedSitesBridge::RecordOpenedMostVisitedItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    jint tile_type,
    jint title_source,
    jint source) {
  ntp_tiles::metrics::RecordTileClick(ntp_tiles::NTPTileImpression(
      index, static_cast<TileSource>(source),
      static_cast<TileTitleSource>(title_source),
      static_cast<TileVisualType>(tile_type), favicon_base::IconType::kInvalid,
      /*url_for_rappor=*/GURL()));
}

static jlong JNI_MostVisitedSitesBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile) {
  MostVisitedSitesBridge* most_visited_sites =
      new MostVisitedSitesBridge(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
