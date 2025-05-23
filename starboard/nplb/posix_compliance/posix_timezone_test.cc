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

// POSIX defines tzname as "char *tzname[]", but both musl and glibc define it
// as "char *tzname[2]". This check allows either.
static_assert(std::is_same_v<decltype(tzname), char* [2]> ||
                  std::is_same_v<decltype(tzname), char*[]>,
              "Type of tzname must be 'char* [2]' or 'char* []'.");

static_assert(std::is_same_v<decltype(timezone), long>,
              "Type of timezone must be 'long'.");
static_assert(std::is_same_v<decltype(daylight), int>,
              "Type of daylight must be 'int'.");

// Number of seconds in an hour.
constexpr int kHour = 3600;
// Number of seconds in a minute.
constexpr int kMinute = 60;

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
      EXPECT_EQ(0, setenv("TZ", new_tz_value, 1))
          << "ScopedTZ: Failed to set TZ environment variable to \""
          << new_tz_value << "\"";
    } else {
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

  ScopedTZ(const ScopedTZ&) = delete;
  ScopedTZ& operator=(const ScopedTZ&) = delete;

 private:
  std::optional<std::string> original_tz_value_;
};

struct IANATestData {
  std::string iana_tz_name;
  long expected_timezone;
  std::string expected_tzname_std;
  std::string expected_tzname_dst;  // Empty if no DST.

  friend void PrintTo(const IANATestData& data, ::std::ostream* os) {
    *os << "{ iana_tz_name: \"" << data.iana_tz_name << "\""
        << ", expected_timezone: " << data.expected_timezone
        << ", expected_tzname_std: \"" << data.expected_tzname_std << "\"";
    if (!data.expected_tzname_dst.empty()) {
      *os << ", expected_tzname_dst: \"" << data.expected_tzname_dst << "\"";
    }
    *os << " }\n";
  }
};

