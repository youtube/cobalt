// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "base/system/helium_knobs.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"

namespace base::helium {
namespace {

constexpr char kLowEndForSwitch[] = "helium-low-end-for";
constexpr char kSizeSwitch[] = "helium-size";

struct Knobs {
  bool all = false;
  std::set<std::string, std::less<>> low_end_for;
  std::map<std::string, int64_t, std::less<>> sizes;
};

Knobs Parse() {
  Knobs knobs;
  const CommandLine& cmd = *CommandLine::ForCurrentProcess();
  const std::string low_end_for = cmd.GetSwitchValueASCII(kLowEndForSwitch);
  for (const std::string& point :
       SplitString(low_end_for, ",+", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
    if (point == "all") {
      knobs.all = true;
    }
    knobs.low_end_for.insert(point);
  }
  const std::string sizes = cmd.GetSwitchValueASCII(kSizeSwitch);
  for (const std::string& entry :
       SplitString(sizes, ",+", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> kv =
        SplitString(entry, ":", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    int64_t value = 0;
    if (kv.size() == 2 && StringToInt64(kv[1], &value) && value >= 0) {
      knobs.sizes[kv[0]] = value;
    } else {
      LOG(ERROR) << "HELIUM bad --" << kSizeSwitch << " entry: " << entry;
    }
  }
  LOG(INFO) << "HELIUM low_end_device=" << SysInfo::IsLowEndDevice()
            << " low_end_for=[" << low_end_for << "] size=[" << sizes << "]";
  return knobs;
}

const Knobs* GetKnobs() {
  // Don't cache anything read before the command line exists.
  if (!CommandLine::InitializedForCurrentProcess()) {
    return nullptr;
  }
  static const NoDestructor<Knobs> knobs(Parse());
  return knobs.get();
}

}  // namespace

bool IsLowEndFor(std::string_view point) {
  const Knobs* knobs = GetKnobs();
  return knobs && (knobs->all || knobs->low_end_for.contains(point));
}

bool IsLowEndDeviceOr(std::string_view point) {
  return SysInfo::IsLowEndDevice() || IsLowEndFor(point);
}

uint64_t AmountOfPhysicalMemoryFor(std::string_view point) {
  const uint64_t real = SysInfo::AmountOfPhysicalMemory();
  if (IsLowEndFor(point)) {
    // Same value as SysInfo::AmountOfPhysicalMemory() under
    // --enable-low-end-device-mode.
    constexpr uint64_t kLowEndMemory = 512ull * 1024 * 1024;
    return std::min(kLowEndMemory, real);
  }
  return real;
}

std::optional<int64_t> GetSize(std::string_view name) {
  const Knobs* knobs = GetKnobs();
  if (!knobs) {
    return std::nullopt;
  }
  auto it = knobs->sizes.find(name);
  if (it == knobs->sizes.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace base::helium
