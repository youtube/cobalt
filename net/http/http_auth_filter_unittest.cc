// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/logging.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif  // OS_WIN

#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_filter_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

static const char* const server_whitelist_array[] = {
  "google.com",
  "linkedin.com",
  "book.com",
  ".chromium.org",
  ".gag",
  "gog"
};

enum {
  ALL_SERVERS_MATCH = (1 << arraysize(server_whitelist_array)) - 1
};

struct UrlData {
  GURL url;
  HttpAuth::Target target;
  bool matches;
  int match_bits;
};

static const UrlData urls[] = {
  { GURL(""),
    HttpAuth::AUTH_NONE, false, 0 },
  { GURL("http://foo.cn"),
    HttpAuth::AUTH_PROXY, true, ALL_SERVERS_MATCH },
  { GURL("http://foo.cn"),
    HttpAuth::AUTH_SERVER, false, 0 },
  { GURL("http://slashdot.org"),
    HttpAuth::AUTH_NONE, false, 0 },
  { GURL("http://www.google.com"),
    HttpAuth::AUTH_SERVER, true, 1 << 0 },
  { GURL("http://www.google.com"),
    HttpAuth::AUTH_PROXY, true, ALL_SERVERS_MATCH },
  { GURL("https://login.facebook.com/login.php?login_attempt=1"),
    HttpAuth::AUTH_NONE, false, 0 },
  { GURL("http://codereview.chromium.org/634002/show"),
    HttpAuth::AUTH_SERVER, true, 1 << 3 },
  { GURL("http://code.google.com/p/chromium/issues/detail?id=34505"),
    HttpAuth::AUTH_SERVER, true, 1 << 0 },
  { GURL("http://code.google.com/p/chromium/issues/list?can=2&q=label:"
         "spdy&sort=owner&colspec=ID%20Stars%20Pri%20Area%20Type%20Status%20"
         "Summary%20Modified%20Owner%20Mstone%20OS"),
    HttpAuth::AUTH_SERVER, true, 1 << 3 },
  { GURL("https://www.linkedin.com/secure/login?trk=hb_signin"),
    HttpAuth::AUTH_SERVER, true, 1 << 1 },
  { GURL("http://www.linkedin.com/mbox?displayMBoxItem=&"
         "itemID=I1717980652_2&trk=COMM_HP_MSGVW_MEBC_MEBC&goback=.hom"),
    HttpAuth::AUTH_SERVER, true, 1 << 1 },
  { GURL("http://news.slashdot.org/story/10/02/18/190236/"
         "New-Plan-Lets-Top-HS-Students-Graduate-2-Years-Early"),
    HttpAuth::AUTH_PROXY, true, ALL_SERVERS_MATCH },
  { GURL("http://codereview.chromium.org/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true, 1 << 3 },
  { GURL("http://codereview.chromium.gag/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true, 1 << 4 },
  { GURL("http://codereview.chromium.gog/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true, 1 << 5 },
};

}   // namespace

TEST(HttpAuthFilterTest, EmptyFilter) {
  // Create an empty filter
  HttpAuthFilterWhitelist filter;
  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(urls[i].target == HttpAuth::AUTH_PROXY,
              filter.IsValid(urls[i].url, urls[i].target))
        << " " << i << ": " << urls[i].url;
  }
}

TEST(HttpAuthFilterTest, NonEmptyFilter) {
  // Create an non-empty filter
  HttpAuthFilterWhitelist filter;
  std::string server_whitelist_filter_string;
  for (size_t i = 0; i < arraysize(server_whitelist_array); ++i) {
    if (!server_whitelist_filter_string.empty())
      server_whitelist_filter_string += ",";
    server_whitelist_filter_string += "*";
    server_whitelist_filter_string += server_whitelist_array[i];
  }
  filter.SetWhitelist(server_whitelist_filter_string);
  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(urls[i].matches, filter.IsValid(urls[i].url, urls[i].target))
        << " " << i << ": " << urls[i].url;
  }
}

#if defined(OS_WIN)
namespace {

static const char16 kTopKey[] = L"Domains";

bool RegKeyExists(HKEY root, const string16& key) {
  RegKey reg_key(root, key.c_str());
  return reg_key.Valid();
}

// Split |key| into |parent_key| and |key_segment|, at the last backslash.
// If there is no backslash, then |parent_key| is empty and |key_segment| gets
// the whole key.
// Returns true if a backslash was found.
bool Split(const string16& key, string16* parent_key, string16* key_segment) {
  parent_key->clear();
  *key_segment = key;
  string16::size_type last_slash = key.rfind(L"\\");
  // Check if this is the last segment.
  if (last_slash != string16::npos) {
    *parent_key = key.substr(0, last_slash);
    *key_segment = key.substr(last_slash + 1);
    return true;
  }
  return false;       // Not allowed to destroy top-level keys.
}

// Recursively destroys registry keys, starting with |key| as long as it
// has no sub-keys or values, until it finds a |key| segment matching |top|.
// Works from the bottom up.
// We only delete the |key| segment matching |top| if |created_top| is set.
// Returns false on failure, true on success.
bool DestroyRegKeysToTop(HKEY root,
                         const string16& key,
                         const string16& top,
                         bool created_top) {
  if (key.empty())
    return false;
  RegKey reg_leaf_key(root, key.c_str());
  if (!reg_leaf_key.Valid())
    return false;       // Can't destroy a non-existent |key|.
  DWORD count = reg_leaf_key.ValueCount();
  if (count > 0)
    return false;       // Not allowed to destroy the |key| if it has values.
  RegistryKeyIterator reg_leaf_iter(root, key.c_str());
  count = reg_leaf_iter.SubkeyCount();
  if (count > 0)
    return false;       // Not allowed to destroy the |key| if it has sub-keys.
  // At this point, we know the |key| has no values or sub-keys.

  // Split into parent key, and leaf segment.
  string16 parent_key;
  string16 key_segment;
  // Check if this is the last segment.
  bool can_recurse = Split(key, &parent_key, &key_segment);
  if (!can_recurse)
    return false;       // Not allowed to destroy top-level keys.

  // Check if we've reached a key segment matching |top|.
  can_recurse &= (key_segment != top);
  if (!can_recurse && !created_top) {
    // We've reached the root, and didn't create it, so we're done.
    return true;
  }
  // Destroy the current leaf |key|.
  RegKey parent_reg_key(root, parent_key.c_str(), KEY_WRITE);
  if (!parent_reg_key.DeleteKey(key_segment.c_str()))
    return false;       // Failed to delete the |key|.

  if (can_recurse) {
    // Destroy the registry key above the current one.
    DestroyRegKeysToTop(root, parent_key, top, created_top);
  }
  return true;    // Destroyed at least the leaf |key| segment.
}

enum RegKeyCreateResult {
  REGKEY_DOESNT_EXIST,
  REGKEY_EXISTS,
  REGKEY_CREATED_TOP
};

// Recursively create a registry |key|, until it finds a |key| segment matching
// |top|.  Works from the bottom up.
// Returns 0 on failure, > 0 on success (1 on normal success, 2 if it creates
// the |top| key segment).
RegKeyCreateResult CreateRegKeyIfNotExists(HKEY root,
                                           const string16& key,
                                           const string16& top) {
  RegKeyCreateResult result = REGKEY_DOESNT_EXIST;
  if (key.empty())
    return REGKEY_DOESNT_EXIST;
  if (RegKeyExists(root, key))
    return REGKEY_EXISTS;

  // Split into parent key, and leaf segment.
  string16 parent_key;
  string16 key_segment;
  // Check if this is the last segment.
  bool can_recurse = Split(key, &parent_key, &key_segment);
  if (!can_recurse)
    return REGKEY_DOESNT_EXIST;   // Not allowed to create top-level keys.
  // Check if we've reached a segment matching |top|.
  can_recurse = (key_segment != top);
  result = REGKEY_EXISTS;
  if (can_recurse) {
    // Create the registry key above the current one.
    result = CreateRegKeyIfNotExists(root, parent_key, top);
    if (result == REGKEY_DOESNT_EXIST)
      return REGKEY_DOESNT_EXIST;
    DCHECK(RegKeyExists(root, parent_key))
        << "Unable to create registry key '" << parent_key << "'";
  } else if (!RegKeyExists(root, parent_key)) {
    // Don't have parent key, and not allowed to create it.
    return REGKEY_DOESNT_EXIST;
  }
  // Create the new key segment.
  RegKey parent_reg_key(root, parent_key.c_str(), KEY_WRITE);
  if (!parent_reg_key.CreateKey(key_segment.c_str(), KEY_WRITE)) {
    DLOG(INFO) << "Unable to create key '" << parent_key
        << "\\" << key_segment << "' in hive 0x" << root
        << ((root == HKEY_LOCAL_MACHINE) ? " (HKEY_LOCAL_MACHINE)" :
            ((root == HKEY_CURRENT_USER) ? " (HKEY_CURRENT_USER)" : ""));
    return REGKEY_DOESNT_EXIST;
  }
  if (key_segment == top)
    return REGKEY_CREATED_TOP;  // We created the |top| key segment.
  return result;
}

bool HasZoneMapKey(RegistryHiveType hive_type, const char* key_name) {
  HKEY root_key = (hive_type == CURRENT_USER) ?
                  HKEY_CURRENT_USER :
                  HKEY_LOCAL_MACHINE;
  string16 key_name_utf16 = ASCIIToUTF16(key_name);
  string16 full_key_name_utf16 = http_auth::GetRegistryWhitelistKey();
  full_key_name_utf16 += L"\\";
  full_key_name_utf16 += key_name_utf16;
  RegKey reg_leaf_key(root_key, full_key_name_utf16.c_str());
  return reg_leaf_key.Valid();
}

std::set<std::string> GetZoneMapKeys(RegistryHiveType hive_type) {
  std::set<std::string> keys;
  HKEY root_key = (hive_type == CURRENT_USER) ?
                      HKEY_CURRENT_USER :
                      HKEY_LOCAL_MACHINE;
  RegistryKeyIterator reg_base_key_iter(root_key,
                                        http_auth::GetRegistryWhitelistKey());
  DWORD count = reg_base_key_iter.SubkeyCount();
  for (DWORD k = 0; k < count; ++k, ++reg_base_key_iter)
    keys.insert(UTF16ToASCII(reg_base_key_iter.Name()));
  return keys;
}

bool SetupZoneMapEntry(RegistryHiveType hive_type,
                       const char* key_name,
                       const char16* name,
                       DWORD value,
                       bool* created_entry) {
  bool created_top = false;
  bool created = false;
  HKEY root_key = (hive_type == CURRENT_USER) ?
                      HKEY_CURRENT_USER :
                      HKEY_LOCAL_MACHINE;
  string16 key_name_utf16 = ASCIIToUTF16(key_name);
  string16 full_key_name_utf16 = http_auth::GetRegistryWhitelistKey();
  full_key_name_utf16 += L"\\";
  full_key_name_utf16 += key_name_utf16;
  RegKeyCreateResult have_key =
      CreateRegKeyIfNotExists(root_key, full_key_name_utf16, kTopKey);
  created_top = (have_key == REGKEY_CREATED_TOP);
  if (have_key != REGKEY_DOESNT_EXIST) {
    RegKey reg_leaf_key(root_key, full_key_name_utf16.c_str(), KEY_WRITE);
    created = reg_leaf_key.WriteValue(name, value);
  }
  if (created_entry)
    *created_entry = created;
  return created_top;
}

void TearDownZoneMapEntry(RegistryHiveType hive_type,
                          const char* key_name,
                          const char16* name,
                          bool created_top) {
  HKEY root_key = (hive_type == CURRENT_USER) ?
                      HKEY_CURRENT_USER :
                      HKEY_LOCAL_MACHINE;
  string16 key_name_utf16 = ASCIIToUTF16(key_name);
  string16 full_key_name_utf16 = http_auth::GetRegistryWhitelistKey();
  full_key_name_utf16 += L"\\";
  full_key_name_utf16 += key_name_utf16;
  RegKey reg_leaf_key(root_key, full_key_name_utf16.c_str(), KEY_WRITE);
  reg_leaf_key.DeleteValue(name);
  DestroyRegKeysToTop(root_key, full_key_name_utf16, kTopKey, created_top);
}

// Sets the registry whitelist key to the given value, and automatically
// restores it on destruction.
class AutoRestoreWhitelistKey {
 public:
  explicit AutoRestoreWhitelistKey(const char16* temp_value) {
    http_auth::SetRegistryWhitelistKey(temp_value);
  }
  ~AutoRestoreWhitelistKey() {
    http_auth::SetRegistryWhitelistKey(NULL);
  }
};

}  // namespace

// NOTE:  Doing unit tests that involve the Windows registry is tricky:
//        1. You may not be able to write to a particular location in the
//           registry.  Specifically, authentication is needed to write to
//           HKEY_LOCAL_MACHINE.
//        2. There may already be values in the registry.  This is handled by
//           changing the registry key that is used for testing.
//        3. The registry should be left in the same state as it was before the
//           test.  Using a different registry key helps to insure this.
//
// We want to disable tests when:
//    - The test URL's host matches a registry entry that already exists.
//    - We are unable to write an entry to the registry.
TEST(HttpAuthFilterTest, FilterFromRegistry) {
  // We want to avoid testing the writing (and later deleting) of URLs that
  // are already in the registry.
  AutoRestoreWhitelistKey auto_restore(
      L"Software\\Microsoft\\Windows\\CurrentVersion\\"
      L"Internet Settings\\ZoneMap\\ChromeHttpAuthUnitTests");

  // If set, only the LOCAL_MACHINE registry entries are valid.
  bool test_only_machine = http_auth::UseOnlyMachineSettings();

  // This array records whether or not a test URL is already in the registry.
  bool owned_urls[2][arraysize(server_whitelist_array)] = { false };
  for (size_t w = 0; w < arraysize(server_whitelist_array); ++w) {
    owned_urls[CURRENT_USER][w] =
        !test_only_machine &&
        !HasZoneMapKey(CURRENT_USER, server_whitelist_array[w]);
    owned_urls[LOCAL_MACHINE][w] =
        !HasZoneMapKey(LOCAL_MACHINE, server_whitelist_array[w]);
  }

  // This array records whether or not any of the test URLs already match some
  // in the registry.  We won't test those because the tests might come out
  // differently than we expect.
  bool skip_tests[2][arraysize(urls)] = { false };
  RegistryHiveType hives[] = { CURRENT_USER, LOCAL_MACHINE };
  const char* type_string[] = { "USER", "MACHINE" };
  for (int t = 0; t < 2; ++t) {
    std::set<std::string> keys = GetZoneMapKeys(hives[t]);
    for (size_t u = 0; u < arraysize(urls); ++u) {
      if (urls[u].url.HostNoBrackets().empty())
        continue;
      if (test_only_machine && !t) {
        skip_tests[t][u] = true;
      } else {
        std::set<std::string>::const_iterator next = keys.begin();
        std::set<std::string>::const_iterator last = keys.end();
        for (; next != last; ++next) {
          const std::string& key = *next;
          if (std::string::npos != key.find(urls[u].url.HostNoBrackets())) {
            skip_tests[t][u] = true;
            break;
          }
        }
      }
    }
  }

  // Set up our test registry entries.
  bool have_test_case = false;
  bool created_user_root = false;
  bool created_machine_root = false;
  bool created_entry = false;
  size_t bit = 1;
  bool created_roots[] = { false, false };
  size_t num_reg_entries = arraysize(http_auth::kRegistryEntries);
  size_t reg_index;
  for (size_t w = 0; w < arraysize(server_whitelist_array); ++w, bit <<= 1) {
    for (int t = 0; t < 2; ++t) {
      if (owned_urls[t][w]) {
        created_entry = false;
        reg_index = (w + t) % num_reg_entries;
        created_roots[t] |=
            SetupZoneMapEntry(
                hives[t],
                server_whitelist_array[w],
                // Cycle through the types: { HTTP, HTTPS, * }.
                http_auth::kRegistryEntries[reg_index],
                // Cycle through zones 0, 1, 2.
                (w + t) % 3,
                &created_entry);
        // If we weren't able to create the entry, we don't own it, disable the
        // test.
        have_test_case |= created_entry;
        if (!created_entry) {
          owned_urls[t][w] = false;
          // Mark any URLs that are dependent on this as not tested.
          for (size_t u = 0; u < arraysize(urls); ++u) {
            if (urls[u].match_bits & bit)
              skip_tests[t][u] = true;
          }
        }
      }
    }
  }

  // Report any problems or skipped tests.
  for (int t = 0; t < 2; ++t) {
    for (size_t w = 0; w < arraysize(server_whitelist_array); ++w) {
      if (!owned_urls[t][w]) {
        DLOG(INFO) << "Cannot write " << type_string[t] << " registry key '"
            << server_whitelist_array[w] << "'";
      }
    }
  }
  for (int t = 0; t < 2; ++t) {
    for (size_t u = 0; u < arraysize(urls); ++u) {
      if (skip_tests[t][u]) {
        DLOG(INFO) << "Skipping " << type_string[t] << " test for URL '"
            << urls[u].url << "'";
      }
    }
  }

  if (have_test_case) {
    // OK, now we're ready to start the tests!
    // Create an non-empty filter, using only registry entries.
    HttpAuthFilterWhitelist filter;
    filter.SetWhitelist("");
    for (size_t u = 0; u < arraysize(urls); u++) {
      if (!skip_tests[CURRENT_USER][u] || !skip_tests[LOCAL_MACHINE][u]) {
        EXPECT_EQ(urls[u].matches, filter.IsValid(urls[u].url, urls[u].target))
            << " " << u << ": " << urls[u].url;
      }
    }
  }

  // Tear down our test registry entries.
  for (size_t w = 0; w < arraysize(server_whitelist_array); ++w) {
    for (int t = 0; t < 2; ++t) {
      reg_index = (w + t) % num_reg_entries;
      if (owned_urls[t][w]) {
        TearDownZoneMapEntry(
            hives[t],
            server_whitelist_array[w],
            // Cycle through the types: { HTTP, HTTPS, * }.
            http_auth::kRegistryEntries[reg_index],
            created_roots[t]);
      }
    }
  }
}
#endif  // OS_WIN

}   // namespace net
