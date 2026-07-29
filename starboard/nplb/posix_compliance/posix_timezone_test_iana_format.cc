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
    {{.tz = "Africa/Abidjan", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Accra", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Addis_Ababa", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Africa/Algiers", .offset = -1 * kSecondsInHour, .std = "CET"},
     {.tz = "Africa/Bamako", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Bangui", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Banjul", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Bissau", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Blantyre", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Brazzaville", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Bujumbura", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Cairo",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Africa/Conakry", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Dakar", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Dar_es_Salaam",
      .offset = -3 * kSecondsInHour,
      .std = "EAT"},
     {.tz = "Africa/Djibouti", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Africa/Douala", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Freetown", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Gaborone", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Harare", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Johannesburg",
      .offset = -2 * kSecondsInHour,
      .std = "SAST"},
     {.tz = "Africa/Juba", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Kampala", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Africa/Khartoum", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Kigali", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Kinshasa", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Lagos", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Libreville", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Lome", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Luanda", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Lubumbashi", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Lusaka", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Malabo", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Maputo", .offset = -2 * kSecondsInHour, .std = "CAT"},
     {.tz = "Africa/Maseru", .offset = -2 * kSecondsInHour, .std = "SAST"},
     {.tz = "Africa/Mbabane", .offset = -2 * kSecondsInHour, .std = "SAST"},
     {.tz = "Africa/Mogadishu", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Africa/Monrovia", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Nairobi", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Africa/Ndjamena", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Niamey", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Nouakchott", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Ouagadougou", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Porto-Novo", .offset = -1 * kSecondsInHour, .std = "WAT"},
     {.tz = "Africa/Sao_Tome", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Timbuktu", .offset = 0, .std = "GMT"},
     {.tz = "Africa/Tripoli", .offset = -2 * kSecondsInHour, .std = "EET"},
     {.tz = "Africa/Tunis", .offset = -1 * kSecondsInHour, .std = "CET"},
     {.tz = "Africa/Windhoek", .offset = -2 * kSecondsInHour, .std = "CAT"},

     // North America.
     //   https://data.iana.org/time-zones/tzdb/northamerica
     {.tz = "America/Adak",
      .offset = 10 * kSecondsInHour,
      .std = "HST",
      .dst = "HDT"},
     {.tz = "America/Anchorage",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "America/Anguilla", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Antigua", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Aruba", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Atikokan", .offset = 5 * kSecondsInHour, .std = "EST"},
     {.tz = "America/Atka",
      .offset = 10 * kSecondsInHour,
      .std = "HST",
      .dst = "HDT"},
     {.tz = "America/Bahia_Banderas",
      .offset = 6 * kSecondsInHour,
      .std = "CST"},
     {.tz = "America/Barbados", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Belize", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Blanc-Sablon", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Boise",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/Cambridge_Bay",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/Cancun", .offset = 5 * kSecondsInHour, .std = "EST"},
     {.tz = "America/Cayman", .offset = 5 * kSecondsInHour, .std = "EST"},
     {.tz = "America/Chicago",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Chihuahua", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Ciudad_Juarez",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/Coral_Harbour",
      .offset = 5 * kSecondsInHour,
      .std = "EST"},
     {.tz = "America/Costa_Rica", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Creston", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Curacao", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Dawson_Creek", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Dawson", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Denver",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/Detroit",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Dominica", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Edmonton",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/El_Salvador", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Ensenada",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "America/Fort_Nelson", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Fort_Wayne",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Glace_Bay",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "America/Goose_Bay",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "America/Grand_Turk",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Grenada", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Guadeloupe", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Guatemala", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Halifax",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "America/Havana",
      .offset = 5 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Hermosillo", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Indiana/Indianapolis",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Indiana/Knox",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Indiana/Marengo",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Indiana/Petersburg",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Indiana/Tell_City",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Indiana/Vevay",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Indiana/Vincennes",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Indiana/Winamac",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Indianapolis",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Inuvik",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/Iqaluit",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Juneau",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "America/Kentucky/Louisville",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Kentucky/Monticello",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Knox_IN",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Kralendijk", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Los_Angeles",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "America/Louisville",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Lower_Princes",
      .offset = 4 * kSecondsInHour,
      .std = "AST"},
     {.tz = "America/Managua", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Marigot", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Martinique", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Matamoros",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Mazatlan", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Menominee",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Merida", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Metlakatla",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "America/Mexico_City", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Moncton",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "America/Monterrey", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Montreal",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Montserrat", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Nassau",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/New_York",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Nipigon",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Nome",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "America/North_Dakota/Beulah",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/North_Dakota/Center",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/North_Dakota/New_Salem",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Ojinaga",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Panama", .offset = 5 * kSecondsInHour, .std = "EST"},
     {.tz = "America/Pangnirtung",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Phoenix", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Port_of_Spain",
      .offset = 4 * kSecondsInHour,
      .std = "AST"},
     {.tz = "America/Port-au-Prince",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Puerto_Rico", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Rainy_River",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Rankin_Inlet",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Regina", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Resolute",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Santa_Isabel",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "America/Santo_Domingo",
      .offset = 4 * kSecondsInHour,
      .std = "AST"},
     {.tz = "America/Shiprock",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "America/Sitka",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "America/St_Barthelemy",
      .offset = 4 * kSecondsInHour,
      .std = "AST"},
     {.tz = "America/St_Johns",
      .offset = static_cast<long>(3.5 * kSecondsInHour),
      .std = "NST",
      .dst = "NDT"},
     {.tz = "America/St_Kitts", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/St_Lucia", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/St_Thomas", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/St_Vincent", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Swift_Current",
      .offset = 6 * kSecondsInHour,
      .std = "CST"},
     {.tz = "America/Tegucigalpa", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "America/Thunder_Bay",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Tijuana",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "America/Toronto",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "America/Tortola", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Vancouver",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "America/Virgin", .offset = 4 * kSecondsInHour, .std = "AST"},
     {.tz = "America/Whitehorse", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "America/Winnipeg",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "America/Yakutat",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "America/Yellowknife",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "Atlantic/Bermuda",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "Canada/Saskatchewan", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "Canada/Yukon", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "Canada/Atlantic",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "Canada/Central",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "Canada/East-Saskatchewan",
      .offset = 6 * kSecondsInHour,
      .std = "CST"},
     {.tz = "Canada/Eastern",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "Canada/Mountain",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "Canada/Newfoundland",
      .offset = static_cast<long>(3.5 * kSecondsInHour),
      .std = "NST",
      .dst = "NDT"},
     {.tz = "Canada/Pacific",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "Pacific/Honolulu", .offset = 10 * kSecondsInHour, .std = "HST"},

     // South America.
     //   https://data.iana.org/time-zones/tzdb/southamerica
     {.tz = "America/Argentina/Buenos_Aires",
      .offset = 3 * kSecondsInHour,
      .std = "ART"},
     {.tz = "America/Asuncion", .offset = 3 * kSecondsInHour, .std = "PYT"},
     {.tz = "America/Bogota", .offset = 5 * kSecondsInHour, .std = "COT"},
     {.tz = "America/Caracas", .offset = 4 * kSecondsInHour, .std = "VET"},
     {.tz = "America/Cayenne", .offset = 3 * kSecondsInHour, .std = "GFT"},
     {.tz = "America/Guyana", .offset = 4 * kSecondsInHour, .std = "GYT"},
     {.tz = "America/La_Paz", .offset = 4 * kSecondsInHour, .std = "BOT"},
     {.tz = "America/Lima", .offset = 5 * kSecondsInHour, .std = "PET"},
     {.tz = "America/Montevideo", .offset = 3 * kSecondsInHour, .std = "UYT"},
     {.tz = "America/Paramaribo", .offset = 3 * kSecondsInHour, .std = "SRT"},
     {.tz = "America/Santiago",
      .offset = 4 * kSecondsInHour,
      .std = "CLT",
      .dst = "CLST"},
     {.tz = "America/Sao_Paulo", .offset = 3 * kSecondsInHour, .std = "BRT"},

     // Australasia.
     //   https://data.iana.org/time-zones/tzdb/australasia
     {.tz = "Antarctica/Macquarie",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Antarctica/McMurdo",
      .offset = -12 * kSecondsInHour,
      .std = "NZST",
      .dst = "NZDT"},
     {.tz = "Antarctica/South_Pole",
      .offset = -12 * kSecondsInHour,
      .std = "NZST",
      .dst = "NZDT"},
     {.tz = "Australia/ACT",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Adelaide",
      .offset = -static_cast<long>(9.5 * kSecondsInHour),
      .std = "ACST",
      .dst = "ACDT"},
     {.tz = "Australia/Brisbane",
      .offset = -10 * kSecondsInHour,
      .std = "AEST"},
     {.tz = "Australia/Broken_Hill",
      .offset = -static_cast<long>(9.5 * kSecondsInHour),
      .std = "ACST",
      .dst = "ACDT"},
     {.tz = "Australia/Canberra",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Currie",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Darwin",
      .offset = -static_cast<long>(9.5 * kSecondsInHour),
      .std = "ACST"},
     {.tz = "Australia/Hobart",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Lindeman",
      .offset = -10 * kSecondsInHour,
      .std = "AEST"},
     {.tz = "Australia/Melbourne",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/North",
      .offset = -static_cast<long>(9.5 * kSecondsInHour),
      .std = "ACST"},
     {.tz = "Australia/NSW",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Perth", .offset = -8 * kSecondsInHour, .std = "AWST"},
     {.tz = "Australia/Queensland",
      .offset = -10 * kSecondsInHour,
      .std = "AEST"},
     {.tz = "Australia/South",
      .offset = -static_cast<long>(9.5 * kSecondsInHour),
      .std = "ACST",
      .dst = "ACDT"},
     {.tz = "Australia/Sydney",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Tasmania",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/Victoria",
      .offset = -10 * kSecondsInHour,
      .std = "AEST",
      .dst = "AEDT"},
     {.tz = "Australia/West", .offset = -8 * kSecondsInHour, .std = "AWST"},
     {.tz = "Australia/Yancowinna",
      .offset = -static_cast<long>(9.5 * kSecondsInHour),
      .std = "ACST",
      .dst = "ACDT"},

     // Asia.
     //   https://data.iana.org/time-zones/tzdb/asia
     {.tz = "Asia/Beirut",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Asia/Calcutta",
      .offset = -static_cast<long>(5.5 * kSecondsInHour),
      .std = "IST"},
     {.tz = "Asia/Dhaka", .offset = -6 * kSecondsInHour, .std = "BST"},
     {.tz = "Asia/Famagusta",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Asia/Gaza",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Asia/Harbin", .offset = -8 * kSecondsInHour, .std = "CST"},
     {.tz = "Asia/Hebron",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Asia/Hong_Kong", .offset = -8 * kSecondsInHour, .std = "HKT"},
     {.tz = "Asia/Istanbul", .offset = -3 * kSecondsInHour, .std = "TRT"},
     {.tz = "Asia/Jerusalem",
      .offset = -2 * kSecondsInHour,
      .std = "IST",
      .dst = "IDT"},
     {.tz = "Asia/Jakarta", .offset = -7 * kSecondsInHour, .std = "WIB"},
     {.tz = "Asia/Jayapura", .offset = -9 * kSecondsInHour, .std = "WIT"},
     {.tz = "Asia/Karachi", .offset = -5 * kSecondsInHour, .std = "PKT"},
     {.tz = "Asia/Kolkata",
      .offset = -static_cast<long>(5.5 * kSecondsInHour),
      .std = "IST"},
     {.tz = "Asia/Macao", .offset = -8 * kSecondsInHour, .std = "CST"},
     {.tz = "Asia/Macau", .offset = -8 * kSecondsInHour, .std = "CST"},
     {.tz = "Asia/Makassar", .offset = -8 * kSecondsInHour, .std = "WITA"},
     {.tz = "Asia/Manila", .offset = -8 * kSecondsInHour, .std = "PHST"},
     {.tz = "Asia/Nicosia",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Asia/Pontianak", .offset = -7 * kSecondsInHour, .std = "WIB"},
     {.tz = "Asia/Seoul", .offset = -9 * kSecondsInHour, .std = "KST"},
     {.tz = "Asia/Singapore", .offset = -8 * kSecondsInHour, .std = "SGT"},
     {.tz = "Asia/Taipei", .offset = -8 * kSecondsInHour, .std = "CST"},
     {.tz = "Asia/Tel_Aviv",
      .offset = -2 * kSecondsInHour,
      .std = "IST",
      .dst = "IDT"},
     {.tz = "Asia/Tokyo", .offset = -9 * kSecondsInHour, .std = "JST"},
     {.tz = "Asia/Ujung_Pandang", .offset = -8 * kSecondsInHour, .std = "WITA"},
     {.tz = "Atlantic/Reykjavik", .offset = 0, .std = "GMT"},
     {.tz = "Atlantic/St_Helena", .offset = 0, .std = "GMT"},

     // Europe.
     //   https://data.iana.org/time-zones/tzdb/europe
     {.tz = "Africa/Ceuta",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "America/Danmarkshavn", .offset = 0, .std = "GMT"},
     {.tz = "America/Thule",
      .offset = 4 * kSecondsInHour,
      .std = "AST",
      .dst = "ADT"},
     {.tz = "Arctic/Longyearbyen",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Atlantic/Canary", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "Atlantic/Faeroe", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "Atlantic/Faroe", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "Atlantic/Madeira", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "Europe/Amsterdam",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Andorra",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Athens",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Belfast", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "Europe/Belgrade",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Berlin",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Bratislava",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Brussels",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Bucharest",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Budapest",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Busingen",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Chisinau",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Copenhagen",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Dublin", .offset = 0, .std = "GMT", .dst = "IST"},
     {.tz = "Europe/Gibraltar",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Guernsey", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "Europe/Helsinki",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Isle_of_Man", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "Europe/Jersey", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "Europe/Kiev",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Kyiv",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Lisbon", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "Europe/Ljubljana",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/London", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "Europe/Luxembourg",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Madrid",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Malta",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Mariehamn",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Monaco",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Nicosia",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Oslo",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Paris",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Podgorica",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Prague",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Riga",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Rome",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/San_Marino",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Sarajevo",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Skopje",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Sofia",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Stockholm",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Tallinn",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Tirane",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Tiraspol",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Uzhgorod",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Vaduz",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Vatican",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Vienna",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Vilnius",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Warsaw",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Zagreb",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Europe/Zaporozhye",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Europe/Zurich",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},

     // Legacy Aliases.
     //   https://data.iana.org/time-zones/tzdb/backward
     //   https://data.iana.org/time-zones/tzdb/backzone
     {.tz = "Atlantic/Jan_Mayen",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "CET", .offset = -1 * kSecondsInHour, .std = "CET", .dst = "CEST"},
     {.tz = "CST6CDT",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "Cuba", .offset = 5 * kSecondsInHour, .std = "CST", .dst = "CDT"},
     {.tz = "EET", .offset = -2 * kSecondsInHour, .std = "EET", .dst = "EEST"},
     {.tz = "Egypt",
      .offset = -2 * kSecondsInHour,
      .std = "EET",
      .dst = "EEST"},
     {.tz = "Eire", .offset = 0, .std = "GMT", .dst = "IST"},
     {.tz = "EST", .offset = 5 * kSecondsInHour, .std = "EST"},
     {.tz = "EST5EDT",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "Etc/GMT-0", .offset = 0, .std = "GMT"},
     {.tz = "Etc/GMT+0", .offset = 0, .std = "GMT"},
     {.tz = "Etc/GMT0", .offset = 0, .std = "GMT"},
     {.tz = "Etc/Greenwich", .offset = 0, .std = "GMT"},
     {.tz = "Etc/UCT", .offset = 0, .std = "UTC"},
     {.tz = "Etc/Universal", .offset = 0, .std = "UTC"},
     {.tz = "Etc/Zulu", .offset = 0, .std = "UTC"},
     {.tz = "GB-Eire", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "GB", .offset = 0, .std = "GMT", .dst = "BST"},
     {.tz = "GMT-0", .offset = 0, .std = "GMT"},
     {.tz = "GMT+0", .offset = 0, .std = "GMT"},
     {.tz = "GMT0", .offset = 0, .std = "GMT"},
     {.tz = "Greenwich", .offset = 0, .std = "GMT"},
     {.tz = "Hongkong", .offset = -8 * kSecondsInHour, .std = "HKT"},
     {.tz = "Iceland", .offset = 0, .std = "GMT"},
     {.tz = "Indian/Antananarivo", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Indian/Comoro", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Indian/Mayotte", .offset = -3 * kSecondsInHour, .std = "EAT"},
     {.tz = "Israel",
      .offset = -2 * kSecondsInHour,
      .std = "IST",
      .dst = "IDT"},
     {.tz = "Jamaica", .offset = 5 * kSecondsInHour, .std = "EST"},
     {.tz = "Japan", .offset = -9 * kSecondsInHour, .std = "JST"},
     {.tz = "Libya", .offset = -2 * kSecondsInHour, .std = "EET"},
     {.tz = "Mexico/BajaNorte",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "Mexico/BajaSur", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "Mexico/General", .offset = 6 * kSecondsInHour, .std = "CST"},
     {.tz = "MST7MDT",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "Navajo", .offset = 7 * kSecondsInHour, .std = "MST", .dst = "MDT"},
     {.tz = "NZ", .offset = -12 * kSecondsInHour, .std = "NZST", .dst = "NZDT"},
     {.tz = "Pacific/Auckland",
      .offset = -12 * kSecondsInHour,
      .std = "NZST",
      .dst = "NZDT"},
     {.tz = "Pacific/Guam", .offset = -10 * kSecondsInHour, .std = "ChST"},
     {.tz = "Pacific/Johnston", .offset = 10 * kSecondsInHour, .std = "HST"},
     {.tz = "Pacific/Midway", .offset = 11 * kSecondsInHour, .std = "SST"},
     {.tz = "Pacific/Pago_Pago", .offset = 11 * kSecondsInHour, .std = "SST"},
     {.tz = "Pacific/Saipan", .offset = -10 * kSecondsInHour, .std = "ChST"},
     {.tz = "Pacific/Samoa", .offset = 11 * kSecondsInHour, .std = "SST"},
     {.tz = "Poland",
      .offset = -1 * kSecondsInHour,
      .std = "CET",
      .dst = "CEST"},
     {.tz = "Portugal", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "PST8PDT",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "ROC", .offset = -8 * kSecondsInHour, .std = "CST"},
     {.tz = "ROK", .offset = -9 * kSecondsInHour, .std = "KST"},
     {.tz = "Turkey", .offset = -3 * kSecondsInHour, .std = "TRT"},
     {.tz = "UCT", .offset = 0, .std = "UTC"},
     {.tz = "Universal", .offset = 0, .std = "UTC"},
     {.tz = "US/Alaska",
      .offset = 9 * kSecondsInHour,
      .std = "AKST",
      .dst = "AKDT"},
     {.tz = "US/Aleutian",
      .offset = 10 * kSecondsInHour,
      .std = "HST",
      .dst = "HDT"},
     {.tz = "US/Arizona", .offset = 7 * kSecondsInHour, .std = "MST"},
     {.tz = "US/Central",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "US/East-Indiana",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "US/Eastern",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "US/Hawaii", .offset = 10 * kSecondsInHour, .std = "HST"},
     {.tz = "US/Indiana-Starke",
      .offset = 6 * kSecondsInHour,
      .std = "CST",
      .dst = "CDT"},
     {.tz = "US/Michigan",
      .offset = 5 * kSecondsInHour,
      .std = "EST",
      .dst = "EDT"},
     {.tz = "US/Mountain",
      .offset = 7 * kSecondsInHour,
      .std = "MST",
      .dst = "MDT"},
     {.tz = "US/Pacific",
      .offset = 8 * kSecondsInHour,
      .std = "PST",
      .dst = "PDT"},
     {.tz = "US/Samoa", .offset = 11 * kSecondsInHour, .std = "SST"},
     {.tz = "UTC", .offset = 0, .std = "UTC"},
     {.tz = "WET", .offset = 0, .std = "WET", .dst = "WEST"},
     {.tz = "Zulu", .offset = 0, .std = "UTC"},

     // Etcetera.
     //   https://data.iana.org/time-zones/tzdb/etcetera
     {.tz = "Etc/UTC", .offset = 0, .std = "UTC"},
     {.tz = "Etc/GMT", .offset = 0, .std = "GMT"},
     {.tz = "GMT", .offset = 0, .std = "GMT"}}};

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
