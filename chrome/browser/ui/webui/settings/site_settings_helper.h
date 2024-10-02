// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"

class HostContentSettingsMap;
class Profile;

namespace content {
class WebUI;
}

namespace permissions {
class ObjectPermissionContextBase;
}

namespace site_settings {

// Maps from a secondary pattern to a setting.
typedef std::map<ContentSettingsPattern, ContentSetting> OnePatternSettings;
// Maps from a primary pattern/source pair to a OnePatternSettings. All the
// mappings in OnePatternSettings share the given primary pattern and source.
typedef std::map<std::pair<ContentSettingsPattern, std::string>,
                 OnePatternSettings>
    AllPatternsSettings;

// A set of <origin, source, incognito> tuple for organizing granted permission
// objects that belong to the same device.
using ChooserExceptionDetails = std::set<std::tuple<GURL, std::string, bool>>;

constexpr char kChooserType[] = "chooserType";
constexpr char kDisplayName[] = "displayName";
constexpr char kEmbeddingOrigin[] = "embeddingOrigin";
constexpr char kIncognito[] = "incognito";
constexpr char kObject[] = "object";
constexpr char kDisabled[] = "disabled";
constexpr char kIsolatedWebAppName[] = "isolatedWebAppName";
constexpr char kOrigin[] = "origin";
constexpr char kOriginForFavicon[] = "originForFavicon";
constexpr char kRecentPermissions[] = "recentPermissions";
constexpr char kSetting[] = "setting";
constexpr char kSites[] = "sites";
constexpr char kPolicyIndicator[] = "indicator";
constexpr char kSource[] = "source";
constexpr char kType[] = "type";
constexpr char kIsDirectory[] = "isDirectory";
constexpr char kIsEmbargoed[] = "isEmbargoed";
constexpr char kIsWritable[] = "isWritable";
constexpr char kDirectoryReadGrants[] = "directoryReadGrants";
constexpr char kDirectoryWriteGrants[] = "directoryWriteGrants";
constexpr char kFilePath[] = "filePath";
constexpr char kFileReadGrants[] = "fileReadGrants";
constexpr char kFileWriteGrants[] = "fileWriteGrants";
constexpr char kNotificationInfoString[] = "notificationInfoString";
constexpr char kPermissions[] = "permissions";
constexpr char kExtensionNameWithId[] = "extensionNameWithId";

enum class SiteSettingSource {
  kAllowlist,
  kAdsFilterBlocklist,
  kDefault,
  kEmbargo,
  kExtension,
  kHostedApp,
  kInsecureOrigin,
  kKillSwitch,
  kPolicy,
  kPreference,
  kNumSources,
};

// Returns whether a group name has been registered for the given type.
bool HasRegisteredGroupName(ContentSettingsType type);

// Converts a ContentSettingsType to/from its group name identifier.
ContentSettingsType ContentSettingsTypeFromGroupName(base::StringPiece name);
base::StringPiece ContentSettingsTypeToGroupName(ContentSettingsType type);

// Returns a list of all content settings types that correspond to permissions
// and which should be displayed in chrome://settings, for any situation not
// tied to particular a origin.
const std::vector<ContentSettingsType>& GetVisiblePermissionCategories();

// Converts a SiteSettingSource to its string identifier.
std::string SiteSettingSourceToString(const SiteSettingSource source);

// Helper function to construct a dictionary for a File System exception.
base::Value::Dict GetFileSystemExceptionForPage(
    ContentSettingsType content_type,
    Profile* profile,
    const std::string& origin,
    const base::FilePath& file_path,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito,
    bool is_embargoed = false);

// Helper function to construct a dictionary for an exception.
base::Value::Dict GetExceptionForPage(
    ContentSettingsType content_type,
    Profile* profile,
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const std::string& display_name,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito,
    bool is_embargoed = false);

// Helper function to construct a dictionary for a hosted app exception.
void AddExceptionForHostedApp(const std::string& url_pattern,
                              const extensions::Extension& app,
                              base::Value::List* exceptions);

// Fills in |exceptions| with Values for the given |type| from |profile|.
void GetExceptionsForContentType(ContentSettingsType type,
                                 Profile* profile,
                                 content::WebUI* web_ui,
                                 bool incognito,
                                 base::Value::List* exceptions);

// Fills in object saying what the current settings is for the category (such as
// enabled or blocked) and the source of that setting (such preference, policy,
// or extension).
void GetContentCategorySetting(const HostContentSettingsMap* map,
                               ContentSettingsType content_type,
                               base::Value::Dict* object);

// Retrieves the current setting for a given origin, category pair, the source
// of that setting, and its display name, which will be different if it's an
// extension. Note this is similar to GetContentCategorySetting() above but this
// goes through the PermissionManager (preferred, see https://crbug.com/739241).
ContentSetting GetContentSettingForOrigin(Profile* profile,
                                          const HostContentSettingsMap* map,
                                          const GURL& origin,
                                          ContentSettingsType content_type,
                                          std::string* source_string,
                                          std::string* display_name);

// Returns URLs with granted entries from the File System Access API.
void GetFileSystemGrantedEntries(std::vector<base::Value::Dict>* exceptions,
                                 Profile* profile,
                                 bool incognito);

// Returns all site permission exceptions for a given content type
std::vector<ContentSettingPatternSource>
GetSingleOriginExceptionsForContentType(HostContentSettingsMap* map,
                                        ContentSettingsType content_type);

// This struct facilitates lookup of a chooser context factory function by name
// for a given content settings type and is declared early so that it can used
// by functions below.
struct ChooserTypeNameEntry {
  permissions::ObjectPermissionContextBase* (*get_context)(Profile*);
  const char* name;
};

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

const ChooserTypeNameEntry* ChooserTypeFromGroupName(base::StringPiece name);

// Creates a chooser exception object for the object with |display_name|. The
// object contains the following properties
// * displayName: string,
// * object: Object,
// * chooserType: string,
// * sites: Array<SiteException>
// The structure of the SiteException objects is the same as the objects
// returned by GetExceptionForPage().
base::Value::Dict CreateChooserExceptionObject(
    const std::u16string& display_name,
    const base::Value& object,
    const std::string& chooser_type,
    const ChooserExceptionDetails& chooser_exception_details,
    Profile* profile);

// Returns an array of chooser exception objects.
base::Value::List GetChooserExceptionListFromProfile(
    Profile* profile,
    const ChooserTypeNameEntry& chooser_type);

// Returns the short name of a browser extension, or nullopt if `origin` is not
// an extension URL.
absl::optional<std::string> GetExtensionDisplayName(Profile* profile,
                                                    GURL origin);

// Takes |url| and converts it into an individual origin string or retrieves
// name of the extension or Isolated Web App it belongs to. If |hostname_only|
// is true, returns |url|'s hostname for HTTP/HTTPS pages or unknown
// extension/IWA URLs, otherwise an origin string will be returned that
// includes the scheme if it's non-cryptographic.
std::string GetDisplayNameForGURL(Profile* profile,
                                  const GURL& url,
                                  bool hostname_only);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_HELPER_H_
