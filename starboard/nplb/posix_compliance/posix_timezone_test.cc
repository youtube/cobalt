// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <cctype>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

// The declarations for tzname, timezone, and daylight are provided by <time.h>.
// We use static_assert to ensure they have the expected types as defined by
// POSIX.

// POSIX defines tzname as "char *tzname[]", but musl and glibc define it as
// "char *tzname[2]". This check allows either.
static_assert(std::is_same_v<decltype(tzname), char* [2]> ||
                  std::is_same_v<decltype(tzname), char*[]>,
              "Type of tzname must be 'char* [2]' or 'char* []'.");

static_assert(std::is_same_v<decltype(timezone), long>,
              "Type of timezone must be 'long'.");
static_assert(std::is_same_v<decltype(daylight), int>,
              "Type of daylight must be 'int'.");

// Number of seconds in an hour.
constexpr int kHour = 3600;

// Helper class to manage the TZ environment variable for test isolation.
// Sets TZ in constructor, restores original TZ in destructor.
// Calls tzset() after each change to TZ.
class ScopedTZ {
 public:
  ScopedTZ(const char* new_tz_value) {
    const char* current_tz_env = getenv("TZ");
    if (current_tz_env != nullptr) {
      original_tz_value_ = current_tz_env;  // Store the original TZ string
    }

    if (new_tz_value != nullptr) {
      // NOTE: Using EXPECT_EQ because ASSERT_EQ and FAIL() cannot be used in a
      // constructor as they may return void. A failure here will be reported
      // but the test will continue, which may lead to subsequent failures.
      EXPECT_EQ(0, setenv("TZ", new_tz_value, 1))
          << "ScopedTZ: Failed to set TZ environment variable to \""
          << new_tz_value << "\"";
    } else {
      // Unset the TZ variable if new_tz_value is nullptr.
      EXPECT_EQ(0, unsetenv("TZ"))
          << "ScopedTZ: Failed to unset TZ environment variable.";
    }
    tzset();
  }

