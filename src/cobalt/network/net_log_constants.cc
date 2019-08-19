// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/network/net_log_constants.h"

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)
#include "net/base/address_family.h"
#include "net/base/load_states.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"

namespace cobalt {
namespace network {
namespace {
// Same log format as in M25.
const int kLogFormatVersion = 1;
}

std::unique_ptr<base::DictionaryValue> NetLogConstants::GetConstants() {
  // This function is based on the implementation of
  // NetInternalsUI::GetConstants() in Chromium 25.
  std::unique_ptr<base::DictionaryValue> constants_dict(
      new base::DictionaryValue());

  // Version of the file format.
  constants_dict->SetInteger("logFormatVersion", kLogFormatVersion);

  // Add a dictionary with information on the relationship between event type
  // enums and their symbolic names.
  constants_dict->Set("logEventTypes", net::NetLog::GetEventTypesAsValue());

  // Add a dictionary with the version of the client and its command line
  // arguments.
  {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

    dict->SetString("name", "Cobalt");
    dict->SetString("version", base::StringPrintf("%s.%s", COBALT_VERSION,
                                                  COBALT_BUILD_VERSION_NUMBER));
    base::TimeDelta time_since_epoch =
        base::Time::Now() - base::Time::UnixEpoch();

    // Not sure where this gets added, but net-internals seems to like it.
    dict->SetDouble("numericDate",
                    static_cast<double>(time_since_epoch.InMilliseconds()));

    // A number of other keys are in clientInfo that either aren't relevant to
    // Cobalt or just not too interesting for us:
    // "cl", "version_mod", "official", "os_type", "command_line"
    //
    // The tool in Chrome handles the absence of these with no problem. See
    // NetInternalsUI::GetConstants() for how these are set for Chrome.

    constants_dict->Set("clientInfo", std::move(dict));
  }

  // Add a dictionary with information about the relationship between load flag
  // enums and their symbolic names.
  {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define LOAD_FLAG(label, value) \
  dict->SetInteger(#label, static_cast<int>(value));
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG

    constants_dict->Set("loadFlag", std::move(dict));
  }

  // Add a dictionary with information about the relationship between load state
  // enums and their symbolic names.
  {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define LOAD_STATE(label, value) \
  dict->SetInteger(#label, static_cast<int>(value));
#include "net/base/load_states_list.h"
#undef LOAD_STATE

    constants_dict->Set("loadState", std::move(dict));
  }

  // Add information on the relationship between net error codes and their
  // symbolic names.
  {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define NET_ERROR(label, value) \
  dict->SetInteger(#label, static_cast<int>(value));
#include "net/base/net_error_list.h"
#undef NET_ERROR

    constants_dict->Set("netError", std::move(dict));
  }

  // Information about the relationship between event phase enums and their
  // symbolic names.
  {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

    // TODO: find out where the corresponding integer number for each
    // event phase lives and replace the following hard-coded number with them.
    dict->SetInteger("PHASE_BEGIN", 0);
    dict->SetInteger("PHASE_END", 1);
    dict->SetInteger("PHASE_NONE", 2);

    constants_dict->Set("logEventPhase", std::move(dict));
  }

  // Information about the relationship between source type enums and
  // their symbolic names.
  constants_dict->Set("logSourceType", net::NetLog::GetSourceTypesAsValue());

  // Information about the relationship between LogLevel enums and their
  // symbolic names.
  // {
  //   std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  //   dict->SetInteger("DEFAULT", "net::NetLog::NetLogCaptureMode::Default()");
  //   dict->SetInteger("INCLUDE_COOKIES_AND_CREDENTIALS",
  //                   "net::NetLogCaptureMode::IncludeCookiesAndCredentials()");
  //   dict->SetInteger("INCLUDE_SOCKET_BYTES",
  //                   "net::NetLogCaptureMode::IncludeSocketBytes()");

  //   constants_dict->Set("logCaptureMode", std::move(dict));
  // }

  // Information about the relationship between address family enums and
  // their symbolic names.
  {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

    dict->SetInteger("ADDRESS_FAMILY_UNSPECIFIED",
                     net::ADDRESS_FAMILY_UNSPECIFIED);
    dict->SetInteger("ADDRESS_FAMILY_IPV4", net::ADDRESS_FAMILY_IPV4);
    dict->SetInteger("ADDRESS_FAMILY_IPV6", net::ADDRESS_FAMILY_IPV6);

    constants_dict->Set("addressFamily", std::move(dict));
  }

  // Information about how the "time ticks" values we have given it relate to
  // actual system times. (We used time ticks throughout since they are stable
  // across system clock changes).
  {
    int64 cur_time_ms = (base::Time::Now() - base::Time()).InMilliseconds();

    int64 cur_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();

    // If we add this number to a time tick value, it gives the timestamp.
    int64 tick_to_time_ms = cur_time_ms - cur_time_ticks_ms;

    // Chrome on all platforms stores times using the Windows epoch
    // (Jan 1 1601), but the javascript wants a unix epoch.
    // TODO(eroman): Getting the timestamp relative to the unix epoch should
    //               be part of the time library.
    const int64 kUnixEpochMs = 11644473600000LL;
    int64 tick_to_unix_time_ms = tick_to_time_ms - kUnixEpochMs;

    // Pass it as a string, since it may be too large to fit in an integer.
    constants_dict->SetString("timeTickOffset",
                              base::Int64ToString(tick_to_unix_time_ms));
  }
  return constants_dict;
}

}  // namespace network
}  // namespace cobalt
