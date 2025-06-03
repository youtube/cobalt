// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/embedded_test_server_setup_mixin.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace supervised_user {

namespace {

std::string CreateResolverRule(base::StringPiece host,
                               base::StringPiece target) {
  return base::StrCat({"MAP ", host, " ", target});
}

std::vector<std::string> SplitHostList(base::StringPiece host_list) {
  return base::SplitString(host_list, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

EmbeddedTestServerSetupMixin::EmbeddedTestServerSetupMixin(
    InProcessBrowserTestMixinHost& host,
    InProcessBrowserTest* test_base,
    raw_ptr<net::EmbeddedTestServer> server,
    const Options& options)
    : InProcessBrowserTestMixin(&host),
      test_base_(test_base),
      embedded_test_server_(server),
      resolver_rules_map_host_list_(
          SplitHostList(options.resolver_rules_map_host_list)) {
  CHECK(server) << "This mixin requires an embedded test server";
}

EmbeddedTestServerSetupMixin::~EmbeddedTestServerSetupMixin() = default;

void EmbeddedTestServerSetupMixin::SetUp() {
  CHECK(embedded_test_server_->InitializeAndListen());
}

void EmbeddedTestServerSetupMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  CHECK(embedded_test_server_->Started());

  std::string target = embedded_test_server_->host_port_pair().ToString();
  std::vector<std::string> resolver_rules(resolver_rules_map_host_list_.size());

  base::ranges::transform(resolver_rules_map_host_list_, resolver_rules.begin(),
                          [&](const std::string& host) -> std::string {
                            return CreateResolverRule(host, target);
                          });

  command_line->AppendSwitchASCII(
      network::switches::kHostResolverRules,
      base::JoinString(base::span<std::string>(resolver_rules), ", "));

  LOG(INFO) << "Embedded test server is listening on " << target << ".";
  LOG(INFO) << "Following hosts will be mapped to it: ";
  for (const std::string& host_pattern : resolver_rules_map_host_list_) {
    LOG(INFO) << "\t" << host_pattern;
  }
}

void EmbeddedTestServerSetupMixin::SetUpOnMainThread() {
  embedded_test_server_->StartAcceptingConnections();
}

void EmbeddedTestServerSetupMixin::TearDownOnMainThread() {
  CHECK(embedded_test_server_->ShutdownAndWaitUntilComplete());
}
}  // namespace supervised_user