  ~ScopedTZ() {
    if (original_tz_value_) {
      setenv("TZ", original_tz_value_->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
  }

  // Delete copy constructor and copy assignment operator to prevent misuse
  ScopedTZ(const ScopedTZ&) = delete;
  ScopedTZ& operator=(const ScopedTZ&) = delete;

 private:
  std::optional<std::string> original_tz_value_;
};

// Unified data structure for all IANA timezone tests.
// If a timezone does not observe Daylight Saving Time, expected_tzname_dst
// should be empty.
struct TestData {
  std::string iana_tz_name;
  long expected_timezone;
  std::string expected_tzname_std;
  std::string expected_tzname_dst;  // Empty if no DST.

  // Custom printer for TestData for readable failure messages.
  friend void PrintTo(const TestData& data, ::std::ostream* os) {
    *os << "{ iana_tz_name: \"" << data.iana_tz_name << "\""
        << ", expected_timezone: " << data.expected_timezone
        << ", expected_tzname_std: \"" << data.expected_tzname_std << "\"";
    if (!data.expected_tzname_dst.empty()) {
      *os << ", expected_tzname_dst: \"" << data.expected_tzname_dst << "\"";
    }
    *os << " }\n";
  }
};

class IANATimezoneTests : public ::testing::TestWithParam<TestData> {
 public:
  // Note: This list of test cases is extensive but does not include all zones
  // known by ICU.
  static const std::vector<TestData>& GetTests() {
    static const std::vector<TestData> all_tests = {
        // Africa.
        //   https://data.iana.org/time-zones/tzdb/africa
        TestData{"Africa/Abidjan", 0, "GMT"},
        TestData{"Africa/Accra", 0, "GMT"},
        TestData{"Africa/Addis_Ababa", -3 * kHour, "EAT"},
        TestData{"Africa/Algiers", -1 * kHour, "CET"},
        TestData{"Africa/Bamako", 0, "GMT"},
        TestData{"Africa/Bangui", -1 * kHour, "WAT"},
        TestData{"Africa/Banjul", 0, "GMT"},
        TestData{"Africa/Bissau", 0, "GMT"},
        TestData{"Africa/Blantyre", -2 * kHour, "CAT"},
        TestData{"Africa/Brazzaville", -1 * kHour, "WAT"},
        TestData{"Africa/Bujumbura", -2 * kHour, "CAT"},
        TestData{"Africa/Cairo", -2 * kHour, "EET", "EEST"},
        TestData{"Africa/Conakry", 0, "GMT"},
        TestData{"Africa/Dakar", 0, "GMT"},
        TestData{"Africa/Dar_es_Salaam", -3 * kHour, "EAT"},
        TestData{"Africa/Djibouti", -3 * kHour, "EAT"},
        TestData{"Africa/Douala", -1 * kHour, "WAT"},
        TestData{"Africa/Freetown", 0, "GMT"},
        TestData{"Africa/Gaborone", -2 * kHour, "CAT"},
        TestData{"Africa/Harare", -2 * kHour, "CAT"},
        TestData{"Africa/Johannesburg", -2 * kHour, "SAST"},
        TestData{"Africa/Juba", -2 * kHour, "CAT"},
        TestData{"Africa/Kampala", -3 * kHour, "EAT"},
        TestData{"Africa/Khartoum", -2 * kHour, "CAT"},
        TestData{"Africa/Kigali", -2 * kHour, "CAT"},
        TestData{"Africa/Kinshasa", -1 * kHour, "WAT"},
        TestData{"Africa/Lagos", -1 * kHour, "WAT"},
        TestData{"Africa/Libreville", -1 * kHour, "WAT"},
        TestData{"Africa/Lome", 0, "GMT"},
        TestData{"Africa/Luanda", -1 * kHour, "WAT"},
        TestData{"Africa/Lubumbashi", -2 * kHour, "CAT"},
        TestData{"Africa/Lusaka", -2 * kHour, "CAT"},
        TestData{"Africa/Malabo", -1 * kHour, "WAT"},
        TestData{"Africa/Maputo", -2 * kHour, "CAT"},
        TestData{"Africa/Maseru", -2 * kHour, "SAST"},
        TestData{"Africa/Mbabane", -2 * kHour, "SAST"},
        TestData{"Africa/Mogadishu", -3 * kHour, "EAT"},
        TestData{"Africa/Monrovia", 0, "GMT"},
        TestData{"Africa/Nairobi", -3 * kHour, "EAT"},
        TestData{"Africa/Ndjamena", -1 * kHour, "WAT"},
        TestData{"Africa/Niamey", -1 * kHour, "WAT"},
        TestData{"Africa/Nouakchott", 0, "GMT"},
        TestData{"Africa/Ouagadougou", 0, "GMT"},
        TestData{"Africa/Porto-Novo", -1 * kHour, "WAT"},
        TestData{"Africa/Sao_Tome", 0, "GMT"},
        TestData{"Africa/Timbuktu", 0, "GMT"},
        TestData{"Africa/Tripoli", -2 * kHour, "EET"},
        TestData{"Africa/Tunis", -1 * kHour, "CET"},
        TestData{"Africa/Windhoek", -2 * kHour, "CAT"},

        // North America.
        //   https://data.iana.org/time-zones/tzdb/northamerica
        TestData{"America/Adak", 10 * kHour, "HST", "HDT"},
        TestData{"America/Anchorage", 9 * kHour, "AKST", "AKDT"},
        TestData{"America/Anguilla", 4 * kHour, "AST"},
        TestData{"America/Antigua", 4 * kHour, "AST"},
        TestData{"America/Aruba", 4 * kHour, "AST"},
        TestData{"America/Atikokan", 5 * kHour, "EST"},
        TestData{"America/Atka", 10 * kHour, "HST", "HDT"},
        TestData{"America/Bahia_Banderas", 6 * kHour, "CST"},
        TestData{"America/Barbados", 4 * kHour, "AST"},
        TestData{"America/Belize", 6 * kHour, "CST"},
        TestData{"America/Blanc-Sablon", 4 * kHour, "AST"},
        TestData{"America/Boise", 7 * kHour, "MST", "MDT"},
        TestData{"America/Cambridge_Bay", 7 * kHour, "MST", "MDT"},
        TestData{"America/Cancun", 5 * kHour, "EST"},
        TestData{"America/Cayman", 5 * kHour, "EST"},
        TestData{"America/Chicago", 6 * kHour, "CST", "CDT"},
        TestData{"America/Chihuahua", 6 * kHour, "CST"},
        TestData{"America/Ciudad_Juarez", 7 * kHour, "MST", "MDT"},
        TestData{"America/Coral_Harbour", 5 * kHour, "EST"},
        TestData{"America/Costa_Rica", 6 * kHour, "CST"},
        TestData{"America/Creston", 7 * kHour, "MST"},
        TestData{"America/Curacao", 4 * kHour, "AST"},
        TestData{"America/Dawson_Creek", 7 * kHour, "MST"},
        TestData{"America/Dawson", 7 * kHour, "MST"},
        TestData{"America/Denver", 7 * kHour, "MST", "MDT"},
        TestData{"America/Detroit", 5 * kHour, "EST", "EDT"},
        TestData{"America/Dominica", 4 * kHour, "AST"},
        TestData{"America/Edmonton", 7 * kHour, "MST", "MDT"},
        TestData{"America/El_Salvador", 6 * kHour, "CST"},
        TestData{"America/Ensenada", 8 * kHour, "PST", "PDT"},
        TestData{"America/Fort_Nelson", 7 * kHour, "MST"},
        TestData{"America/Fort_Wayne", 5 * kHour, "EST", "EDT"},
        TestData{"America/Glace_Bay", 4 * kHour, "AST", "ADT"},
        TestData{"America/Goose_Bay", 4 * kHour, "AST", "ADT"},
        TestData{"America/Grand_Turk", 5 * kHour, "EST", "EDT"},
        TestData{"America/Grenada", 4 * kHour, "AST"},
        TestData{"America/Guadeloupe", 4 * kHour, "AST"},
        TestData{"America/Guatemala", 6 * kHour, "CST"},
        TestData{"America/Halifax", 4 * kHour, "AST", "ADT"},
        TestData{"America/Havana", 5 * kHour, "CST", "CDT"},
        TestData{"America/Hermosillo", 7 * kHour, "MST"},
        TestData{"America/Indiana/Indianapolis", 5 * kHour, "EST", "EDT"},
        TestData{"America/Indiana/Knox", 6 * kHour, "CST", "CDT"},
        TestData{"America/Indiana/Marengo", 5 * kHour, "EST", "EDT"},
        TestData{"America/Indiana/Petersburg", 5 * kHour, "EST", "EDT"},
        TestData{"America/Indiana/Tell_City", 6 * kHour, "CST", "CDT"},
        TestData{"America/Indiana/Vevay", 5 * kHour, "EST", "EDT"},
        TestData{"America/Indiana/Vincennes", 5 * kHour, "EST", "EDT"},
        TestData{"America/Indiana/Winamac", 5 * kHour, "EST", "EDT"},
        TestData{"America/Indianapolis", 5 * kHour, "EST", "EDT"},
        TestData{"America/Inuvik", 7 * kHour, "MST", "MDT"},
        TestData{"America/Iqaluit", 5 * kHour, "EST", "EDT"},
        TestData{"America/Juneau", 9 * kHour, "AKST", "AKDT"},
        TestData{"America/Kentucky/Louisville", 5 * kHour, "EST", "EDT"},
        TestData{"America/Kentucky/Monticello", 5 * kHour, "EST", "EDT"},
        TestData{"America/Knox_IN", 6 * kHour, "CST", "CDT"},
        TestData{"America/Kralendijk", 4 * kHour, "AST"},
        TestData{"America/Los_Angeles", 8 * kHour, "PST", "PDT"},
        TestData{"America/Louisville", 5 * kHour, "EST", "EDT"},
        TestData{"America/Lower_Princes", 4 * kHour, "AST"},
        TestData{"America/Managua", 6 * kHour, "CST"},
        TestData{"America/Marigot", 4 * kHour, "AST"},
        TestData{"America/Martinique", 4 * kHour, "AST"},
        TestData{"America/Matamoros", 6 * kHour, "CST", "CDT"},
        TestData{"America/Mazatlan", 7 * kHour, "MST"},
        TestData{"America/Menominee", 6 * kHour, "CST", "CDT"},
        TestData{"America/Merida", 6 * kHour, "CST"},
        TestData{"America/Metlakatla", 9 * kHour, "AKST", "AKDT"},
        TestData{"America/Mexico_City", 6 * kHour, "CST"},
        TestData{"America/Moncton", 4 * kHour, "AST", "ADT"},
        TestData{"America/Monterrey", 6 * kHour, "CST"},
        TestData{"America/Montreal", 5 * kHour, "EST", "EDT"},
        TestData{"America/Montserrat", 4 * kHour, "AST"},
        TestData{"America/Nassau", 5 * kHour, "EST", "EDT"},
        TestData{"America/New_York", 5 * kHour, "EST", "EDT"},
        TestData{"America/Nipigon", 5 * kHour, "EST", "EDT"},
        TestData{"America/Nome",  // codespell:ignore nome
                 9 * kHour, "AKST", "AKDT"},
        TestData{"America/North_Dakota/Beulah", 6 * kHour, "CST", "CDT"},
        TestData{"America/North_Dakota/Center", 6 * kHour, "CST", "CDT"},
        TestData{"America/North_Dakota/New_Salem", 6 * kHour, "CST", "CDT"},
        TestData{"America/Ojinaga", 6 * kHour, "CST", "CDT"},
        TestData{"America/Panama", 5 * kHour, "EST"},
        TestData{"America/Pangnirtung", 5 * kHour, "EST", "EDT"},
        TestData{"America/Phoenix", 7 * kHour, "MST"},
        TestData{"America/Port_of_Spain", 4 * kHour, "AST"},
        TestData{"America/Port-au-Prince", 5 * kHour, "EST", "EDT"},
        TestData{"America/Puerto_Rico", 4 * kHour, "AST"},
        TestData{"America/Rainy_River", 6 * kHour, "CST", "CDT"},
        TestData{"America/Rankin_Inlet", 6 * kHour, "CST", "CDT"},
        TestData{"America/Regina", 6 * kHour, "CST"},
        TestData{"America/Resolute", 6 * kHour, "CST", "CDT"},
        TestData{"America/Santa_Isabel", 8 * kHour, "PST", "PDT"},
        TestData{"America/Santo_Domingo", 4 * kHour, "AST"},
        TestData{"America/Shiprock", 7 * kHour, "MST", "MDT"},
        TestData{"America/Sitka", 9 * kHour, "AKST", "AKDT"},
        TestData{"America/St_Barthelemy", 4 * kHour, "AST"},
        TestData{"America/St_Johns", static_cast<long>(3.5 * kHour), "NST",
                 "NDT"},
        TestData{"America/St_Kitts", 4 * kHour, "AST"},
        TestData{"America/St_Lucia", 4 * kHour, "AST"},
        TestData{"America/St_Thomas", 4 * kHour, "AST"},
        TestData{"America/St_Vincent", 4 * kHour, "AST"},
        TestData{"America/Swift_Current", 6 * kHour, "CST"},
        TestData{"America/Tegucigalpa", 6 * kHour, "CST"},
        TestData{"America/Thunder_Bay", 5 * kHour, "EST", "EDT"},
        TestData{"America/Tijuana", 8 * kHour, "PST", "PDT"},
        TestData{"America/Toronto", 5 * kHour, "EST", "EDT"},
        TestData{"America/Tortola", 4 * kHour, "AST"},
        TestData{"America/Vancouver", 8 * kHour, "PST", "PDT"},
        TestData{"America/Virgin", 4 * kHour, "AST"},
        TestData{"America/Whitehorse", 7 * kHour, "MST"},
        TestData{"America/Winnipeg", 6 * kHour, "CST", "CDT"},
        TestData{"America/Yakutat", 9 * kHour, "AKST", "AKDT"},
        TestData{"America/Yellowknife", 7 * kHour, "MST", "MDT"},
        TestData{"Atlantic/Bermuda", 4 * kHour, "AST", "ADT"},
        TestData{"Canada/Saskatchewan", 6 * kHour, "CST"},
        TestData{"Canada/Yukon", 7 * kHour, "MST"},
        TestData{"Canada/Atlantic", 4 * kHour, "AST", "ADT"},
        TestData{"Canada/Central", 6 * kHour, "CST", "CDT"},
        TestData{"Canada/East-Saskatchewan", 6 * kHour, "CST"},
        TestData{"Canada/Eastern", 5 * kHour, "EST", "EDT"},
        TestData{"Canada/Mountain", 7 * kHour, "MST", "MDT"},
        TestData{"Canada/Newfoundland", static_cast<long>(3.5 * kHour), "NST",
                 "NDT"},
        TestData{"Canada/Pacific", 8 * kHour, "PST", "PDT"},
        TestData{"Pacific/Honolulu", 10 * kHour, "HST"},

        // South America.
        //   https://data.iana.org/time-zones/tzdb/southamerica
        TestData{"America/Argentina/Buenos_Aires", 3 * kHour, "ART"},
        TestData{"America/Asuncion", 3 * kHour, "PYT"},
        TestData{"America/Bogota", 5 * kHour, "COT"},
        TestData{"America/Caracas", 4 * kHour, "VET"},
        TestData{"America/Cayenne", 3 * kHour, "GFT"},
        TestData{"America/Guyana", 4 * kHour, "GYT"},
        TestData{"America/La_Paz", 4 * kHour, "BOT"},
        TestData{"America/Lima", 5 * kHour, "PET"},
        TestData{"America/Montevideo", 3 * kHour, "UYT"},
        TestData{"America/Paramaribo", 3 * kHour, "SRT"},
        TestData{"America/Santiago", 4 * kHour, "CLT", "CLST"},
        TestData{"America/Sao_Paulo", 3 * kHour, "BRT"},

        // Australasia.
        //   https://data.iana.org/time-zones/tzdb/australasia
        TestData{"Antarctica/Macquarie", -10 * kHour, "AEST", "AEDT"},
        TestData{"Antarctica/McMurdo", -12 * kHour, "NZST", "NZDT"},
        TestData{"Antarctica/South_Pole", -12 * kHour, "NZST", "NZDT"},
        TestData{"Australia/ACT", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Adelaide", -static_cast<long>(9.5 * kHour), "ACST",
                 "ACDT"},
        TestData{"Australia/Brisbane", -10 * kHour, "AEST"},
        TestData{"Australia/Broken_Hill", -static_cast<long>(9.5 * kHour),
                 "ACST", "ACDT"},
        TestData{"Australia/Canberra", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Currie", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Darwin", -static_cast<long>(9.5 * kHour), "ACST"},
        TestData{"Australia/Hobart", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Lindeman", -10 * kHour, "AEST"},
        TestData{"Australia/Melbourne", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/North", -static_cast<long>(9.5 * kHour), "ACST"},
        TestData{"Australia/NSW", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Perth", -8 * kHour, "AWST"},
        TestData{"Australia/Queensland", -10 * kHour, "AEST"},
        TestData{"Australia/South", -static_cast<long>(9.5 * kHour), "ACST",
                 "ACDT"},
        TestData{"Australia/Sydney", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Tasmania", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/Victoria", -10 * kHour, "AEST", "AEDT"},
        TestData{"Australia/West", -8 * kHour, "AWST", "AWDT"},
        TestData{"Australia/Yancowinna", -static_cast<long>(9.5 * kHour),
                 "ACST", "ACDT"},

        // Asia.
        //   https://data.iana.org/time-zones/tzdb/asia
        TestData{"Asia/Beirut", -2 * kHour, "EET", "EEST"},
        TestData{"Asia/Calcutta", -static_cast<long>(5.5 * kHour),
                 "IST"},  // codespell:ignore ist
        TestData{"Asia/Dhaka", -6 * kHour, "BST"},
        TestData{"Asia/Famagusta", -2 * kHour, "EET", "EEST"},
        TestData{"Asia/Gaza", -2 * kHour, "EET", "EEST"},
        TestData{"Asia/Harbin", -8 * kHour, "CST"},
        TestData{"Asia/Hebron", -2 * kHour, "EET", "EEST"},
        TestData{"Asia/Hong_Kong", -8 * kHour, "HKT"},
        TestData{"Asia/Istanbul", -3 * kHour, "TRT"},
        TestData{"Asia/Jerusalem", -2 * kHour, "IST",  // codespell:ignore ist
                 "IDT"},
        TestData{"Asia/Jakarta", -7 * kHour, "WIB"},
        TestData{"Asia/Jayapura", -9 * kHour, "WIT"},  // codespell:ignore wit
        TestData{"Asia/Karachi", -5 * kHour, "PKT"},
        TestData{"Asia/Kolkata", -static_cast<long>(5.5 * kHour),
                 "IST"},  // codespell:ignore ist
        TestData{"Asia/Macao", -8 * kHour, "CST"},
        TestData{"Asia/Macau", -8 * kHour, "CST"},
        TestData{"Asia/Makassar", -8 * kHour, "WITA"},
        TestData{"Asia/Manila", -8 * kHour, "PHST"},
        TestData{"Asia/Nicosia", -2 * kHour, "EET", "EEST"},
        TestData{"Asia/Pontianak", -7 * kHour, "WIB"},
        TestData{"Asia/Seoul", -9 * kHour, "KST"},
        TestData{"Asia/Singapore", -8 * kHour, "SGT"},
        TestData{"Asia/Taipei", -8 * kHour, "CST"},
        TestData{"Asia/Tel_Aviv", -2 * kHour, "IST",  // codespell:ignore ist
                 "IDT"},
        TestData{"Asia/Tokyo", -9 * kHour, "JST"},
        TestData{"Asia/Ujung_Pandang", -8 * kHour, "WITA"},
        TestData{"Atlantic/Reykjavik", 0, "GMT"},
        TestData{"Atlantic/St_Helena", 0, "GMT"},

        // Europe.
        //   https://data.iana.org/time-zones/tzdb/europe
        TestData{"Africa/Ceuta", -1 * kHour, "CET", "CEST"},
        TestData{"America/Danmarkshavn", 0, "GMT"},
        TestData{"America/Thule", 4 * kHour, "AST", "ADT"},
        TestData{"Arctic/Longyearbyen", -1 * kHour, "CET", "CEST"},
        TestData{"Atlantic/Canary", 0, "WET", "WEST"},
        TestData{"Atlantic/Faeroe", 0, "WET", "WEST"},
        TestData{"Atlantic/Faroe", 0, "WET", "WEST"},
        TestData{"Atlantic/Madeira", 0, "WET", "WEST"},
        TestData{"Europe/Amsterdam", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Andorra", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Athens", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Belfast", 0, "GMT", "BST"},
        TestData{"Europe/Belgrade", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Berlin", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Bratislava", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Brussels", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Bucharest", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Budapest", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Busingen", -1 * kHour, "CET"},
        TestData{"Europe/Chisinau", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Copenhagen", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Dublin", 0, "GMT", "IST"},  // codespell:ignore ist
        TestData{"Europe/Gibraltar", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Guernsey", 0, "GMT", "BST"},
        TestData{"Europe/Helsinki", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Isle_of_Man", 0, "GMT", "BST"},
        TestData{"Europe/Jersey", 0, "GMT", "BST"},
        TestData{"Europe/Kiev", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Kyiv", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Lisbon", 0, "WET", "WEST"},
        TestData{"Europe/Ljubljana", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/London", 0, "GMT", "BST"},
        TestData{"Europe/Luxembourg", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Madrid", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Malta", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Mariehamn", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Monaco", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Nicosia", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Oslo", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Paris", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Podgorica", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Prague", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Riga", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Rome", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/San_Marino", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Sarajevo", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Skopje", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Sofia", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Stockholm", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Tallinn", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Tirane", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Tiraspol", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Uzhgorod", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Vaduz", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Vatican", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Vienna", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Vilnius", -2 * kHour, "EET", "EEST"},
        TestData{"Europe/Warsaw", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Zagreb", -1 * kHour, "CET", "CEST"},
        TestData{"Europe/Zaporozhye", -2 * kHour, "EET"},
        TestData{"Europe/Zurich", -1 * kHour, "CET", "CEST"},

        // Legacy Aliases.
        //   https://data.iana.org/time-zones/tzdb/backward
        //   https://data.iana.org/time-zones/tzdb/backzone
        TestData{"Atlantic/Jan_Mayen", -1 * kHour, "CET", "CEST"},
        TestData{"CET", -1 * kHour, "CET", "CEST"},
        TestData{"CST6CDT", 6 * kHour, "CST", "CDT"},
        TestData{"Cuba", 5 * kHour, "CST", "CDT"},
        TestData{"EET", -2 * kHour, "EET", "EEST"},
        TestData{"Egypt", -2 * kHour, "EET", "EEST"},
        TestData{"Eire", 0, "GMT", "IST"},  // codespell:ignore ist
        TestData{"EST", 5 * kHour, "EST"},
        TestData{"EST5EDT", 5 * kHour, "EST", "EDT"},
        TestData{"Etc/GMT-0", 0, "GMT"},
        TestData{"Etc/GMT+0", 0, "GMT"},
        TestData{"Etc/GMT0", 0, "GMT"},
        TestData{"Etc/Greenwich", 0, "GMT"},
        TestData{"Etc/UCT", 0, "UTC"},
        TestData{"Etc/Universal", 0, "UTC"},
        TestData{"Etc/Zulu", 0, "UTC"},
        TestData{"GB-Eire", 0, "GMT", "BST"},
        TestData{"GB", 0, "GMT", "BST"},
        TestData{"GMT-0", 0, "GMT"},
        TestData{"GMT+0", 0, "GMT"},
        TestData{"GMT0", 0, "GMT"},
        TestData{"Greenwich", 0, "GMT"},
        TestData{"Hongkong", -8 * kHour, "HKT", "HKST"},
        TestData{"HST", 10 * kHour, "HST", "HPT"},
        TestData{"Iceland", 0, "GMT"},
        TestData{"Indian/Antananarivo", -3 * kHour, "EAT"},
        TestData{"Indian/Comoro", -3 * kHour, "EAT"},
        TestData{"Indian/Mayotte", -3 * kHour, "EAT"},
        TestData{"Israel", -2 * kHour, "IST", "IDT"},  // codespell:ignore ist
        TestData{"Jamaica", 5 * kHour, "EST", "EDT"},
        TestData{"Japan", -9 * kHour, "JST"},
        TestData{"Libya", -2 * kHour, "EET"},
        TestData{"MET", -1 * kHour, "CET", "CEST"},
        TestData{"Mexico/BajaNorte", 8 * kHour, "PST", "PDT"},
        TestData{"Mexico/BajaSur", 7 * kHour, "MST", "MDT"},
        TestData{"Mexico/General", 6 * kHour, "CST", "CDT"},
        TestData{"MST", 7 * kHour, "MST", "MDT"},
        TestData{"MST7MDT", 7 * kHour, "MST", "MDT"},
        TestData{"Navajo", 7 * kHour, "MST", "MDT"},
        TestData{"NZ", -12 * kHour, "NZST", "NZDT"},
        TestData{"Pacific/Auckland", -12 * kHour, "NZST", "NZDT"},
        TestData{"Pacific/Guam", -10 * kHour, "ChST"},
        TestData{"Pacific/Johnston", 10 * kHour, "HST"},
        TestData{"Pacific/Midway", 11 * kHour, "SST"},
        TestData{"Pacific/Pago_Pago", 11 * kHour, "SST"},
        TestData{"Pacific/Saipan", -10 * kHour, "ChST"},
        TestData{"Pacific/Samoa", 11 * kHour, "SST"},
        TestData{"Poland", -1 * kHour, "CET", "CEST"},
        TestData{"Portugal", 0, "WET", "WEST"},
        TestData{"PST8PDT", 8 * kHour, "PST", "PDT"},
        TestData{"ROC", -8 * kHour, "CST", "CDT"},
        TestData{"ROK", -9 * kHour, "KST", "KDT"},
        TestData{"Turkey", -3 * kHour, "TRT"},
        TestData{"UCT", 0, "UTC"},
        TestData{"Universal", 0, "UTC"},
        TestData{"US/Alaska", 9 * kHour, "AKST", "AKDT"},
        TestData{"US/Aleutian", 10 * kHour, "HST", "HDT"},
        TestData{"US/Arizona", 7 * kHour, "MST"},
        TestData{"US/Central", 6 * kHour, "CST", "CDT"},
        TestData{"US/East-Indiana", 5 * kHour, "EST"},
        TestData{"US/Eastern", 5 * kHour, "EST", "EDT"},
        TestData{"US/Hawaii", 10 * kHour, "HST"},
        TestData{"US/Indiana-Starke", 6 * kHour, "CST", "CDT"},
        TestData{"US/Michigan", 5 * kHour, "EST", "EDT"},
        TestData{"US/Mountain", 7 * kHour, "MST", "MDT"},
        TestData{"US/Pacific", 8 * kHour, "PST", "PDT"},
        TestData{"US/Samoa", 11 * kHour, "SST"},
        TestData{"UTC", 0, "UTC"},
        TestData{"WET", 0, "WET", "WEST"},
        TestData{"Zulu", 0, "UTC"},

        // Etcetera.
        //   https://data.iana.org/time-zones/tzdb/etcetera
        TestData{"Etc/UTC", 0, "UTC"},
        TestData{"Etc/GMT", 0, "GMT"},
        TestData{"GMT", 0, "GMT"},
    };
    return all_tests;
  }
};

// The unified test case for all enabled IANA timezones.
// It conditionally validates DST properties based on whether
// `expected_tzname_dst` is provided.
TEST_P(IANATimezoneTests, HandlesTimezone) {
  const auto& param = GetParam();
  ScopedTZ tz_manager(param.iana_tz_name.c_str());

  // Assert that the global variables match the expected values.
  EXPECT_EQ(timezone, param.expected_timezone)
      << "Timezone offset mismatch for " << param.iana_tz_name;

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.expected_tzname_std.c_str())
      << "Standard TZ name mismatch for " << param.iana_tz_name;

  if (!param.expected_tzname_dst.empty()) {
    // This is a timezone WITH daylight savings, so we check the DST name
    // and that the daylight flag is set.
    EXPECT_EQ(daylight, 1) << "Daylight flag should be 1 for "
                           << param.iana_tz_name;
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], param.expected_tzname_dst.c_str())
        << "Daylight TZ name mismatch for " << param.iana_tz_name;
  } else {
    // This is a timezone WITHOUT daylight savings. We do not check `daylight`
    // or `tzname[1]`, as their state can be inconsistent for zones that
    // historically had DST.
  }
}

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// We only disable some IANA tests in non-hermetic builds.
class DisabledIANATests : public ::testing::TestWithParam<TestData> {};

// A "test" case that verifies a disabled test is still failing.
// If it passes, this test will FAIL, indicating it should be re-enabled.
// Note: This is done this custom way because there is no standard method in
// gtest to disable tests for only certain parameters.
TEST_P(DisabledIANATests, VerifyExpectedFailure) {
  const auto& param = GetParam();
  ScopedTZ tz_manager(param.iana_tz_name.c_str());

  bool is_still_broken = false;
  std::string failure_details;

  if (timezone != param.expected_timezone) {
    is_still_broken = true;
    failure_details += "\n  - timezone: expected " +
                       std::to_string(param.expected_timezone) + ", got " +
                       std::to_string(timezone);
  }

  if (tzname[0] == nullptr ||
      strcmp(tzname[0], param.expected_tzname_std.c_str()) != 0) {
    is_still_broken = true;
    failure_details += "\n  - tzname[0]: expected \"" +
                       param.expected_tzname_std + "\", got \"" +
                       (tzname[0] ? tzname[0] : "nullptr") + "\"";
  }

  // Conditionally check DST fields if the test case expects them.
  if (!param.expected_tzname_dst.empty()) {
    if (daylight != 1) {
      is_still_broken = true;
      failure_details +=
          "\n  - daylight: expected 1, got " + std::to_string(daylight);
    }

    if (tzname[1] == nullptr ||
        strcmp(tzname[1], param.expected_tzname_dst.c_str()) != 0) {
      is_still_broken = true;
      failure_details += "\n  - tzname[1]: expected \"" +
                         param.expected_tzname_dst + "\", got \"" +
                         (tzname[1] ? tzname[1] : "nullptr") + "\"";
    }
  }

  if (!is_still_broken) {
    // The test passed, which is unexpected for a disabled test.
    // We use GTEST_SKIP to report this without failing the build.
    GTEST_SKIP()
        << "Test for " << param.iana_tz_name
        << " is marked as disabled but now passes. Please re-enable it.";
  }
}
#endif

// Generates a gtest-friendly name from the IANA timezone string
// by replacing non-alphanumeric characters with underscores,
// and explicitly naming "plus" and "minus" for GMT offsets.
// e.g., "America/Los_Angeles" -> "America_Los_Angeles"
//       "Etc/GMT+0" -> "Etc_GMT_plus_0"
//       "Etc/GMT-0" -> "Etc_GMT_minus_0"
auto test_name_generator = [](const auto& info) {
  std::string name = info.param.iana_tz_name;
  std::string result;

  for (char c : name) {
    if (c == '+') {
      result += "_plus_";
    } else if (c == '-') {
      result += "_minus_";
    } else if (std::isalnum(static_cast<unsigned char>(c))) {
      result += c;
    } else {
      result += '_';
    }
  }
  return result;
};

// This function holds the set of IANA names for tests to be disabled.
const std::set<std::string>& GetDisabledTestNames() {
// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  // These are zones where non-hermetic builds give incorrect results.
  static const std::set<std::string> disabled_tests = {
      "America/Argentina/Buenos_Aires",  // tzname[0] ("-03")
      "America/Asuncion",                // tzname[0] ("-03")
      "America/Bogota",                  // tzname[0] ("-05")
      "America/Caracas",                 // tzname[0] ("-04")
      "America/Cayenne",                 // tzname[0] ("-03")
      "America/Guyana",                  // tzname[0] ("-04")
      "America/La_Paz",                  // tzname[0] ("-04")
      "America/Lima",                    // tzname[0] ("-05")
      "America/Montevideo",              // tzname[0] ("-03")
      "America/Paramaribo",              // tzname[0] ("-03")
      "America/Santiago",                // tzname[0] ("-04"), tzname[1] ("-03")
      "America/Sao_Paulo",               // tzname[0] ("-03")
      "Asia/Dhaka",                      // tzname[0] ("+06").
      "Asia/Manila",                     // tzname[0] ("PST").
      "Asia/Istanbul",                   // tzname[0] ("+03").
      "Asia/Singapore",                  // tzname[0] ("+08").
      "Canada/East-Saskatchewan",        // tzname[0] ("Canada"), timezone (0)
      "Europe/Istanbul",                 // tzname[0] ("+03").
      "Turkey",                          // tzname[0] ("+03").
  };
#else   // !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  // On hermetic builds, all of the tested timezones have correct data.
  static const std::set<std::string> disabled_tests = {};
#endif  // !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  return disabled_tests;
}

// Helper function to filter a source list into enabled or disabled tests.
template <typename T>
std::vector<T> GetFilteredTimezoneTests(const std::vector<T>& source_tests,
                                        bool get_disabled) {
  const auto& disabled_names = GetDisabledTestNames();
  std::vector<T> filtered_tests;
  for (const auto& test : source_tests) {
    const bool is_disabled =
        (disabled_names.find(test.iana_tz_name) != disabled_names.end());
    if (is_disabled == get_disabled) {
      filtered_tests.push_back(test);
    }
  }
  return filtered_tests;
}

// Instantiates the test suite for all enabled IANA timezones.
INSTANTIATE_TEST_SUITE_P(
    AllTimezoneTests,
    IANATimezoneTests,
    ::testing::ValuesIn(GetFilteredTimezoneTests(IANATimezoneTests::GetTests(),
                                                 false)),
    test_name_generator);

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// We only disable some IANA tests in non-hermetic builds.
// Instantiates a test suite for all disabled tests. These will
// be reported as "SKIPPED" in the test output, providing visibility.
INSTANTIATE_TEST_SUITE_P(
    DisabledAllTimezoneTests,
    DisabledIANATests,
    ::testing::ValuesIn(GetFilteredTimezoneTests(IANATimezoneTests::GetTests(),
                                                 true)),
    test_name_generator);
#endif

// POSIX non-IANA Format Timezone Tests.

TEST(PosixTimezoneTests, HandlesUnsetTZEnvironmentVariable) {
  ScopedTZ tz_manager(nullptr);
  // We can't assert specific values as they depend on the system's default.
  // We can assert that tzname pointers are not null.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_GT(strlen(tzname[0]), 0U);
  ASSERT_NE(tzname[1], nullptr);
}

TEST(PosixTimezoneTests, HandlesEmptyTZStringAsUTC) {
  ScopedTZ tz_manager("");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "UTC");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "UTC");
  EXPECT_EQ(timezone, 0);
  EXPECT_EQ(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesLeadingColon) {
  ScopedTZ tz_manager(":UTC0");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "UTC");
  EXPECT_EQ(timezone, 0);
  EXPECT_EQ(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesISTMinus530) {
  ScopedTZ tz_manager("IST-5:30");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "IST");  // codespell:ignore ist
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "IST");  // codespell:ignore ist
  EXPECT_EQ(timezone, -(5 * kHour + 30 * 60L));
  EXPECT_EQ(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesFullHmsOffset) {
  ScopedTZ tz_manager("FOO+08:30:15");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "FOO");
  EXPECT_EQ(timezone, 8 * kHour + 30 * 60 + 15);
  EXPECT_EQ(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesCST6CDT) {
  ScopedTZ tz_manager("CST6CDT");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "CST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "CDT");
  EXPECT_EQ(timezone, 6 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesDefaultDstOffset) {
  ScopedTZ tz_manager("PST8PDT");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "PST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "PDT");
  EXPECT_EQ(timezone, 8 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesExplicitDSTOffset) {
  ScopedTZ tz_manager("AAA+5BBB+4");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "AAA");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "BBB");
  EXPECT_EQ(timezone, 5 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesQuotedNames) {
  ScopedTZ tz_manager("<UTC+3>-3<-UTC+2>-2");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "UTC+3");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "-UTC+2");
  EXPECT_EQ(timezone, -3 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesJulianDayDSTRule) {
  ScopedTZ tz_manager("CST6CDT,J60,J300");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "CST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "CDT");
  EXPECT_EQ(timezone, 6 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesZeroBasedJulianDayDSTRule) {
  ScopedTZ tz_manager("MST7MDT,59,299");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "MST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "MDT");
  EXPECT_EQ(timezone, 7 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesDSTRulesEST5EDT) {
  ScopedTZ tz_manager("EST5EDT,M3.2.0/2,M11.1.0/2");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "EST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "EDT");
  EXPECT_EQ(timezone, 5 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesTransitionRuleWithExplicitTime) {
  ScopedTZ tz_manager("CST6CDT,M3.2.0/02:30:00,M11.1.0/03:00:00");
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], "CST");
  ASSERT_NE(tzname[1], nullptr);
  EXPECT_STREQ(tzname[1], "CDT");
  EXPECT_EQ(timezone, 6 * kHour);
  EXPECT_NE(daylight, 0);
}

TEST(PosixTimezoneTests, MultipleScopedTZCallsEnsureCorrectUpdates) {
  {
    ScopedTZ tz_manager_pst("PST8PDT");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "PST");
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], "PDT");
    EXPECT_EQ(timezone, 8 * kHour);
    EXPECT_NE(daylight, 0);
  }
  {
    ScopedTZ tz_manager_est("EST5EDT");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "EST");
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], "EDT");
    EXPECT_EQ(timezone, 5 * kHour);
    EXPECT_NE(daylight, 0);
  }
  {
    ScopedTZ tz_manager_utc("UTC0");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "UTC");
    EXPECT_EQ(timezone, 0L);
    EXPECT_EQ(daylight, 0);
  }
}

