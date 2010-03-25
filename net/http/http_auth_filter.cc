// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_filter.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif

#include "base/string_util.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
#include "net/http/http_auth_filter_win.h"
#endif

namespace net {

// Using a std::set<> has the benefit of removing duplicates automatically.
typedef std::set<string16> RegistryWhitelist;

#if defined(OS_WIN)
namespace http_auth {

// The common path to all the registry keys containing domain zone information.
const char16 kRegistryWhitelistKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\"
    L"Internet Settings\\ZoneMap\\Domains";
const char16 kRegistryInternetSettings[] =
    L"Software\\Policies\\Microsoft\\Windows\\"
    L"CurrentVersion\\Internet Settings";

const char16 kSettingsMachineOnly[] = L"Security_HKLM_only";
static const char16 kRegistryHttp[] = L"http";
static const char16 kRegistryHttps[] = L"https";
static const char16 kRegistryStar[] = L"*";
const char16* kRegistryEntries[3] = {
  kRegistryHttp,
  kRegistryHttps,
  kRegistryStar
};

const char16* g_registry_whitelist_key_override = NULL;

const char16* GetRegistryWhitelistKey() {
  // If we've overridden the whitelist key, return that instead of the default.
  if (g_registry_whitelist_key_override)
    return g_registry_whitelist_key_override;
  return kRegistryWhitelistKey;
}

void SetRegistryWhitelistKey(const char16* new_whitelist_key) {
  g_registry_whitelist_key_override = new_whitelist_key;
}

bool UseOnlyMachineSettings() {
  DWORD machine_only = 0;
  // TODO(ahendrickson) -- Check if the "Use only machine settings" option is
  // enabled in the Security Zones section of the Group Policy, and return
  // false if not.

  // Get the key indicating whether or not to use only machine settings.
  RegKey InternetSettingsKey(HKEY_LOCAL_MACHINE,
                             http_auth::kRegistryInternetSettings);
  if (!InternetSettingsKey.ReadValueDW(http_auth::kSettingsMachineOnly,
                                       &machine_only)) {
    return false;
  }
  return machine_only != 0;
}

}  // namespace http_auth

namespace {

// |whitelist| is the list of whitelist entries to populate, initially empty.
//
// |subkeys| holds the list of keys from the base key to our current one.
// For example the key ".../ZoneMap/Domains/example.com/foo.bar" would have
// subkeys { "example.com", "foo.bar" }.
void GetRegistryWhitelistInfo(RegistryWhitelist* whitelist,
                              std::list<string16>* subkeys,
                              RegistryHiveType hive_type) {
  // Iterate through all the subkeys of GetRegistryWhitelistKey(), looking
  // for values whose names are in |kRegistryEntries| and whose data is less
  // than or equal to 2.  The key names are the domain names, and the values
  // are the zone that the domain is in.  Values are:
  //    0 - My Computer
  //    1 - Local Intranet Zone
  //    2 - Trusted Sites Zone (specifically, zones to access via HTTPS)
  //    3 - Internet Zone
  //    4 - Restricted Sites Zone
  // See http://support.microsoft.com/kb/182569 for more information.
  string16 full_domain_key = http_auth::GetRegistryWhitelistKey();
  HKEY hive =
      (hive_type == CURRENT_USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  // Build the key
  std::list<string16>::iterator iter = subkeys->begin();
  for (; iter != subkeys->end(); ++iter) {
    full_domain_key += L"\\";
    full_domain_key += *iter;
  }
  // Check all the sub-keys, recursively.
  RegistryKeyIterator key_iter(hive, full_domain_key.c_str());
  for (; key_iter.Valid(); ++key_iter) {
    subkeys->push_back(key_iter.Name());  // Add the new key,
    GetRegistryWhitelistInfo(whitelist, subkeys, hive_type);
    subkeys->pop_back();                  // and remove it when done.
  }
  // Check the value(s) in this key.
  RegKey key(hive, full_domain_key.c_str());
  for (size_t i = 0; i < arraysize(http_auth::kRegistryEntries); ++i) {
    DWORD val = 0;
    if (!key.ReadValueDW(http_auth::kRegistryEntries[i], &val))
      continue;
    // Check if the setting is for Trusted sites zone (2) or better.
    // TODO(ahendrickson) - Something that we need to handle (at some point) is
    // that we can have a specific url "downgraded" to the internet zone even
    // if a previous rule says it is an intranet address. (*.foo.com intranet,
    // *.external.foo.com internet). This also has to do with some user setting
    // overriding a machine setting (if allowed by policy).
    if (val > 2)
      continue;
    // TODO(ahendrickson) -- How do we handle ranges?
    // Concatenate the subkeys (in reverse order), separated with a period.
    bool start = true;
    string16 entry;
    std::list<string16>::reverse_iterator rev_iter = subkeys->rbegin();
    for (; rev_iter != subkeys->rend(); ++rev_iter) {
      if (!start)
        entry += L".";
      start = false;
      entry += *rev_iter;
    }
    if (!entry.empty())
      whitelist->insert(entry);
  }
}

void GetRegistryWhitelistInfoTop(RegistryWhitelist* whitelist) {
  std::list<string16> subkeys;
  GetRegistryWhitelistInfo(whitelist, &subkeys, LOCAL_MACHINE);
  if (!http_auth::UseOnlyMachineSettings())
    GetRegistryWhitelistInfo(whitelist, &subkeys, CURRENT_USER);
}

}  // namespace
#endif  // OS_WIN

// TODO(ahendrickson) -- Determine if we want separate whitelists for HTTP and
// HTTPS, one for both, or only an HTTP one.  My understanding is that the HTTPS
// entries in the registry mean that you are only allowed to connect to the site
// via HTTPS and still be considered 'safe'.

HttpAuthFilterWhitelist::HttpAuthFilterWhitelist() {
}

HttpAuthFilterWhitelist::~HttpAuthFilterWhitelist() {
}

void HttpAuthFilterWhitelist::UpdateRegistryWhitelist() {
  // Updates the whitelist from the Windows registry.
  // |extra_whitelist_entries_| are the ones passed in via
  // the command line, so we add those first.
  // If there are no command line entries, and no registry entries, then
  // |rules_| will be empty.
  rules_.Clear();
  // Get the registry whitelist entries.
  RegistryWhitelist registry_whitelist;
#if defined(OS_WIN)
  GetRegistryWhitelistInfoTop(&registry_whitelist);
#endif
  // Parse the saved input string using commas as separators.
  if (!extra_whitelist_entries_.empty())
    rules_.ParseFromString(extra_whitelist_entries_);
  // Add the entries from the registry.
  const std::string prefix = "*";
  RegistryWhitelist::const_iterator iter = registry_whitelist.begin();
  for (; iter != registry_whitelist.end(); ++iter) {
    const std::string& s = UTF16ToASCII(*iter);
    AddFilter(prefix + s, HttpAuth::AUTH_SERVER);
  }
}

void HttpAuthFilterWhitelist::SetWhitelist(
    const std::string& server_whitelist) {
  // Save for when we update.
  extra_whitelist_entries_ = server_whitelist;
  UpdateRegistryWhitelist();
}

bool HttpAuthFilterWhitelist::IsValid(const GURL& url,
                                      HttpAuth::Target target) const {
  if ((target != HttpAuth::AUTH_SERVER) && (target != HttpAuth::AUTH_PROXY))
    return false;
  // All proxies pass
  if (target == HttpAuth::AUTH_PROXY)
    return true;
  return rules_.Matches(url);
}

// Add a new domain |filter| to the whitelist, if it's not already there
bool HttpAuthFilterWhitelist::AddFilter(const std::string& filter,
                                        HttpAuth::Target target) {
  if ((target != HttpAuth::AUTH_SERVER) && (target != HttpAuth::AUTH_PROXY))
    return false;
  // All proxies pass
  if (target == HttpAuth::AUTH_PROXY)
    return true;
  rules_.AddRuleFromString(filter);
  return true;
}

void HttpAuthFilterWhitelist::AddRuleToBypassLocal() {
  rules_.AddRuleToBypassLocal();
}

}  // namespace net
