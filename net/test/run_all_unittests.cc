// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/build_time.h"
#include "base/functional/bind.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/test/net_test_suite.h"
#include "url/buildflags.h"

#if defined(STARBOARD)
#include "starboard/client_porting/wrap_main/wrap_main.h"

int TestSuiteRun(int argc, char** argv) {
  // set_connect_backup_jobs_enabled(false) below disables backup transport
  // layer connection which is turned on by default. The backup transport layer
  // connection sends new connection if certain amount of time has passed
  // without ACK being received. Some net_unittests have assumption for the
  // lack of this feature.
  net::TransportClientSocketPool::set_connect_backup_jobs_enabled(false);
  base::AtExitManager exit_manager;
  base::test::AllowCheckIsTestForTesting();
  return NetTestSuite(argc, argv).Run();
}

STARBOARD_WRAP_SIMPLE_MAIN(TestSuiteRun);

#else

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

int main(int argc, char** argv) {
  if (!VerifyBuildIsTimely())
    return 1;

  NetTestSuite test_suite(argc, argv);
  net::TransportClientSocketPool::set_connect_backup_jobs_enabled(false);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&NetTestSuite::Run, base::Unretained(&test_suite)));
}

#endif
