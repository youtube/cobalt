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
constexpr int kSecondsInHour = 3600;
// Number of seconds in a minute.
constexpr int kSecondsInMinute = 60;

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
        IANATestData{"Africa/Addis_Ababa", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Africa/Algiers", -1 * kSecondsInHour, "CET"},
        IANATestData{"Africa/Bamako", 0, "GMT"},
        IANATestData{"Africa/Bangui", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Banjul", 0, "GMT"},
        IANATestData{"Africa/Bissau", 0, "GMT"},
        IANATestData{"Africa/Blantyre", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Brazzaville", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Bujumbura", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Cairo", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Africa/Conakry", 0, "GMT"},
        IANATestData{"Africa/Dakar", 0, "GMT"},
        IANATestData{"Africa/Dar_es_Salaam", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Africa/Djibouti", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Africa/Douala", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Freetown", 0, "GMT"},
        IANATestData{"Africa/Gaborone", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Harare", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Johannesburg", -2 * kSecondsInHour, "SAST"},
        IANATestData{"Africa/Juba", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Kampala", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Africa/Khartoum", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Kigali", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Kinshasa", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Lagos", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Libreville", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Lome", 0, "GMT"},
        IANATestData{"Africa/Luanda", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Lubumbashi", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Lusaka", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Malabo", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Maputo", -2 * kSecondsInHour, "CAT"},
        IANATestData{"Africa/Maseru", -2 * kSecondsInHour, "SAST"},
        IANATestData{"Africa/Mbabane", -2 * kSecondsInHour, "SAST"},
        IANATestData{"Africa/Mogadishu", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Africa/Monrovia", 0, "GMT"},
        IANATestData{"Africa/Nairobi", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Africa/Ndjamena", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Niamey", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Nouakchott", 0, "GMT"},
        IANATestData{"Africa/Ouagadougou", 0, "GMT"},
        IANATestData{"Africa/Porto-Novo", -1 * kSecondsInHour, "WAT"},
        IANATestData{"Africa/Sao_Tome", 0, "GMT"},
        IANATestData{"Africa/Timbuktu", 0, "GMT"},
        IANATestData{"Africa/Tripoli", -2 * kSecondsInHour, "EET"},
        IANATestData{"Africa/Tunis", -1 * kSecondsInHour, "CET"},
        IANATestData{"Africa/Windhoek", -2 * kSecondsInHour, "CAT"},

        // North America.
        //   https://data.iana.org/time-zones/tzdb/northamerica
        IANATestData{"America/Adak", 10 * kSecondsInHour, "HST", "HDT"},
        IANATestData{"America/Anchorage", 9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"America/Anguilla", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Antigua", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Aruba", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Atikokan", 5 * kSecondsInHour, "EST"},
        IANATestData{"America/Atka", 10 * kSecondsInHour, "HST", "HDT"},
        IANATestData{"America/Bahia_Banderas", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Barbados", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Belize", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Blanc-Sablon", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Boise", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/Cambridge_Bay", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/Cancun", 5 * kSecondsInHour, "EST"},
        IANATestData{"America/Cayman", 5 * kSecondsInHour, "EST"},
        IANATestData{"America/Chicago", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Chihuahua", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Ciudad_Juarez", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/Coral_Harbour", 5 * kSecondsInHour, "EST"},
        IANATestData{"America/Costa_Rica", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Creston", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Curacao", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Dawson_Creek", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Dawson", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Denver", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/Detroit", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Dominica", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Edmonton", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/El_Salvador", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Ensenada", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"America/Fort_Nelson", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Fort_Wayne", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Glace_Bay", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"America/Goose_Bay", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"America/Grand_Turk", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Grenada", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Guadeloupe", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Guatemala", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Halifax", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"America/Havana", 5 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Hermosillo", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Indiana/Indianapolis", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Indiana/Knox", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Indiana/Marengo", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Indiana/Petersburg", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Indiana/Tell_City", 6 * kSecondsInHour, "CST",
                     "CDT"},
        IANATestData{"America/Indiana/Vevay", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Indiana/Vincennes", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Indiana/Winamac", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Indianapolis", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Inuvik", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/Iqaluit", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Juneau", 9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"America/Kentucky/Louisville", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Kentucky/Monticello", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Knox_IN", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Kralendijk", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Los_Angeles", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"America/Louisville", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Lower_Princes", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Managua", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Marigot", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Martinique", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Matamoros", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Mazatlan", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Menominee", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Merida", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Metlakatla", 9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"America/Mexico_City", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Moncton", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"America/Monterrey", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Montreal", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Montserrat", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Nassau", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/New_York", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Nipigon", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Nome",  // codespell:ignore nome
                     9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"America/North_Dakota/Beulah", 6 * kSecondsInHour, "CST",
                     "CDT"},
        IANATestData{"America/North_Dakota/Center", 6 * kSecondsInHour, "CST",
                     "CDT"},
        IANATestData{"America/North_Dakota/New_Salem", 6 * kSecondsInHour,
                     "CST", "CDT"},
        IANATestData{"America/Ojinaga", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Panama", 5 * kSecondsInHour, "EST"},
        IANATestData{"America/Pangnirtung", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Phoenix", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Port_of_Spain", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Port-au-Prince", 5 * kSecondsInHour, "EST",
                     "EDT"},
        IANATestData{"America/Puerto_Rico", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Rainy_River", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Rankin_Inlet", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Regina", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Resolute", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Santa_Isabel", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"America/Santo_Domingo", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Shiprock", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"America/Sitka", 9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"America/St_Barthelemy", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/St_Johns",
                     static_cast<long>(3.5 * kSecondsInHour), "NST", "NDT"},
        IANATestData{"America/St_Kitts", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/St_Lucia", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/St_Thomas", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/St_Vincent", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Swift_Current", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Tegucigalpa", 6 * kSecondsInHour, "CST"},
        IANATestData{"America/Thunder_Bay", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Tijuana", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"America/Toronto", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"America/Tortola", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Vancouver", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"America/Virgin", 4 * kSecondsInHour, "AST"},
        IANATestData{"America/Whitehorse", 7 * kSecondsInHour, "MST"},
        IANATestData{"America/Winnipeg", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"America/Yakutat", 9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"America/Yellowknife", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"Atlantic/Bermuda", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"Canada/Saskatchewan", 6 * kSecondsInHour, "CST"},
        IANATestData{"Canada/Yukon", 7 * kSecondsInHour, "MST"},
        IANATestData{"Canada/Atlantic", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"Canada/Central", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"Canada/East-Saskatchewan", 6 * kSecondsInHour, "CST"},
        IANATestData{"Canada/Eastern", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"Canada/Mountain", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"Canada/Newfoundland",
                     static_cast<long>(3.5 * kSecondsInHour), "NST", "NDT"},
        IANATestData{"Canada/Pacific", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"Pacific/Honolulu", 10 * kSecondsInHour, "HST"},

        // South America.
        //   https://data.iana.org/time-zones/tzdb/southamerica
        IANATestData{"America/Argentina/Buenos_Aires", 3 * kSecondsInHour,
                     "ART"},
        IANATestData{"America/Asuncion", 3 * kSecondsInHour, "PYT"},
        IANATestData{"America/Bogota", 5 * kSecondsInHour, "COT"},
        IANATestData{"America/Caracas", 4 * kSecondsInHour, "VET"},
        IANATestData{"America/Cayenne", 3 * kSecondsInHour, "GFT"},
        IANATestData{"America/Guyana", 4 * kSecondsInHour, "GYT"},
        IANATestData{"America/La_Paz", 4 * kSecondsInHour, "BOT"},
        IANATestData{"America/Lima", 5 * kSecondsInHour, "PET"},
        IANATestData{"America/Montevideo", 3 * kSecondsInHour, "UYT"},
        IANATestData{"America/Paramaribo", 3 * kSecondsInHour, "SRT"},
        IANATestData{"America/Santiago", 4 * kSecondsInHour, "CLT", "CLST"},
        IANATestData{"America/Sao_Paulo", 3 * kSecondsInHour, "BRT"},

        // Australasia.
        //   https://data.iana.org/time-zones/tzdb/australasia
        IANATestData{"Antarctica/Macquarie", -10 * kSecondsInHour, "AEST",
                     "AEDT"},
        IANATestData{"Antarctica/McMurdo", -12 * kSecondsInHour, "NZST",
                     "NZDT"},
        IANATestData{"Antarctica/South_Pole", -12 * kSecondsInHour, "NZST",
                     "NZDT"},
        IANATestData{"Australia/ACT", -10 * kSecondsInHour, "AEST", "AEDT"},
        IANATestData{"Australia/Adelaide",
                     -static_cast<long>(9.5 * kSecondsInHour), "ACST", "ACDT"},
        IANATestData{"Australia/Brisbane", -10 * kSecondsInHour, "AEST"},
        IANATestData{"Australia/Broken_Hill",
                     -static_cast<long>(9.5 * kSecondsInHour), "ACST", "ACDT"},
        IANATestData{"Australia/Canberra", -10 * kSecondsInHour, "AEST",
                     "AEDT"},
        IANATestData{"Australia/Currie", -10 * kSecondsInHour, "AEST", "AEDT"},
        IANATestData{"Australia/Darwin",
                     -static_cast<long>(9.5 * kSecondsInHour), "ACST"},
        IANATestData{"Australia/Hobart", -10 * kSecondsInHour, "AEST", "AEDT"},
        IANATestData{"Australia/Lindeman", -10 * kSecondsInHour, "AEST"},
        IANATestData{"Australia/Melbourne", -10 * kSecondsInHour, "AEST",
                     "AEDT"},
        IANATestData{"Australia/North",
                     -static_cast<long>(9.5 * kSecondsInHour), "ACST"},
        IANATestData{"Australia/NSW", -10 * kSecondsInHour, "AEST", "AEDT"},
        IANATestData{"Australia/Perth", -8 * kSecondsInHour, "AWST"},
        IANATestData{"Australia/Queensland", -10 * kSecondsInHour, "AEST"},
        IANATestData{"Australia/South",
                     -static_cast<long>(9.5 * kSecondsInHour), "ACST", "ACDT"},
        IANATestData{"Australia/Sydney", -10 * kSecondsInHour, "AEST", "AEDT"},
        IANATestData{"Australia/Tasmania", -10 * kSecondsInHour, "AEST",
                     "AEDT"},
        IANATestData{"Australia/Victoria", -10 * kSecondsInHour, "AEST",
                     "AEDT"},
        IANATestData{"Australia/West", -8 * kSecondsInHour, "AWST", "AWDT"},
        IANATestData{"Australia/Yancowinna",
                     -static_cast<long>(9.5 * kSecondsInHour), "ACST", "ACDT"},

        // Asia.
        //   https://data.iana.org/time-zones/tzdb/asia
        IANATestData{"Asia/Beirut", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Asia/Calcutta", -static_cast<long>(5.5 * kSecondsInHour),
                     "IST"},  // codespell:ignore ist
        IANATestData{"Asia/Dhaka", -6 * kSecondsInHour, "BST"},
        IANATestData{"Asia/Famagusta", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Asia/Gaza", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Asia/Harbin", -8 * kSecondsInHour, "CST"},
        IANATestData{"Asia/Hebron", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Asia/Hong_Kong", -8 * kSecondsInHour, "HKT"},
        IANATestData{"Asia/Istanbul", -3 * kSecondsInHour, "TRT"},
        IANATestData{"Asia/Jerusalem", -2 * kSecondsInHour,
                     "IST",  // codespell:ignore ist
                     "IDT"},
        IANATestData{"Asia/Jakarta", -7 * kSecondsInHour, "WIB"},
        IANATestData{"Asia/Jayapura", -9 * kSecondsInHour,
                     "WIT"},  // codespell:ignore wit
        IANATestData{"Asia/Karachi", -5 * kSecondsInHour, "PKT"},
        IANATestData{"Asia/Kolkata", -static_cast<long>(5.5 * kSecondsInHour),
                     "IST"},  // codespell:ignore ist
        IANATestData{"Asia/Macao", -8 * kSecondsInHour, "CST"},
        IANATestData{"Asia/Macau", -8 * kSecondsInHour, "CST"},
        IANATestData{"Asia/Makassar", -8 * kSecondsInHour, "WITA"},
        IANATestData{"Asia/Manila", -8 * kSecondsInHour, "PHST"},
        IANATestData{"Asia/Nicosia", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Asia/Pontianak", -7 * kSecondsInHour, "WIB"},
        IANATestData{"Asia/Seoul", -9 * kSecondsInHour, "KST"},
        IANATestData{"Asia/Singapore", -8 * kSecondsInHour, "SGT"},
        IANATestData{"Asia/Taipei", -8 * kSecondsInHour, "CST"},
        IANATestData{"Asia/Tel_Aviv", -2 * kSecondsInHour,
                     "IST",  // codespell:ignore ist
                     "IDT"},
        IANATestData{"Asia/Tokyo", -9 * kSecondsInHour, "JST"},
        IANATestData{"Asia/Ujung_Pandang", -8 * kSecondsInHour, "WITA"},
        IANATestData{"Atlantic/Reykjavik", 0, "GMT"},
        IANATestData{"Atlantic/St_Helena", 0, "GMT"},

        // Europe.
        //   https://data.iana.org/time-zones/tzdb/europe
        IANATestData{"Africa/Ceuta", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"America/Danmarkshavn", 0, "GMT"},
        IANATestData{"America/Thule", 4 * kSecondsInHour, "AST", "ADT"},
        IANATestData{"Arctic/Longyearbyen", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Atlantic/Canary", 0, "WET", "WEST"},
        IANATestData{"Atlantic/Faeroe", 0, "WET", "WEST"},
        IANATestData{"Atlantic/Faroe", 0, "WET", "WEST"},
        IANATestData{"Atlantic/Madeira", 0, "WET", "WEST"},
        IANATestData{"Europe/Amsterdam", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Andorra", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Athens", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Belfast", 0, "GMT", "BST"},
        IANATestData{"Europe/Belgrade", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Berlin", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Bratislava", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Brussels", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Bucharest", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Budapest", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Busingen", -1 * kSecondsInHour, "CET"},
        IANATestData{"Europe/Chisinau", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Copenhagen", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Dublin", 0, "GMT", "IST"},  // codespell:ignore ist
        IANATestData{"Europe/Gibraltar", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Guernsey", 0, "GMT", "BST"},
        IANATestData{"Europe/Helsinki", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Isle_of_Man", 0, "GMT", "BST"},
        IANATestData{"Europe/Jersey", 0, "GMT", "BST"},
        IANATestData{"Europe/Kiev", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Kyiv", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Lisbon", 0, "WET", "WEST"},
        IANATestData{"Europe/Ljubljana", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/London", 0, "GMT", "BST"},
        IANATestData{"Europe/Luxembourg", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Madrid", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Malta", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Mariehamn", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Monaco", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Nicosia", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Oslo", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Paris", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Podgorica", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Prague", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Riga", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Rome", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/San_Marino", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Sarajevo", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Skopje", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Sofia", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Stockholm", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Tallinn", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Tirane", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Tiraspol", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Uzhgorod", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Vaduz", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Vatican", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Vienna", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Vilnius", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Europe/Warsaw", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Zagreb", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Europe/Zaporozhye", -2 * kSecondsInHour, "EET"},
        IANATestData{"Europe/Zurich", -1 * kSecondsInHour, "CET", "CEST"},

        // Legacy Aliases.
        //   https://data.iana.org/time-zones/tzdb/backward
        //   https://data.iana.org/time-zones/tzdb/backzone
        IANATestData{"Atlantic/Jan_Mayen", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"CET", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"CST6CDT", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"Cuba", 5 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"EET", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Egypt", -2 * kSecondsInHour, "EET", "EEST"},
        IANATestData{"Eire", 0, "GMT", "IST"},  // codespell:ignore ist
        IANATestData{"EST", 5 * kSecondsInHour, "EST"},
        IANATestData{"EST5EDT", 5 * kSecondsInHour, "EST", "EDT"},
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
        IANATestData{"Hongkong", -8 * kSecondsInHour, "HKT", "HKST"},
        IANATestData{"Iceland", 0, "GMT"},
        IANATestData{"Indian/Antananarivo", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Indian/Comoro", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Indian/Mayotte", -3 * kSecondsInHour, "EAT"},
        IANATestData{"Israel", -2 * kSecondsInHour,
                     "IST",  // codespell:ignore ist
                     "IDT"},
        IANATestData{"Jamaica", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"Japan", -9 * kSecondsInHour, "JST"},
        IANATestData{"Libya", -2 * kSecondsInHour, "EET"},
        IANATestData{"Mexico/BajaNorte", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"Mexico/BajaSur", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"Mexico/General", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"MST7MDT", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"Navajo", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"NZ", -12 * kSecondsInHour, "NZST", "NZDT"},
        IANATestData{"Pacific/Auckland", -12 * kSecondsInHour, "NZST", "NZDT"},
        IANATestData{"Pacific/Guam", -10 * kSecondsInHour, "ChST"},
        IANATestData{"Pacific/Johnston", 10 * kSecondsInHour, "HST"},
        IANATestData{"Pacific/Midway", 11 * kSecondsInHour, "SST"},
        IANATestData{"Pacific/Pago_Pago", 11 * kSecondsInHour, "SST"},
        IANATestData{"Pacific/Saipan", -10 * kSecondsInHour, "ChST"},
        IANATestData{"Pacific/Samoa", 11 * kSecondsInHour, "SST"},
        IANATestData{"Poland", -1 * kSecondsInHour, "CET", "CEST"},
        IANATestData{"Portugal", 0, "WET", "WEST"},
        IANATestData{"PST8PDT", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"ROC", -8 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"ROK", -9 * kSecondsInHour, "KST", "KDT"},
        IANATestData{"Turkey", -3 * kSecondsInHour, "TRT"},
        IANATestData{"UCT", 0, "UTC"},
        IANATestData{"Universal", 0, "UTC"},
        IANATestData{"US/Alaska", 9 * kSecondsInHour, "AKST", "AKDT"},
        IANATestData{"US/Aleutian", 10 * kSecondsInHour, "HST", "HDT"},
        IANATestData{"US/Arizona", 7 * kSecondsInHour, "MST"},
        IANATestData{"US/Central", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"US/East-Indiana", 5 * kSecondsInHour, "EST"},
        IANATestData{"US/Eastern", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"US/Hawaii", 10 * kSecondsInHour, "HST"},
        IANATestData{"US/Indiana-Starke", 6 * kSecondsInHour, "CST", "CDT"},
        IANATestData{"US/Michigan", 5 * kSecondsInHour, "EST", "EDT"},
        IANATestData{"US/Mountain", 7 * kSecondsInHour, "MST", "MDT"},
        IANATestData{"US/Pacific", 8 * kSecondsInHour, "PST", "PDT"},
        IANATestData{"US/Samoa", 11 * kSecondsInHour, "SST"},
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
      PosixTestData{"ISTMinus530", "IST-5:30",
                    -(5 * kSecondsInHour + 30 * kSecondsInMinute),
                    "IST"},  // codespell:ignore ist
      PosixTestData{"FullHmsOffset", "FOO+08:30:15",
                    8 * kSecondsInHour + 30 * kSecondsInMinute + 15, "FOO"},
      PosixTestData{"CST6CDT", "CST6CDT", 6 * kSecondsInHour, "CST", "CDT"},
      PosixTestData{"DefaultDstOffset", "PST8PDT", 8 * kSecondsInHour, "PST",
                    "PDT"},
      PosixTestData{"ExplicitDSTOffset", "AAA+5BBB+4", 5 * kSecondsInHour,
                    "AAA", "BBB"},
      PosixTestData{"QuotedNames", "<UTC+3>-3<-UTC+2>-2", -3 * kSecondsInHour,
                    "UTC+3", "-UTC+2"},
      PosixTestData{"JulianDayDSTRule", "CST6CDT,J60,J300", 6 * kSecondsInHour,
                    "CST", "CDT"},
      PosixTestData{"ZeroBasedJulianDayDSTRule", "MST7MDT,59,299",
                    7 * kSecondsInHour, "MST", "MDT"},
      PosixTestData{"DSTRulesEST5EDT", "EST5EDT,M3.2.0/2,M11.1.0/2",
                    5 * kSecondsInHour, "EST", "EDT"},
      PosixTestData{"TransitionRuleWithExplicitTime",
                    "CST6CDT,M3.2.0/02:30:00,M11.1.0/03:00:00",
                    6 * kSecondsInHour, "CST", "CDT"},
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
    EXPECT_EQ(timezone, 8 * kSecondsInHour);
    EXPECT_NE(daylight, 0);
  }
  {
    ScopedTZ tz_manager_est("EST5EDT");
    ASSERT_NE(tzname[0], nullptr);
    EXPECT_STREQ(tzname[0], "EST");
    ASSERT_NE(tzname[1], nullptr);
    EXPECT_STREQ(tzname[1], "EDT");
    EXPECT_EQ(timezone, 5 * kSecondsInHour);
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
