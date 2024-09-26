// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/proto/web_app_translations.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

using Locale = std::string;

// Handles reading and writing web app translations data to disk and caching
// the translations for the current browser locale.
class WebAppTranslationManager {
 public:
  using ReadCallback = base::OnceCallback<void(
      const std::map<AppId, blink::Manifest::TranslationItem>& cache)>;
  using WriteCallback = base::OnceCallback<void(bool success)>;

  WebAppTranslationManager(Profile* profile,
                           scoped_refptr<FileUtilsWrapper> utils);
  WebAppTranslationManager(const WebAppTranslationManager&) = delete;
  WebAppTranslationManager& operator=(const WebAppTranslationManager&) = delete;
  ~WebAppTranslationManager();

  void Start();

  void WriteTranslations(
      const AppId& app_id,
      const base::flat_map<Locale, blink::Manifest::TranslationItem>&
          translations,
      WriteCallback callback);
  void DeleteTranslations(const AppId& app_id, WriteCallback callback);
  void ReadTranslations(ReadCallback callback);

  // Returns the translated web app name, if available in the browser's locale,
  // otherwise returns an empty string.
  std::string GetTranslatedName(const AppId& app_id);

  // Returns the translated web app description, if available in the browser's
  // locale, otherwise returns an empty string.
  std::string GetTranslatedDescription(const AppId& app_id);

  // TODO(crbug.com/1212519): Add a method to get the short_name.

 private:
  void OnTranslationsRead(ReadCallback callback, const AllTranslations& proto);

  base::FilePath web_apps_directory_;
  scoped_refptr<FileUtilsWrapper> utils_;
  // Cache of the translations on disk for the current device language.
  std::map<AppId, blink::Manifest::TranslationItem> translation_cache_;

  base::WeakPtrFactory<WebAppTranslationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TRANSLATION_MANAGER_H_