class IANAFormat : public ::testing::TestWithParam<IANATestData> {
 public:
  // Note: This list of test cases is extensive but does not include all zones
  // known by ICU.
  static const std::vector<IANATestData>& GetTests() {
    static const std::vector<IANATestData> all_tests = {
        // Africa.
        //   https://data.iana.org/time-zones/tzdb/africa
        IANATestData{"Africa/Abidjan", 0, "GMT"},
        IANATestData{"Africa/Accra", 0, "GMT"},
        IANATestData{"Africa/Addis_Ababa", -3 * kHour, "EAT"},
        IANATestData{"Africa/Algiers", -1 * kHour, "CET"},
        IANATestData{"Africa/Bamako", 0, "GMT"},
        IANATestData{"Africa/Bangui", -1 * kHour, "WAT"},
        IANATestData{"Africa/Banjul", 0, "GMT"},
        IANATestData{"Africa/Bissau", 0, "GMT"},
        IANATestData{"Africa/Blantyre", -2 * kHour, "CAT"},
        IANATestData{"Africa/Brazzaville", -1 * kHour, "WAT"},
        IANATestData{"Africa/Bujumbura", -2 * kHour, "CAT"},
        IANATestData{"Africa/Cairo", -2 * kHour, "EET", "EEST"},
        IANATestData{"Africa/Conakry", 0, "GMT"},
        IANATestData{"Africa/Dakar", 0, "GMT"},
        IANATestData{"Africa/Dar_es_Salaam", -3 * kHour, "EAT"},
        IANATestData{"Africa/Djibouti", -3 * kHour, "EAT"},
        IANATestData{"Africa/Douala", -1 * kHour, "WAT"},
        IANATestData{"Africa/Freetown", 0, "GMT"},
        IANATestData{"Africa/Gaborone", -2 * kHour, "CAT"},
        IANATestData{"Africa/Harare", -2 * kHour, "CAT"},
        IANATestData{"Africa/Johannesburg", -2 * kHour, "SAST"},
        IANATestData{"Africa/Juba", -2 * kHour, "CAT"},
        IANATestData{"Africa/Kampala", -3 * kHour, "EAT"},
        IANATestData{"Africa/Khartoum", -2 * kHour, "CAT"},
        IANATestData{"Africa/Kigali", -2 * kHour, "CAT"},
        IANATestData{"Africa/Kinshasa", -1 * kHour, "WAT"},
        IANATestData{"Africa/Lagos", -1 * kHour, "WAT"},
        IANATestData{"Africa/Libreville", -1 * kHour, "WAT"},
        IANATestData{"Africa/Lome", 0, "GMT"},
        IANATestData{"Africa/Luanda", -1 * kHour, "WAT"},
        IANATestData{"Africa/Lubumbashi", -2 * kHour, "CAT"},
        IANATestData{"Africa/Lusaka", -2 * kHour, "CAT"},
        IANATestData{"Africa/Malabo", -1 * kHour, "WAT"},
        IANATestData{"Africa/Maputo", -2 * kHour, "CAT"},
        IANATestData{"Africa/Maseru", -2 * kHour, "SAST"},
        IANATestData{"Africa/Mbabane", -2 * kHour, "SAST"},
        IANATestData{"Africa/Mogadishu", -3 * kHour, "EAT"},
        IANATestData{"Africa/Monrovia", 0, "GMT"},
        IANATestData{"Africa/Nairobi", -3 * kHour, "EAT"},
        IANATestData{"Africa/Ndjamena", -1 * kHour, "WAT"},
        IANATestData{"Africa/Niamey", -1 * kHour, "WAT"},
        IANATestData{"Africa/Nouakchott", 0, "GMT"},
        IANATestData{"Africa/Ouagadougou", 0, "GMT"},
        IANATestData{"Africa/Porto-Novo", -1 * kHour, "WAT"},
        IANATestData{"Africa/Sao_Tome", 0, "GMT"},
        IANATestData{"Africa/Timbuktu", 0, "GMT"},
        IANATestData{"Africa/Tripoli", -2 * kHour, "EET"},
        IANATestData{"Africa/Tunis", -1 * kHour, "CET"},
        IANATestData{"Africa/Windhoek", -2 * kHour, "CAT"},

        // North America.
        //   https://data.iana.org/time-zones/tzdb/northamerica
        IANATestData{"America/Adak", 10 * kHour, "HST", "HDT"},
        IANATestData{"America/Anchorage", 9 * kHour, "AKST", "AKDT"},
        IANATestData{"America/Anguilla", 4 * kHour, "AST"},
        IANATestData{"America/Antigua", 4 * kHour, "AST"},
        IANATestData{"America/Aruba", 4 * kHour, "AST"},
        IANATestData{"America/Atikokan", 5 * kHour, "EST"},
        IANATestData{"America/Atka", 10 * kHour, "HST", "HDT"},
        IANATestData{"America/Bahia_Banderas", 6 * kHour, "CST"},
        IANATestData{"America/Barbados", 4 * kHour, "AST"},
        IANATestData{"America/Belize", 6 * kHour, "CST"},
        IANATestData{"America/Blanc-Sablon", 4 * kHour, "AST"},
        IANATestData{"America/Boise", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/Cambridge_Bay", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/Cancun", 5 * kHour, "EST"},
        IANATestData{"America/Cayman", 5 * kHour, "EST"},
        IANATestData{"America/Chicago", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Chihuahua", 6 * kHour, "CST"},
        IANATestData{"America/Ciudad_Juarez", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/Coral_Harbour", 5 * kHour, "EST"},
        IANATestData{"America/Costa_Rica", 6 * kHour, "CST"},
        IANATestData{"America/Creston", 7 * kHour, "MST"},
        IANATestData{"America/Curacao", 4 * kHour, "AST"},
        IANATestData{"America/Dawson_Creek", 7 * kHour, "MST"},
        IANATestData{"America/Dawson", 7 * kHour, "MST"},
        IANATestData{"America/Denver", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/Detroit", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Dominica", 4 * kHour, "AST"},
        IANATestData{"America/Edmonton", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/El_Salvador", 6 * kHour, "CST"},
        IANATestData{"America/Ensenada", 8 * kHour, "PST", "PDT"},
        IANATestData{"America/Fort_Nelson", 7 * kHour, "MST"},
        IANATestData{"America/Fort_Wayne", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Glace_Bay", 4 * kHour, "AST", "ADT"},
        IANATestData{"America/Goose_Bay", 4 * kHour, "AST", "ADT"},
        IANATestData{"America/Grand_Turk", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Grenada", 4 * kHour, "AST"},
        IANATestData{"America/Guadeloupe", 4 * kHour, "AST"},
        IANATestData{"America/Guatemala", 6 * kHour, "CST"},
        IANATestData{"America/Halifax", 4 * kHour, "AST", "ADT"},
        IANATestData{"America/Havana", 5 * kHour, "CST", "CDT"},
        IANATestData{"America/Hermosillo", 7 * kHour, "MST"},
        IANATestData{"America/Indiana/Indianapolis", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Indiana/Knox", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Indiana/Marengo", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Indiana/Petersburg", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Indiana/Tell_City", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Indiana/Vevay", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Indiana/Vincennes", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Indiana/Winamac", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Indianapolis", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Inuvik", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/Iqaluit", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Juneau", 9 * kHour, "AKST", "AKDT"},
        IANATestData{"America/Kentucky/Louisville", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Kentucky/Monticello", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Knox_IN", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Kralendijk", 4 * kHour, "AST"},
        IANATestData{"America/Los_Angeles", 8 * kHour, "PST", "PDT"},
        IANATestData{"America/Louisville", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Lower_Princes", 4 * kHour, "AST"},
        IANATestData{"America/Managua", 6 * kHour, "CST"},
        IANATestData{"America/Marigot", 4 * kHour, "AST"},
        IANATestData{"America/Martinique", 4 * kHour, "AST"},
        IANATestData{"America/Matamoros", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Mazatlan", 7 * kHour, "MST"},
        IANATestData{"America/Menominee", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Merida", 6 * kHour, "CST"},
        IANATestData{"America/Metlakatla", 9 * kHour, "AKST", "AKDT"},
        IANATestData{"America/Mexico_City", 6 * kHour, "CST"},
        IANATestData{"America/Moncton", 4 * kHour, "AST", "ADT"},
        IANATestData{"America/Monterrey", 6 * kHour, "CST"},
        IANATestData{"America/Montreal", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Montserrat", 4 * kHour, "AST"},
        IANATestData{"America/Nassau", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/New_York", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Nipigon", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Nome",  // codespell:ignore nome
                     9 * kHour, "AKST", "AKDT"},
        IANATestData{"America/North_Dakota/Beulah", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/North_Dakota/Center", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/North_Dakota/New_Salem", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Ojinaga", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Panama", 5 * kHour, "EST"},
        IANATestData{"America/Pangnirtung", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Phoenix", 7 * kHour, "MST"},
        IANATestData{"America/Port_of_Spain", 4 * kHour, "AST"},
        IANATestData{"America/Port-au-Prince", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Puerto_Rico", 4 * kHour, "AST"},
        IANATestData{"America/Rainy_River", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Rankin_Inlet", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Regina", 6 * kHour, "CST"},
        IANATestData{"America/Resolute", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Santa_Isabel", 8 * kHour, "PST", "PDT"},
        IANATestData{"America/Santo_Domingo", 4 * kHour, "AST"},
        IANATestData{"America/Shiprock", 7 * kHour, "MST", "MDT"},
        IANATestData{"America/Sitka", 9 * kHour, "AKST", "AKDT"},
        IANATestData{"America/St_Barthelemy", 4 * kHour, "AST"},
        IANATestData{"America/St_Johns", static_cast<long>(3.5 * kHour), "NST",
                     "NDT"},
        IANATestData{"America/St_Kitts", 4 * kHour, "AST"},
        IANATestData{"America/St_Lucia", 4 * kHour, "AST"},
        IANATestData{"America/St_Thomas", 4 * kHour, "AST"},
        IANATestData{"America/St_Vincent", 4 * kHour, "AST"},
        IANATestData{"America/Swift_Current", 6 * kHour, "CST"},
        IANATestData{"America/Tegucigalpa", 6 * kHour, "CST"},
        IANATestData{"America/Thunder_Bay", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Tijuana", 8 * kHour, "PST", "PDT"},
        IANATestData{"America/Toronto", 5 * kHour, "EST", "EDT"},
        IANATestData{"America/Tortola", 4 * kHour, "AST"},
        IANATestData{"America/Vancouver", 8 * kHour, "PST", "PDT"},
        IANATestData{"America/Virgin", 4 * kHour, "AST"},
        IANATestData{"America/Whitehorse", 7 * kHour, "MST"},
        IANATestData{"America/Winnipeg", 6 * kHour, "CST", "CDT"},
        IANATestData{"America/Yakutat", 9 * kHour, "AKST", "AKDT"},
        IANATestData{"America/Yellowknife", 7 * kHour, "MST", "MDT"},
        IANATestData{"Atlantic/Bermuda", 4 * kHour, "AST", "ADT"},
        IANATestData{"Canada/Saskatchewan", 6 * kHour, "CST"},
        IANATestData{"Canada/Yukon", 7 * kHour, "MST"},
        IANATestData{"Canada/Atlantic", 4 * kHour, "AST", "ADT"},
        IANATestData{"Canada/Central", 6 * kHour, "CST", "CDT"},
        IANATestData{"Canada/East-Saskatchewan", 6 * kHour, "CST"},
        IANATestData{"Canada/Eastern", 5 * kHour, "EST", "EDT"},
        IANATestData{"Canada/Mountain", 7 * kHour, "MST", "MDT"},
        IANATestData{"Canada/Newfoundland", static_cast<long>(3.5 * kHour),
                     "NST", "NDT"},
        IANATestData{"Canada/Pacific", 8 * kHour, "PST", "PDT"},
        IANATestData{"Pacific/Honolulu", 10 * kHour, "HST"},

        // South America.
        //   https://data.iana.org/time-zones/tzdb/southamerica
        IANATestData{"America/Argentina/Buenos_Aires", 3 * kHour, "ART"},
        IANATestData{"America/Asuncion", 3 * kHour, "PYT"},
        IANATestData{"America/Bogota", 5 * kHour, "COT"},
        IANATestData{"America/Caracas", 4 * kHour, "VET"},
        IANATestData{"America/Cayenne", 3 * kHour, "GFT"},
        IANATestData{"America/Guyana", 4 * kHour, "GYT"},
        IANATestData{"America/La_Paz", 4 * kHour, "BOT"},
        IANATestData{"America/Lima", 5 * kHour, "PET"},
        IANATestData{"America/Montevideo", 3 * kHour, "UYT"},
        IANATestData{"America/Paramaribo", 3 * kHour, "SRT"},
        IANATestData{"America/Santiago", 4 * kHour, "CLT", "CLST"},
        IANATestData{"America/Sao_Paulo", 3 * kHour, "BRT"},

        // Australasia.
        //   https://data.iana.org/time-zones/tzdb/australasia
        IANATestData{"Antarctica/Macquarie", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Antarctica/McMurdo", -12 * kHour, "NZST", "NZDT"},
        IANATestData{"Antarctica/South_Pole", -12 * kHour, "NZST", "NZDT"},
        IANATestData{"Australia/ACT", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Adelaide", -static_cast<long>(9.5 * kHour),
                     "ACST", "ACDT"},
        IANATestData{"Australia/Brisbane", -10 * kHour, "AEST"},
        IANATestData{"Australia/Broken_Hill", -static_cast<long>(9.5 * kHour),
                     "ACST", "ACDT"},
        IANATestData{"Australia/Canberra", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Currie", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Darwin", -static_cast<long>(9.5 * kHour),
                     "ACST"},
        IANATestData{"Australia/Hobart", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Lindeman", -10 * kHour, "AEST"},
        IANATestData{"Australia/Melbourne", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/North", -static_cast<long>(9.5 * kHour),
                     "ACST"},
        IANATestData{"Australia/NSW", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Perth", -8 * kHour, "AWST"},
        IANATestData{"Australia/Queensland", -10 * kHour, "AEST"},
        IANATestData{"Australia/South", -static_cast<long>(9.5 * kHour), "ACST",
                     "ACDT"},
        IANATestData{"Australia/Sydney", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Tasmania", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/Victoria", -10 * kHour, "AEST", "AEDT"},
        IANATestData{"Australia/West", -8 * kHour, "AWST", "AWDT"},
        IANATestData{"Australia/Yancowinna", -static_cast<long>(9.5 * kHour),
                     "ACST", "ACDT"},

        // Asia.
        //   https://data.iana.org/time-zones/tzdb/asia
        IANATestData{"Asia/Beirut", -2 * kHour, "EET", "EEST"},
        IANATestData{"Asia/Calcutta", -static_cast<long>(5.5 * kHour),
                     "IST"},  // codespell:ignore ist
        IANATestData{"Asia/Dhaka", -6 * kHour, "BST"},
        IANATestData{"Asia/Famagusta", -2 * kHour, "EET", "EEST"},
        IANATestData{"Asia/Gaza", -2 * kHour, "EET", "EEST"},
        IANATestData{"Asia/Harbin", -8 * kHour, "CST"},
        IANATestData{"Asia/Hebron", -2 * kHour, "EET", "EEST"},
        IANATestData{"Asia/Hong_Kong", -8 * kHour, "HKT"},
        IANATestData{"Asia/Istanbul", -3 * kHour, "TRT"},
        IANATestData{"Asia/Jerusalem", -2 * kHour,
                     "IST",  // codespell:ignore ist
                     "IDT"},
        IANATestData{"Asia/Jakarta", -7 * kHour, "WIB"},
        IANATestData{"Asia/Jayapura", -9 * kHour,
                     "WIT"},  // codespell:ignore wit
        IANATestData{"Asia/Karachi", -5 * kHour, "PKT"},
        IANATestData{"Asia/Kolkata", -static_cast<long>(5.5 * kHour),
                     "IST"},  // codespell:ignore ist
        IANATestData{"Asia/Macao", -8 * kHour, "CST"},
        IANATestData{"Asia/Macau", -8 * kHour, "CST"},
        IANATestData{"Asia/Makassar", -8 * kHour, "WITA"},
        IANATestData{"Asia/Manila", -8 * kHour, "PHST"},
        IANATestData{"Asia/Nicosia", -2 * kHour, "EET", "EEST"},
        IANATestData{"Asia/Pontianak", -7 * kHour, "WIB"},
        IANATestData{"Asia/Seoul", -9 * kHour, "KST"},
        IANATestData{"Asia/Singapore", -8 * kHour, "SGT"},
        IANATestData{"Asia/Taipei", -8 * kHour, "CST"},
        IANATestData{"Asia/Tel_Aviv", -2 * kHour,
                     "IST",  // codespell:ignore ist
                     "IDT"},
        IANATestData{"Asia/Tokyo", -9 * kHour, "JST"},
        IANATestData{"Asia/Ujung_Pandang", -8 * kHour, "WITA"},
        IANATestData{"Atlantic/Reykjavik", 0, "GMT"},
        IANATestData{"Atlantic/St_Helena", 0, "GMT"},

        // Europe.
        //   https://data.iana.org/time-zones/tzdb/europe
        IANATestData{"Africa/Ceuta", -1 * kHour, "CET", "CEST"},
        IANATestData{"America/Danmarkshavn", 0, "GMT"},
        IANATestData{"America/Thule", 4 * kHour, "AST", "ADT"},
        IANATestData{"Arctic/Longyearbyen", -1 * kHour, "CET", "CEST"},
        IANATestData{"Atlantic/Canary", 0, "WET", "WEST"},
        IANATestData{"Atlantic/Faeroe", 0, "WET", "WEST"},
        IANATestData{"Atlantic/Faroe", 0, "WET", "WEST"},
        IANATestData{"Atlantic/Madeira", 0, "WET", "WEST"},
        IANATestData{"Europe/Amsterdam", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Andorra", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Athens", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Belfast", 0, "GMT", "BST"},
        IANATestData{"Europe/Belgrade", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Berlin", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Bratislava", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Brussels", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Bucharest", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Budapest", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Busingen", -1 * kHour, "CET"},
        IANATestData{"Europe/Chisinau", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Copenhagen", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Dublin", 0, "GMT", "IST"},  // codespell:ignore ist
        IANATestData{"Europe/Gibraltar", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Guernsey", 0, "GMT", "BST"},
        IANATestData{"Europe/Helsinki", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Isle_of_Man", 0, "GMT", "BST"},
        IANATestData{"Europe/Jersey", 0, "GMT", "BST"},
        IANATestData{"Europe/Kiev", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Kyiv", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Lisbon", 0, "WET", "WEST"},
        IANATestData{"Europe/Ljubljana", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/London", 0, "GMT", "BST"},
        IANATestData{"Europe/Luxembourg", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Madrid", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Malta", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Mariehamn", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Monaco", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Nicosia", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Oslo", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Paris", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Podgorica", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Prague", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Riga", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Rome", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/San_Marino", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Sarajevo", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Skopje", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Sofia", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Stockholm", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Tallinn", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Tirane", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Tiraspol", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Uzhgorod", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Vaduz", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Vatican", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Vienna", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Vilnius", -2 * kHour, "EET", "EEST"},
        IANATestData{"Europe/Warsaw", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Zagreb", -1 * kHour, "CET", "CEST"},
        IANATestData{"Europe/Zaporozhye", -2 * kHour, "EET"},
        IANATestData{"Europe/Zurich", -1 * kHour, "CET", "CEST"},

        // Legacy Aliases.
        //   https://data.iana.org/time-zones/tzdb/backward
        //   https://data.iana.org/time-zones/tzdb/backzone
        IANATestData{"Atlantic/Jan_Mayen", -1 * kHour, "CET", "CEST"},
        IANATestData{"CET", -1 * kHour, "CET", "CEST"},
        IANATestData{"CST6CDT", 6 * kHour, "CST", "CDT"},
        IANATestData{"Cuba", 5 * kHour, "CST", "CDT"},
        IANATestData{"EET", -2 * kHour, "EET", "EEST"},
        IANATestData{"Egypt", -2 * kHour, "EET", "EEST"},
        IANATestData{"Eire", 0, "GMT", "IST"},  // codespell:ignore ist
        IANATestData{"EST", 5 * kHour, "EST"},
        IANATestData{"EST5EDT", 5 * kHour, "EST", "EDT"},
        IANATestData{"Etc/GMT-0", 0, "GMT"},
        IANATestData{"Etc/GMT+0", 0, "GMT"},
        IANATestData{"Etc/GMT0", 0, "GMT"},
        IANATestData{"Etc/Greenwich", 0, "GMT"},
        IANATestData{"Etc/UCT", 0, "UTC"},
        IANATestData{"Etc/Universal", 0, "UTC"},
        IANATestData{"Etc/Zulu", 0, "UTC"},
        IANATestData{"GB-Eire", 0, "GMT", "BST"},
        IANATestData{"GB", 0, "GMT", "BST"},
        IANATestData{"GMT-0", 0, "GMT"},
        IANATestData{"GMT+0", 0, "GMT"},
        IANATestData{"GMT0", 0, "GMT"},
        IANATestData{"Greenwich", 0, "GMT"},
        IANATestData{"Hongkong", -8 * kHour, "HKT", "HKST"},
        IANATestData{"Iceland", 0, "GMT"},
        IANATestData{"Indian/Antananarivo", -3 * kHour, "EAT"},
        IANATestData{"Indian/Comoro", -3 * kHour, "EAT"},
        IANATestData{"Indian/Mayotte", -3 * kHour, "EAT"},
        IANATestData{"Israel", -2 * kHour, "IST",  // codespell:ignore ist
                     "IDT"},
        IANATestData{"Jamaica", 5 * kHour, "EST", "EDT"},
        IANATestData{"Japan", -9 * kHour, "JST"},
        IANATestData{"Libya", -2 * kHour, "EET"},
        IANATestData{"Mexico/BajaNorte", 8 * kHour, "PST", "PDT"},
        IANATestData{"Mexico/BajaSur", 7 * kHour, "MST", "MDT"},
        IANATestData{"Mexico/General", 6 * kHour, "CST", "CDT"},
        IANATestData{"MST7MDT", 7 * kHour, "MST", "MDT"},
        IANATestData{"Navajo", 7 * kHour, "MST", "MDT"},
        IANATestData{"NZ", -12 * kHour, "NZST", "NZDT"},
        IANATestData{"Pacific/Auckland", -12 * kHour, "NZST", "NZDT"},
        IANATestData{"Pacific/Guam", -10 * kHour, "ChST"},
        IANATestData{"Pacific/Johnston", 10 * kHour, "HST"},
        IANATestData{"Pacific/Midway", 11 * kHour, "SST"},
        IANATestData{"Pacific/Pago_Pago", 11 * kHour, "SST"},
        IANATestData{"Pacific/Saipan", -10 * kHour, "ChST"},
        IANATestData{"Pacific/Samoa", 11 * kHour, "SST"},
        IANATestData{"Poland", -1 * kHour, "CET", "CEST"},
        IANATestData{"Portugal", 0, "WET", "WEST"},
        IANATestData{"PST8PDT", 8 * kHour, "PST", "PDT"},
        IANATestData{"ROC", -8 * kHour, "CST", "CDT"},
        IANATestData{"ROK", -9 * kHour, "KST", "KDT"},
        IANATestData{"Turkey", -3 * kHour, "TRT"},
        IANATestData{"UCT", 0, "UTC"},
        IANATestData{"Universal", 0, "UTC"},
        IANATestData{"US/Alaska", 9 * kHour, "AKST", "AKDT"},
        IANATestData{"US/Aleutian", 10 * kHour, "HST", "HDT"},
        IANATestData{"US/Arizona", 7 * kHour, "MST"},
        IANATestData{"US/Central", 6 * kHour, "CST", "CDT"},
        IANATestData{"US/East-Indiana", 5 * kHour, "EST"},
        IANATestData{"US/Eastern", 5 * kHour, "EST", "EDT"},
        IANATestData{"US/Hawaii", 10 * kHour, "HST"},
        IANATestData{"US/Indiana-Starke", 6 * kHour, "CST", "CDT"},
        IANATestData{"US/Michigan", 5 * kHour, "EST", "EDT"},
        IANATestData{"US/Mountain", 7 * kHour, "MST", "MDT"},
        IANATestData{"US/Pacific", 8 * kHour, "PST", "PDT"},
        IANATestData{"US/Samoa", 11 * kHour, "SST"},
        IANATestData{"UTC", 0, "UTC"},
        IANATestData{"WET", 0, "WET", "WEST"},
        IANATestData{"Zulu", 0, "UTC"},

        // Etcetera.
        //   https://data.iana.org/time-zones/tzdb/etcetera
        IANATestData{"Etc/UTC", 0, "UTC"},
        IANATestData{"Etc/GMT", 0, "GMT"},
        IANATestData{"GMT", 0, "GMT"},
    };
    return all_tests;
  }
};

// The unified test case for all enabled IANA timezones.
// It conditionally validates DST properties based on whether
// `expected_tzname_dst` is provided.
TEST_P(IANAFormat, Handles) {
  const auto& param = GetParam();
  ScopedTZ tz_manager(param.iana_tz_name.c_str());

  // Assert that the global variables match the expected values.
  EXPECT_EQ(timezone, param.expected_timezone)
      << "Timezone offset mismatch for " << param.iana_tz_name;

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.expected_tzname_std.c_str())
      << "Standard TZ name mismatch for " << param.iana_tz_name;

  ASSERT_NE(tzname[1], nullptr);
  if (!param.expected_tzname_dst.empty()) {
    // This is a timezone WITH daylight savings, so we check the DST name
    // and that the daylight flag is set.
    EXPECT_EQ(daylight, 1) << "Daylight flag should be 1 for "
                           << param.iana_tz_name;
    EXPECT_STREQ(tzname[1], param.expected_tzname_dst.c_str())
        << "Daylight TZ name mismatch for " << param.iana_tz_name;
  } else {
    // This is a timezone WITHOUT daylight savings. We do not check `daylight`
    // or `tzname[1]`, as their state can be inconsistent for zones that
    // historically had DST.
  }
}

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
      "Europe/Dublin",  // tzname[0] ("IST"),  // codespell:ignore ist
                        // tzname[1] ("GMT"), timezone (-3600)
      "Eire",           // tzname[0] ("IST"),  // codespell:ignore ist
                        // tzname[1] ("GMT"), timezone (-3600)
  };
#else   // !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  // On hermetic builds, all of the tested timezones have correct data.
  static const std::set<std::string> disabled_tests = {};
#endif  // !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  return disabled_tests;
}

// Helper function to filter a source list into enabled or disabled tests.
std::vector<IANATestData> GetFilteredTimezoneTests(
    const std::vector<IANATestData>& source_tests,
    bool get_disabled) {
  const auto& disabled_names = GetDisabledTestNames();
  std::vector<IANATestData> filtered_tests;
  for (const auto& test : source_tests) {
    const bool is_disabled =
        (disabled_names.find(test.iana_tz_name) != disabled_names.end());
    if (is_disabled == get_disabled) {
      filtered_tests.push_back(test);
    }
  }
  return filtered_tests;
}

// Instantiates the test suite for all enabled IANA timezones, named
// "PosixTimezoneTests/IANAFormat.Handles/Location_Name"
INSTANTIATE_TEST_SUITE_P(
    PosixTimezoneTests,
    IANAFormat,
    ::testing::ValuesIn(GetFilteredTimezoneTests(IANAFormat::GetTests(),
                                                 false)),
    [](const auto& info) {
      // Generates a gtest-friendly name from the IANA timezone string
      // by replacing non-alphanumeric characters with underscores,
      // and explicitly naming "plus" and "minus" for GMT offsets.
      // e.g., "America/Los_Angeles" -> "America_Los_Angeles"
      //       "Etc/GMT+0" -> "Etc_GMT_plus_0"
      //       "Etc/GMT-0" -> "Etc_GMT_minus_0"
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
    });

struct PosixTestData {
  std::string test_name;
  std::string tz_string;
  long expected_timezone;
  std::string expected_tzname_std;
  std::string expected_tzname_dst;  // Empty if no DST rule.

  friend void PrintTo(const PosixTestData& data, ::std::ostream* os) {
    *os << "{ test_name: \"" << data.test_name << "\", tz_string: \""
        << data.tz_string << "\", expected_timezone: " << data.expected_timezone
        << ", expected_tzname_std: \"" << data.expected_tzname_std << "\"";
    if (!data.expected_tzname_dst.empty()) {
      *os << ", expected_tzname_dst: \"" << data.expected_tzname_dst << "\"";
    } else {
      *os << ", expected_tzname_dst: (not checked)";
    }
    *os << " }\n";
  }
};

class PosixFormat : public ::testing::TestWithParam<PosixTestData> {
 public:
  static const inline std::vector<PosixTestData> kAllTests = {
      PosixTestData{"EmptyTZStringAsUTC", "", 0, "UTC"},
      PosixTestData{"LeadingColon", ":UTC0", 0, "UTC"},
      PosixTestData{"ISTMinus530", "IST-5:30", -(5 * kHour + 30 * kMinute),
                    "IST"},  // codespell:ignore ist
      PosixTestData{"FullHmsOffset", "FOO+08:30:15",
                    8 * kHour + 30 * kMinute + 15, "FOO"},
      PosixTestData{"CST6CDT", "CST6CDT", 6 * kHour, "CST", "CDT"},
      PosixTestData{"DefaultDstOffset", "PST8PDT", 8 * kHour, "PST", "PDT"},
      PosixTestData{"ExplicitDSTOffset", "AAA+5BBB+4", 5 * kHour, "AAA", "BBB"},
      PosixTestData{"QuotedNames", "<UTC+3>-3<-UTC+2>-2", -3 * kHour, "UTC+3",
                    "-UTC+2"},
      PosixTestData{"JulianDayDSTRule", "CST6CDT,J60,J300", 6 * kHour, "CST",
                    "CDT"},
      PosixTestData{"ZeroBasedJulianDayDSTRule", "MST7MDT,59,299", 7 * kHour,
                    "MST", "MDT"},
      PosixTestData{"DSTRulesEST5EDT", "EST5EDT,M3.2.0/2,M11.1.0/2", 5 * kHour,
                    "EST", "EDT"},
      PosixTestData{"TransitionRuleWithExplicitTime",
                    "CST6CDT,M3.2.0/02:30:00,M11.1.0/03:00:00", 6 * kHour,
                    "CST", "CDT"},
  };
};

TEST_P(PosixFormat, Handles) {
  const auto& param = GetParam();
  ScopedTZ tz_manager(param.tz_string.c_str());

  EXPECT_EQ(timezone, param.expected_timezone);

  bool expected_daylight = !param.expected_tzname_dst.empty();
  EXPECT_EQ(daylight, expected_daylight);

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.expected_tzname_std.c_str());

  if (!param.expected_tzname_dst.empty()) {
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], param.expected_tzname_dst.c_str());
  }
}

// Instantiate a set of tests named
// "PosixTimezoneTests/PosixFormat.Handles/TestName"
INSTANTIATE_TEST_SUITE_P(PosixTimezoneTests,
                         PosixFormat,
                         ::testing::ValuesIn(PosixFormat::kAllTests),
                         [](const auto& info) { return info.param.test_name; });

// Standalone tests for cases that don't fit the parameterized structure.
TEST(PosixTimezoneStandaloneTests, HandlesUnsetTZEnvironmentVariable) {
  ScopedTZ tz_manager(nullptr);
  // We can't assert specific values as they depend on the system's default.
  // We can assert that tzname pointers are not null.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_GT(strlen(tzname[0]), 0U);
  ASSERT_NE(tzname[1], nullptr);
}

TEST(PosixTimezoneStandaloneTests, MultipleScopedTZCallsEnsureCorrectUpdates) {
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

TEST(PosixTimezoneStandaloneTests, HandlesInvalidTZStringGracefully) {
  // POSIX does not specify behavior for invalid TZ strings.
  ScopedTZ tz_manager("ThisIsClearlyInvalid!@#123");
  // We only check that there is not a crash, that tzname is not nullptr, and
  // that timezone is set to 0.
  ASSERT_NE(tzname[0], nullptr);
  EXPECT_EQ(timezone, 0L);
}
