// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/google_apps_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome/bookmark_item.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/welcome_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace welcome {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GoogleApps {
  kGmail = 0,
  kYouTube = 1,
  kMaps = 2,
  kTranslate = 3,
  kNews = 4,
  kChromeWebStoreDoNotUse = 5,  // Deprecated.
  kSearch = 6,
  kCount,
};

const char* kGoogleAppsInteractionHistogram =
    "FirstRun.NewUserExperience.GoogleAppsInteraction";

constexpr const int kGoogleAppIconSize = 48;  // Pixels.

GoogleAppsHandler::GoogleAppsHandler() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Do not translate icon name as it is not human visible and needs to
  // match CSS.

  BookmarkItem gmail = {
      static_cast<int>(GoogleApps::kGmail),
      l10n_util::GetStringUTF8(IDS_WELCOME_GOOGLE_GMAIL), "gmail",
      "https://accounts.google.com/b/0/AddMailService", IDS_WELCOME_GMAIL};

  if (IsAppVariationEnabled()) {
    google_apps_.push_back({static_cast<int>(GoogleApps::kSearch),
                            l10n_util::GetStringUTF8(IDS_WELCOME_GOOGLE_SEARCH),
                            "search", "https://google.com",
                            IDS_WELCOME_SEARCH});
  } else {
    google_apps_.push_back(gmail);
  }

  google_apps_.push_back(
      {static_cast<int>(GoogleApps::kYouTube),
       l10n_util::GetStringUTF8(IDS_WELCOME_GOOGLE_APPS_YOUTUBE), "youtube",
       "https://youtube.com", IDS_WELCOME_YOUTUBE});

  google_apps_.push_back(
      {static_cast<int>(GoogleApps::kMaps),
       l10n_util::GetStringUTF8(IDS_WELCOME_GOOGLE_APPS_MAPS), "maps",
       "https://maps.google.com", IDS_WELCOME_MAPS});

  if (IsAppVariationEnabled()) {
    google_apps_.push_back(gmail);
  } else {
    google_apps_.push_back(
        {static_cast<int>(GoogleApps::kNews),
         l10n_util::GetStringUTF8(IDS_WELCOME_GOOGLE_APPS_NEWS), "news",
         "https://news.google.com", IDS_WELCOME_NEWS});
  }

  google_apps_.push_back(
      {static_cast<int>(GoogleApps::kTranslate),
       l10n_util::GetStringUTF8(IDS_WELCOME_GOOGLE_APPS_TRANSLATE), "translate",
       "https://translate.google.com", IDS_WELCOME_TRANSLATE});
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

GoogleAppsHandler::~GoogleAppsHandler() {}

void GoogleAppsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cacheGoogleAppIcon",
      base::BindRepeating(&GoogleAppsHandler::HandleCacheGoogleAppIcon,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getGoogleAppsList",
      base::BindRepeating(&GoogleAppsHandler::HandleGetGoogleAppsList,
                          base::Unretained(this)));
}

void GoogleAppsHandler::HandleCacheGoogleAppIcon(
    const base::Value::List& args) {
  CHECK_GE(args.size(), 1u);
  int app_id = args[0].GetInt();

  const BookmarkItem* selectedApp = nullptr;
  for (const auto& google_app : google_apps_) {
    if (google_app.id == app_id) {
      selectedApp = &google_app;
      break;
    }
  }
  CHECK(selectedApp);  // WebUI should not be able to pass non-existent ID.

  // Preload the favicon cache with Chrome-bundled images. Otherwise, the
  // pre-populated bookmarks don't have favicons and look bad. Favicons are
  // updated automatically when a user visits a site.
  GURL app_url = GURL(selectedApp->url);
  FaviconServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->MergeFavicon(
          app_url, app_url, favicon_base::IconType::kFavicon,
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              selectedApp->icon),
          gfx::Size(kGoogleAppIconSize, kGoogleAppIconSize));
}

void GoogleAppsHandler::HandleGetGoogleAppsList(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id,
                            BookmarkItemsToListValue(google_apps_));
}

}  // namespace welcome
