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
#include <string.h>
#include <time.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "starboard/nplb/posix_compliance/posix_timezone_test_helpers.h"
#include "starboard/nplb/posix_compliance/scoped_tz_set.h"

namespace nplb {
namespace {

struct IANATestData {
  const char* tz;
  long offset;
  const char* std;
  std::optional<const char*> dst = std::nullopt;

  friend void PrintTo(const IANATestData& data, ::std::ostream* os) {
    *os << "{ tz: \"" << data.tz << "\""
        << ", offset: " << data.offset << ", std: \"" << data.std << "\"";
    if (data.dst.has_value()) {
      *os << ", dst: \"" << data.dst.value() << "\"";
    }
    *os << " }\n";
  }
};

std::string GetTestName(const ::testing::TestParamInfo<IANATestData>& info) {
  std::string name = info.param.tz;
  // Replace characters that are not valid in test names.
  size_t pos = 0;
  while ((pos = name.find('/', pos)) != std::string::npos) {
    name.replace(pos, 1, "_");
    pos += 1;
  }
  pos = 0;
  while ((pos = name.find('+', pos)) != std::string::npos) {
    name.replace(pos, 1, "plus");
    pos += 4;
  }
  pos = 0;
  while ((pos = name.find('-', pos)) != std::string::npos) {
    name.replace(pos, 1, "minus");
    pos += 5;
  }
  return name;
}

}  // namespace

class IANAFormat : public ::testing::TestWithParam<IANATestData> {
 public:
  // Note: This list of test cases is extensive but does not include all zones
  // known by ICU.
  static const std::array<IANATestData, 367> kAllTests;
};

const std::array<IANATestData, 367> IANAFormat::kAllTests = {
    {{"Africa/Abidjan", 0, "GMT"},
     {"Africa/Accra", 0, "GMT"},
     {"Africa/Addis_Ababa", -3 * kSecondsInHour, "EAT"},
     {"Africa/Algiers", -1 * kSecondsInHour, "CET"},
     {"Africa/Bamako", 0, "GMT"},
     {"Africa/Bangui", -1 * kSecondsInHour, "WAT"},
     {"Africa/Banjul", 0, "GMT"},
     {"Africa/Bissau", 0, "GMT"},
     {"Africa/Blantyre", -2 * kSecondsInHour, "CAT"},
     {"Africa/Brazzaville", -1 * kSecondsInHour, "WAT"},
     {"Africa/Bujumbura", -2 * kSecondsInHour, "CAT"},
     {"Africa/Cairo", -2 * kSecondsInHour, "EET", "EEST"},
     {"Africa/Conakry", 0, "GMT"},
     {"Africa/Dakar", 0, "GMT"},
     {"Africa/Dar_es_Salaam", -3 * kSecondsInHour, "EAT"},
     {"Africa/Djibouti", -3 * kSecondsInHour, "EAT"},
     {"Africa/Douala", -1 * kSecondsInHour, "WAT"},
     {"Africa/Freetown", 0, "GMT"},
     {"Africa/Gaborone", -2 * kSecondsInHour, "CAT"},
     {"Africa/Harare", -2 * kSecondsInHour, "CAT"},
     {"Africa/Johannesburg", -2 * kSecondsInHour, "SAST"},
     {"Africa/Juba", -2 * kSecondsInHour, "CAT"},
     {"Africa/Kampala", -3 * kSecondsInHour, "EAT"},
     {"Africa/Khartoum", -2 * kSecondsInHour, "CAT"},
     {"Africa/Kigali", -2 * kSecondsInHour, "CAT"},
     {"Africa/Kinshasa", -1 * kSecondsInHour, "WAT"},
     {"Africa/Lagos", -1 * kSecondsInHour, "WAT"},
     {"Africa/Libreville", -1 * kSecondsInHour, "WAT"},
     {"Africa/Lome", 0, "GMT"},
     {"Africa/Luanda", -1 * kSecondsInHour, "WAT"},
     {"Africa/Lubumbashi", -2 * kSecondsInHour, "CAT"},
     {"Africa/Lusaka", -2 * kSecondsInHour, "CAT"},
     {"Africa/Malabo", -1 * kSecondsInHour, "WAT"},
     {"Africa/Maputo", -2 * kSecondsInHour, "CAT"},
     {"Africa/Maseru", -2 * kSecondsInHour, "SAST"},
     {"Africa/Mbabane", -2 * kSecondsInHour, "SAST"},
     {"Africa/Mogadishu", -3 * kSecondsInHour, "EAT"},
     {"Africa/Monrovia", 0, "GMT"},
     {"Africa/Nairobi", -3 * kSecondsInHour, "EAT"},
     {"Africa/Ndjamena", -1 * kSecondsInHour, "WAT"},
     {"Africa/Niamey", -1 * kSecondsInHour, "WAT"},
     {"Africa/Nouakchott", 0, "GMT"},
     {"Africa/Ouagadougou", 0, "GMT"},
     {"Africa/Porto-Novo", -1 * kSecondsInHour, "WAT"},
     {"Africa/Sao_Tome", 0, "GMT"},
     {"Africa/Timbuktu", 0, "GMT"},
     {"Africa/Tripoli", -2 * kSecondsInHour, "EET"},
     {"Africa/Tunis", -1 * kSecondsInHour, "CET"},
     {"Africa/Windhoek", -2 * kSecondsInHour, "CAT"},

     // North America.
     //   https://data.iana.org/time-zones/tzdb/northamerica
     {"America/Adak", 10 * kSecondsInHour, "HST", "HDT"},
     {"America/Anchorage", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"America/Anguilla", 4 * kSecondsInHour, "AST"},
     {"America/Antigua", 4 * kSecondsInHour, "AST"},
     {"America/Aruba", 4 * kSecondsInHour, "AST"},
     {"America/Atikokan", 5 * kSecondsInHour, "EST"},
     {"America/Atka", 10 * kSecondsInHour, "HST", "HDT"},
     {"America/Bahia_Banderas", 6 * kSecondsInHour, "CST"},
     {"America/Barbados", 4 * kSecondsInHour, "AST"},
     {"America/Belize", 6 * kSecondsInHour, "CST"},
     {"America/Blanc-Sablon", 4 * kSecondsInHour, "AST"},
     {"America/Boise", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/Cambridge_Bay", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/Cancun", 5 * kSecondsInHour, "EST"},
     {"America/Cayman", 5 * kSecondsInHour, "EST"},
     {"America/Chicago", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Chihuahua", 6 * kSecondsInHour, "CST"},
     {"America/Ciudad_Juarez", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/Coral_Harbour", 5 * kSecondsInHour, "EST"},
     {"America/Costa_Rica", 6 * kSecondsInHour, "CST"},
     {"America/Creston", 7 * kSecondsInHour, "MST"},
     {"America/Curacao", 4 * kSecondsInHour, "AST"},
     {"America/Dawson_Creek", 7 * kSecondsInHour, "MST"},
     {"America/Dawson", 7 * kSecondsInHour, "MST"},
     {"America/Denver", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/Detroit", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Dominica", 4 * kSecondsInHour, "AST"},
     {"America/Edmonton", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/El_Salvador", 6 * kSecondsInHour, "CST"},
     {"America/Ensenada", 8 * kSecondsInHour, "PST", "PDT"},
     {"America/Fort_Nelson", 7 * kSecondsInHour, "MST"},
     {"America/Fort_Wayne", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Glace_Bay", 4 * kSecondsInHour, "AST", "ADT"},
     {"America/Goose_Bay", 4 * kSecondsInHour, "AST", "ADT"},
     {"America/Grand_Turk", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Grenada", 4 * kSecondsInHour, "AST"},
     {"America/Guadeloupe", 4 * kSecondsInHour, "AST"},
     {"America/Guatemala", 6 * kSecondsInHour, "CST"},
     {"America/Halifax", 4 * kSecondsInHour, "AST", "ADT"},
     {"America/Havana", 5 * kSecondsInHour, "CST", "CDT"},
     {"America/Hermosillo", 7 * kSecondsInHour, "MST"},
     {"America/Indiana/Indianapolis", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Indiana/Knox", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Indiana/Marengo", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Indiana/Petersburg", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Indiana/Tell_City", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Indiana/Vevay", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Indiana/Vincennes", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Indiana/Winamac", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Indianapolis", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Inuvik", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/Iqaluit", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Juneau", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"America/Kentucky/Louisville", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Kentucky/Monticello", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Knox_IN", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Kralendijk", 4 * kSecondsInHour, "AST"},
     {"America/Los_Angeles", 8 * kSecondsInHour, "PST", "PDT"},
     {"America/Louisville", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Lower_Princes", 4 * kSecondsInHour, "AST"},
     {"America/Managua", 6 * kSecondsInHour, "CST"},
     {"America/Marigot", 4 * kSecondsInHour, "AST"},
     {"America/Martinique", 4 * kSecondsInHour, "AST"},
     {"America/Matamoros", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Mazatlan", 7 * kSecondsInHour, "MST"},
     {"America/Menominee", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Merida", 6 * kSecondsInHour, "CST"},
     {"America/Metlakatla", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"America/Mexico_City", 6 * kSecondsInHour, "CST"},
     {"America/Moncton", 4 * kSecondsInHour, "AST", "ADT"},
     {"America/Monterrey", 6 * kSecondsInHour, "CST"},
     {"America/Montreal", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Montserrat", 4 * kSecondsInHour, "AST"},
     {"America/Nassau", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/New_York", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Nipigon", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Nome", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"America/North_Dakota/Beulah", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/North_Dakota/Center", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/North_Dakota/New_Salem", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Ojinaga", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Panama", 5 * kSecondsInHour, "EST"},
     {"America/Pangnirtung", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Phoenix", 7 * kSecondsInHour, "MST"},
     {"America/Port_of_Spain", 4 * kSecondsInHour, "AST"},
     {"America/Port-au-Prince", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Puerto_Rico", 4 * kSecondsInHour, "AST"},
     {"America/Rainy_River", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Rankin_Inlet", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Regina", 6 * kSecondsInHour, "CST"},
     {"America/Resolute", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Santa_Isabel", 8 * kSecondsInHour, "PST", "PDT"},
     {"America/Santo_Domingo", 4 * kSecondsInHour, "AST"},
     {"America/Shiprock", 7 * kSecondsInHour, "MST", "MDT"},
     {"America/Sitka", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"America/St_Barthelemy", 4 * kSecondsInHour, "AST"},
     {"America/St_Johns", static_cast<long>(3.5 * kSecondsInHour), "NST",
      "NDT"},
     {"America/St_Kitts", 4 * kSecondsInHour, "AST"},
     {"America/St_Lucia", 4 * kSecondsInHour, "AST"},
     {"America/St_Thomas", 4 * kSecondsInHour, "AST"},
     {"America/St_Vincent", 4 * kSecondsInHour, "AST"},
     {"America/Swift_Current", 6 * kSecondsInHour, "CST"},
     {"America/Tegucigalpa", 6 * kSecondsInHour, "CST"},
     {"America/Thunder_Bay", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Tijuana", 8 * kSecondsInHour, "PST", "PDT"},
     {"America/Toronto", 5 * kSecondsInHour, "EST", "EDT"},
     {"America/Tortola", 4 * kSecondsInHour, "AST"},
     {"America/Vancouver", 8 * kSecondsInHour, "PST", "PDT"},
     {"America/Virgin", 4 * kSecondsInHour, "AST"},
     {"America/Whitehorse", 7 * kSecondsInHour, "MST"},
     {"America/Winnipeg", 6 * kSecondsInHour, "CST", "CDT"},
     {"America/Yakutat", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"America/Yellowknife", 7 * kSecondsInHour, "MST", "MDT"},
     {"Atlantic/Bermuda", 4 * kSecondsInHour, "AST", "ADT"},
     {"Canada/Saskatchewan", 6 * kSecondsInHour, "CST"},
     {"Canada/Yukon", 7 * kSecondsInHour, "MST"},
     {"Canada/Atlantic", 4 * kSecondsInHour, "AST", "ADT"},
     {"Canada/Central", 6 * kSecondsInHour, "CST", "CDT"},
     {"Canada/East-Saskatchewan", 6 * kSecondsInHour, "CST"},
     {"Canada/Eastern", 5 * kSecondsInHour, "EST", "EDT"},
     {"Canada/Mountain", 7 * kSecondsInHour, "MST", "MDT"},
     {"Canada/Newfoundland", static_cast<long>(3.5 * kSecondsInHour), "NST",
      "NDT"},
     {"Canada/Pacific", 8 * kSecondsInHour, "PST", "PDT"},
     {"Pacific/Honolulu", 10 * kSecondsInHour, "HST"},

     // South America.
     //   https://data.iana.org/time-zones/tzdb/southamerica
     {"America/Argentina/Buenos_Aires", 3 * kSecondsInHour, "ART"},
     {"America/Asuncion", 3 * kSecondsInHour, "PYT"},
     {"America/Bogota", 5 * kSecondsInHour, "COT"},
     {"America/Caracas", 4 * kSecondsInHour, "VET"},
     {"America/Cayenne", 3 * kSecondsInHour, "GFT"},
     {"America/Guyana", 4 * kSecondsInHour, "GYT"},
     {"America/La_Paz", 4 * kSecondsInHour, "BOT"},
     {"America/Lima", 5 * kSecondsInHour, "PET"},
     {"America/Montevideo", 3 * kSecondsInHour, "UYT"},
     {"America/Paramaribo", 3 * kSecondsInHour, "SRT"},
     {"America/Santiago", 4 * kSecondsInHour, "CLT", "CLST"},
     {"America/Sao_Paulo", 3 * kSecondsInHour, "BRT"},

     // Australasia.
     //   https://data.iana.org/time-zones/tzdb/australasia
     {"Antarctica/Macquarie", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Antarctica/McMurdo", -12 * kSecondsInHour, "NZST", "NZDT"},
     {"Antarctica/South_Pole", -12 * kSecondsInHour, "NZST", "NZDT"},
     {"Australia/ACT", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Adelaide", -static_cast<long>(9.5 * kSecondsInHour), "ACST",
      "ACDT"},
     {"Australia/Brisbane", -10 * kSecondsInHour, "AEST"},
     {"Australia/Broken_Hill", -static_cast<long>(9.5 * kSecondsInHour), "ACST",
      "ACDT"},
     {"Australia/Canberra", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Currie", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Darwin", -static_cast<long>(9.5 * kSecondsInHour), "ACST"},
     {"Australia/Hobart", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Lindeman", -10 * kSecondsInHour, "AEST"},
     {"Australia/Melbourne", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/North", -static_cast<long>(9.5 * kSecondsInHour), "ACST"},
     {"Australia/NSW", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Perth", -8 * kSecondsInHour, "AWST"},
     {"Australia/Queensland", -10 * kSecondsInHour, "AEST"},
     {"Australia/South", -static_cast<long>(9.5 * kSecondsInHour), "ACST",
      "ACDT"},
     {"Australia/Sydney", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Tasmania", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/Victoria", -10 * kSecondsInHour, "AEST", "AEDT"},
     {"Australia/West", -8 * kSecondsInHour, "AWST"},
     {"Australia/Yancowinna", -static_cast<long>(9.5 * kSecondsInHour), "ACST",
      "ACDT"},

     // Asia.
     //   https://data.iana.org/time-zones/tzdb/asia
     {"Asia/Beirut", -2 * kSecondsInHour, "EET", "EEST"},
     {"Asia/Calcutta", -static_cast<long>(5.5 * kSecondsInHour), "IST"},
     {"Asia/Dhaka", -6 * kSecondsInHour, "BST"},
     {"Asia/Famagusta", -2 * kSecondsInHour, "EET", "EEST"},
     {"Asia/Gaza", -2 * kSecondsInHour, "EET", "EEST"},
     {"Asia/Harbin", -8 * kSecondsInHour, "CST"},
     {"Asia/Hebron", -2 * kSecondsInHour, "EET", "EEST"},
     {"Asia/Hong_Kong", -8 * kSecondsInHour, "HKT"},
     {"Asia/Istanbul", -3 * kSecondsInHour, "TRT"},
     {"Asia/Jerusalem", -2 * kSecondsInHour, "IST", "IDT"},
     {"Asia/Jakarta", -7 * kSecondsInHour, "WIB"},
     {"Asia/Jayapura", -9 * kSecondsInHour, "WIT"},
     {"Asia/Karachi", -5 * kSecondsInHour, "PKT"},
     {"Asia/Kolkata", -static_cast<long>(5.5 * kSecondsInHour), "IST"},
     {"Asia/Macao", -8 * kSecondsInHour, "CST"},
     {"Asia/Macau", -8 * kSecondsInHour, "CST"},
     {"Asia/Makassar", -8 * kSecondsInHour, "WITA"},
     {"Asia/Manila", -8 * kSecondsInHour, "PHST"},
     {"Asia/Nicosia", -2 * kSecondsInHour, "EET", "EEST"},
     {"Asia/Pontianak", -7 * kSecondsInHour, "WIB"},
     {"Asia/Seoul", -9 * kSecondsInHour, "KST"},
     {"Asia/Singapore", -8 * kSecondsInHour, "SGT"},
     {"Asia/Taipei", -8 * kSecondsInHour, "CST"},
     {"Asia/Tel_Aviv", -2 * kSecondsInHour, "IST", "IDT"},
     {"Asia/Tokyo", -9 * kSecondsInHour, "JST"},
     {"Asia/Ujung_Pandang", -8 * kSecondsInHour, "WITA"},
     {"Atlantic/Reykjavik", 0, "GMT"},
     {"Atlantic/St_Helena", 0, "GMT"},

     // Europe.
     //   https://data.iana.org/time-zones/tzdb/europe
     {"Africa/Ceuta", -1 * kSecondsInHour, "CET", "CEST"},
     {"America/Danmarkshavn", 0, "GMT"},
     {"America/Thule", 4 * kSecondsInHour, "AST", "ADT"},
     {"Arctic/Longyearbyen", -1 * kSecondsInHour, "CET", "CEST"},
     {"Atlantic/Canary", 0, "WET", "WEST"},
     {"Atlantic/Faeroe", 0, "WET", "WEST"},
     {"Atlantic/Faroe", 0, "WET", "WEST"},
     {"Atlantic/Madeira", 0, "WET", "WEST"},
     {"Europe/Amsterdam", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Andorra", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Athens", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Belfast", 0, "GMT", "BST"},
     {"Europe/Belgrade", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Berlin", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Bratislava", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Brussels", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Bucharest", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Budapest", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Busingen", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Chisinau", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Copenhagen", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Dublin", 0, "GMT", "IST"},
     {"Europe/Gibraltar", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Guernsey", 0, "GMT", "BST"},
     {"Europe/Helsinki", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Isle_of_Man", 0, "GMT", "BST"},
     {"Europe/Jersey", 0, "GMT", "BST"},
     {"Europe/Kiev", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Kyiv", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Lisbon", 0, "WET", "WEST"},
     {"Europe/Ljubljana", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/London", 0, "GMT", "BST"},
     {"Europe/Luxembourg", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Madrid", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Malta", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Mariehamn", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Monaco", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Nicosia", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Oslo", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Paris", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Podgorica", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Prague", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Riga", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Rome", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/San_Marino", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Sarajevo", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Skopje", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Sofia", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Stockholm", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Tallinn", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Tirane", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Tiraspol", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Uzhgorod", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Vaduz", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Vatican", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Vienna", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Vilnius", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Warsaw", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Zagreb", -1 * kSecondsInHour, "CET", "CEST"},
     {"Europe/Zaporozhye", -2 * kSecondsInHour, "EET", "EEST"},
     {"Europe/Zurich", -1 * kSecondsInHour, "CET", "CEST"},

     // Legacy Aliases.
     //   https://data.iana.org/time-zones/tzdb/backward
     //   https://data.iana.org/time-zones/tzdb/backzone
     {"Atlantic/Jan_Mayen", -1 * kSecondsInHour, "CET", "CEST"},
     {"CET", -1 * kSecondsInHour, "CET", "CEST"},
     {"CST6CDT", 6 * kSecondsInHour, "CST", "CDT"},
     {"Cuba", 5 * kSecondsInHour, "CST", "CDT"},
     {"EET", -2 * kSecondsInHour, "EET", "EEST"},
     {"Egypt", -2 * kSecondsInHour, "EET", "EEST"},
     {"Eire", 0, "GMT", "IST"},
     {"EST", 5 * kSecondsInHour, "EST"},
     {"EST5EDT", 5 * kSecondsInHour, "EST", "EDT"},
     {"Etc/GMT-0", 0, "GMT"},
     {"Etc/GMT+0", 0, "GMT"},
     {"Etc/GMT0", 0, "GMT"},
     {"Etc/Greenwich", 0, "GMT"},
     {"Etc/UCT", 0, "UTC"},
     {"Etc/Universal", 0, "UTC"},
     {"Etc/Zulu", 0, "UTC"},
     {"GB-Eire", 0, "GMT", "BST"},
     {"GB", 0, "GMT", "BST"},
     {"GMT-0", 0, "GMT"},
     {"GMT+0", 0, "GMT"},
     {"GMT0", 0, "GMT"},
     {"Greenwich", 0, "GMT"},
     {"Hongkong", -8 * kSecondsInHour, "HKT"},
     {"Iceland", 0, "GMT"},
     {"Indian/Antananarivo", -3 * kSecondsInHour, "EAT"},
     {"Indian/Comoro", -3 * kSecondsInHour, "EAT"},
     {"Indian/Mayotte", -3 * kSecondsInHour, "EAT"},
     {"Israel", -2 * kSecondsInHour, "IST", "IDT"},
     {"Jamaica", 5 * kSecondsInHour, "EST"},
     {"Japan", -9 * kSecondsInHour, "JST"},
     {"Libya", -2 * kSecondsInHour, "EET"},
     {"Mexico/BajaNorte", 8 * kSecondsInHour, "PST", "PDT"},
     {"Mexico/BajaSur", 7 * kSecondsInHour, "MST"},
     {"Mexico/General", 6 * kSecondsInHour, "CST"},
     {"MST7MDT", 7 * kSecondsInHour, "MST", "MDT"},
     {"Navajo", 7 * kSecondsInHour, "MST", "MDT"},
     {"NZ", -12 * kSecondsInHour, "NZST", "NZDT"},
     {"Pacific/Auckland", -12 * kSecondsInHour, "NZST", "NZDT"},
     {"Pacific/Guam", -10 * kSecondsInHour, "ChST"},
     {"Pacific/Johnston", 10 * kSecondsInHour, "HST"},
     {"Pacific/Midway", 11 * kSecondsInHour, "SST"},
     {"Pacific/Pago_Pago", 11 * kSecondsInHour, "SST"},
     {"Pacific/Saipan", -10 * kSecondsInHour, "ChST"},
     {"Pacific/Samoa", 11 * kSecondsInHour, "SST"},
     {"Poland", -1 * kSecondsInHour, "CET", "CEST"},
     {"Portugal", 0, "WET", "WEST"},
     {"PST8PDT", 8 * kSecondsInHour, "PST", "PDT"},
     {"ROC", -8 * kSecondsInHour, "CST"},
     {"ROK", -9 * kSecondsInHour, "KST"},
     {"Turkey", -3 * kSecondsInHour, "TRT"},
     {"UCT", 0, "UTC"},
     {"Universal", 0, "UTC"},
     {"US/Alaska", 9 * kSecondsInHour, "AKST", "AKDT"},
     {"US/Aleutian", 10 * kSecondsInHour, "HST", "HDT"},
     {"US/Arizona", 7 * kSecondsInHour, "MST"},
     {"US/Central", 6 * kSecondsInHour, "CST", "CDT"},
     {"US/East-Indiana", 5 * kSecondsInHour, "EST", "EDT"},
     {"US/Eastern", 5 * kSecondsInHour, "EST", "EDT"},
     {"US/Hawaii", 10 * kSecondsInHour, "HST"},
     {"US/Indiana-Starke", 6 * kSecondsInHour, "CST", "CDT"},
     {"US/Michigan", 5 * kSecondsInHour, "EST", "EDT"},
     {"US/Mountain", 7 * kSecondsInHour, "MST", "MDT"},
     {"US/Pacific", 8 * kSecondsInHour, "PST", "PDT"},
     {"US/Samoa", 11 * kSecondsInHour, "SST"},
     {"UTC", 0, "UTC"},
     {"WET", 0, "WET", "WEST"},
     {"Zulu", 0, "UTC"},

     // Etcetera.
     //   https://data.iana.org/time-zones/tzdb/etcetera
     {"Etc/UTC", 0, "UTC"},
     {"Etc/GMT", 0, "GMT"},
     {"GMT", 0, "GMT"}}};

// The unified test case for all enabled IANA timezones.
// It conditionally validates DST properties based on whether
// `dst` is provided.
TEST_P(IANAFormat, TzSetHandles) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  // Assert that the global variables match the expected values.
  EXPECT_EQ(timezone, param.offset)
      << "Timezone offset mismatch for " << param.tz;

  ASSERT_NE(tzname[0], nullptr);
  EXPECT_STREQ(tzname[0], param.std)
      << "Standard TZ name mismatch for " << param.tz;

  ASSERT_NE(tzname[1], nullptr);
  if (param.dst.has_value()) {
    // This is a timezone WITH daylight savings, so we check the DST name
    // and that the daylight flag is set.
    EXPECT_EQ(daylight, 1) << "Daylight flag should be 1 for " << param.tz;
    EXPECT_STREQ(tzname[1], param.dst.value())
        << "Daylight TZ name mismatch for " << param.tz;
  } else {
    // This is a timezone WITHOUT daylight savings. We do not check `daylight`
    // or `tzname[1]`, as their state can be inconsistent for zones that
    // historically had DST.
  }
}

TEST_P(IANAFormat, Localtime) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  TimeSamples samples = GetTimeSamples(2025);

  // Every timezone must have a standard time.
  ASSERT_TRUE(samples.standard.has_value());
  struct tm* tm_standard = localtime(&samples.standard.value());
  ASSERT_NE(tm_standard, nullptr);
  AssertTM(*tm_standard, param.offset, param.std, std::nullopt, param.tz);

  if (param.dst.has_value()) {
    // If the test data expects DST, we must have found a DST sample.
    ASSERT_TRUE(samples.daylight.has_value())
        << "Test data has DST, but no DST time was found for " << param.tz;
    struct tm* tm_daylight = localtime(&samples.daylight.value());
    ASSERT_NE(tm_daylight, nullptr);
    AssertTM(*tm_daylight, param.offset - kSecondsInHour, param.std, param.dst,
             param.tz);
  } else {
    // If the test data does not expect DST, we must not have found one.
    ASSERT_FALSE(samples.daylight.has_value())
        << "Test data has no DST, but a DST time was found for " << param.tz;
  }
}

TEST_P(IANAFormat, Localtime_r) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  TimeSamples samples = GetTimeSamples(2025);

  // Every timezone must have a standard time.
  ASSERT_TRUE(samples.standard.has_value());
  struct tm tm_standard_r;
  struct tm* tm_standard_r_res =
      localtime_r(&samples.standard.value(), &tm_standard_r);
  ASSERT_EQ(tm_standard_r_res, &tm_standard_r);
  AssertTM(tm_standard_r, param.offset, param.std, std::nullopt, param.tz);

  if (param.dst.has_value()) {
    // If the test data expects DST, we must have found a DST sample.
    ASSERT_TRUE(samples.daylight.has_value())
        << "Test data has DST, but no DST time was found for " << param.tz;
    struct tm tm_daylight_r;
    struct tm* tm_daylight_r_res =
        localtime_r(&samples.daylight.value(), &tm_daylight_r);
    ASSERT_EQ(tm_daylight_r_res, &tm_daylight_r);
    AssertTM(tm_daylight_r, param.offset - kSecondsInHour, param.std, param.dst,
             param.tz);
  } else {
    // If the test data does not expect DST, we must not have found one.
    ASSERT_FALSE(samples.daylight.has_value())
        << "Test data has no DST, but a DST time was found for " << param.tz;
  }
}

TEST_P(IANAFormat, Mktime) {
  const auto& param = GetParam();
  ScopedTzSet tz_manager(param.tz);

  TimeSamples samples = GetTimeSamples(2025);

  // Every timezone must have a standard time.
  ASSERT_TRUE(samples.standard.has_value());
  struct tm* tm_standard = localtime(&samples.standard.value());
  ASSERT_NE(tm_standard, nullptr);
  time_t standard_time_rt = mktime(tm_standard);
  EXPECT_EQ(standard_time_rt, samples.standard.value());

  if (samples.daylight.has_value()) {
    struct tm* tm_daylight = localtime(&samples.daylight.value());
    ASSERT_NE(tm_daylight, nullptr);
    time_t daylight_time_rt = mktime(tm_daylight);
    EXPECT_EQ(daylight_time_rt, samples.daylight.value());
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
      "Europe/Dublin",                   // tzname[0] ("IST"),
                                         // tzname[1] ("GMT"), timezone (-3600)
      "Eire",                            // tzname[0] ("IST"),
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
    const std::array<IANATestData, 367>& source_tests,
    bool get_disabled) {
  const auto& disabled_names = GetDisabledTestNames();
  std::vector<IANATestData> filtered_tests;
  for (const auto& test : source_tests) {
    const bool is_disabled =
        (disabled_names.find(test.tz) != disabled_names.end());
    if (is_disabled == get_disabled) {
      filtered_tests.push_back(test);
    }
  }
  return filtered_tests;
}

INSTANTIATE_TEST_SUITE_P(
    PosixTimezoneTests,
    IANAFormat,
    ::testing::ValuesIn(GetFilteredTimezoneTests(IANAFormat::kAllTests, false)),
    GetTestName);

}  // namespace nplb