TEST(PosixTimezoneTests, HandlesInvalidTZStringGracefully) {
  // POSIX does not specify behavior for invalid TZ strings.
  // A common fallback is UTC.
  ScopedTZ tz_manager("ThisIsClearlyInvalid!@#123");
  ASSERT_NE(tzname[0], nullptr);

// TODO: b/390675141 - Remove this after non-hermetic linux build is removed.
// On non-hermetic builds, this returns 'ThisIsClearlyInvalid' in tzname[0]
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  // Cobalt defaults to UTC for invalid and not recognized TZ values.
  EXPECT_STREQ(tzname[0], "UTC");
#endif
  EXPECT_EQ(timezone, 0L);
  EXPECT_EQ(daylight, 0);
}

// Helper for testing that various invalid TZ strings result in a fallback
// to a default state, which is assumed here to be UTC0.
void TestInvalidTzStringFallback(const char* tz_string) {
  ScopedTZ tz_manager(tz_string);
  ASSERT_NE(tzname[0], nullptr);
  // The behavior for invalid TZ strings is implementation-defined.
  // A robust implementation should fall back to a known state like UTC.
  EXPECT_STREQ(tzname[0], "UTC");
  EXPECT_EQ(timezone, 0);
  EXPECT_EQ(daylight, 0);
}

TEST(PosixTimezoneTests, HandlesEmptyStringAsFallback) {
  TestInvalidTzStringFallback("");
}

TEST(PosixTimezoneTests, HandlesGarbageStringAsFallback) {
  TestInvalidTzStringFallback("This is not a TZ string");
}

TEST(PosixTimezoneTests, HandlesInvalidStdOffsetAsFallback) {
  TestInvalidTzStringFallback("ABC+12:XX");
}

TEST(PosixTimezoneTests, HandlesInvalidDstOffsetAsFallback) {
  TestInvalidTzStringFallback("ABC+1DEF+2:XX");
}

TEST(PosixTimezoneTests, HandlesIncompleteRulesAsFallback) {
  TestInvalidTzStringFallback("CST6CDT,J60");
}

TEST(PosixTimezoneTests, HandlesRulesWithInvalidDateAsFallback) {
  TestInvalidTzStringFallback("CST6CDT,M3..0/2,M11.1.0/2");
}

TEST(PosixTimezoneTests, HandlesRulesWithInvalidTimeAsFallback) {
  TestInvalidTzStringFallback("CST6CDT,M3.2.0/2:30:xx,M11.1.0/2");
}
