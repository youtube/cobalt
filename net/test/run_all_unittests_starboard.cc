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

#include <iostream>

#include "base/build_time.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/net_test_suite.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "url/buildflags.h"

namespace {

bool VerifyBuildIsTimely() {
  // This lines up with various //net security features, like Certificate
  // Transparency or HPKP, in that they require the build time be less than 70
  // days old. Moreover, operating on the assumption that tests are run against
  // recently compiled builds, this also serves as a sanity check for the
  // system clock, which should be close to the build date.
  base::TimeDelta kMaxAge = base::Days(70);

  base::Time build_time = base::GetBuildTime();
  base::Time now = base::Time::Now();

  if ((now - build_time).magnitude() <= kMaxAge)
    return true;

  std::cerr
      << "ERROR: This build is more than " << kMaxAge.InDays()
      << " days out of date.\n"
         "This could indicate a problem with the device's clock, or the build "
         "is simply too old.\n"
         "See crbug.com/666821 for why this is a problem\n"
      << "    base::Time::Now() --> " << now << " (" << now.ToInternalValue()
      << ")\n"
      << "    base::GetBuildTime() --> " << build_time << " ("
      << build_time.ToInternalValue() << ")\n";

  return false;
}

}  // namespace

static int InitAndRunAllTests(int argc, char** argv) {
  if (!VerifyBuildIsTimely())
    return 1;

  NetTestSuite test_suite(argc, argv);
  net::TransportClientSocketPool::set_connect_backup_jobs_enabled(false);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&NetTestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif