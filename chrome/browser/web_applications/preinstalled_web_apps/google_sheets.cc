// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_sheets.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

namespace {

// clang-format off
constexpr Translation kNameTranslations[] = {
    {"af", "Blaaie"},
    {"am", "ሉሆች"},
    {"ar", "جداول البيانات"},
    {"hy", "Աղյուսակներ"},
    {"az", "Cədvəl"},
    {"eu", "Kalkulu-orriak"},
    {"be", "Табліцы"},
    {"bn", "Sheets"},
    {"bg", "Таблици"},
    {"my", "စာမျက်နှာများ"},
    {"ca", "Fulls de càlcul"},
    {"zh-HK", "試算表"},
    {"zh-CN", "表格"},
    {"zh-TW", "試算表"},
    {"hr", "Listovi"},
    {"cs", "Tabulky"},
    {"da", "Regneark"},
    {"nl", "Spreadsheets"},
    {"en-GB", "Sheets"},
    {"et", "Lehed"},
    {"fil", "Mga Sheet"},
    {"fi", "Sheets"},
    {"fr", "Feuilles de calcul"},
    {"fr-CA", "Feuilles de calcul"},
    {"gl", "Follas de cálculo"},
    {"ka", "Sheets"},
    {"de", "Tabellen"},
    {"el", "Υπολογιστικά φύλλα"},
    {"gu", "Sheets"},
    {"iw", "Sheets"},
    {"hi", "Sheets"},
    {"hu", "Táblázatok"},
    {"is", "Töflureiknar"},
    {"id", "Spreadsheet"},
    {"it", "Fogli"},
    {"ja", "スプレッドシート"},
    {"kn", "Sheets"},
    {"kk", "Sheets"},
    {"km", "បញ្ជី"},
    {"ko", "스프레드시트"},
    {"lo", "​ຊີດ"},
    {"lv", "Izklājlapas"},
    {"lt", "Skaičiuoklės"},
    {"ms", "Helaian"},
    {"ml", "Sheets"},
    {"mr", "Sheets"},
    {"mn", "Хүснэгт"},
    {"ne", "पाना"},
    {"no", "Regneark"},
    {"fa", "کاربرگ‌نگار"},
    {"pl", "Arkusze"},
    {"pt-BR", "Planilhas"},
    {"pt-PT", "Sheets"},
    {"pa", "Sheets"},
    {"ro", "Foi de calcul"},
    {"ru", "Таблица"},
    {"sr", "Табеле"},
    {"si", "Sheets"},
    {"sk", "Tabuľky"},
    {"sl", "Preglednice"},
    {"es", "Hojas de cálculo"},
    {"es-419", "Hojas de cálculo"},
    {"sw", "Majedwali"},
    {"sv", "Kalkylark"},
    {"ta", "Sheets"},
    {"te", "షీట్‌లు"},
    {"th", "ชีต"},
    {"tr", "E-Tablolar"},
    {"uk", "Таблиці"},
    {"ur", "شیٹس"},
    {"vi", "Trang tính"},
    {"cy", "Dalenni"},
    {"zu", "AmaSpredishithi"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleSheets() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://docs.google.com/spreadsheets/"
                           "installwebapp?usp=chrome_default"),
      /*user_display_mode=*/mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("felcaaldnbdncclmgdcncolpebgiejap");
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Sheets", kNameTranslations));

    info->start_url =
        GURL("https://docs.google.com/spreadsheets/?usp=installed_webapp");
    info->scope = GURL("https://docs.google.com/spreadsheets/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps.any = LoadBundledIcons(
        {IDR_PREINSTALLED_WEB_APPS_GOOGLE_SHEETS_ICON_192_PNG});
    return info;
  });
  options.expected_app_id = kGoogleSheetsAppId;

  return options;
}

}  // namespace web_app
