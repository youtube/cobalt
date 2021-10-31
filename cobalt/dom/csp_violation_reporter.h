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

#ifndef COBALT_DOM_CSP_VIOLATION_REPORTER_H_
#define COBALT_DOM_CSP_VIOLATION_REPORTER_H_

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/base/source_location.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/network_bridge/net_poster.h"

namespace cobalt {
namespace dom {

class Document;

// Responsible for reporting CSP violations, i.e. posting JSON
// reports to any reporting endpoints described by the policy.
// This object should be created by the Document that owns a CspDelegate,
// and passed in to its constructor. Generally it should not be called directly.
class CspViolationReporter {
 public:
  CspViolationReporter(Document* document,
                       const network_bridge::PostSender& post_sender);
  virtual ~CspViolationReporter();

  // Used as a callback from ContentSecurityPolicy to dispatch security
  // violation events on the Document and send the reports.
  // Report() can be called from any thread and it will re-post itself
  // to the message loop it was created on, which should be the Document's
  // message loop.
  virtual void Report(const csp::ViolationInfo& violation_info);
  const network_bridge::PostSender& post_sender() const { return post_sender_; }

 private:
  void SendViolationReports(const std::vector<std::string>& endpoints,
                            const std::string& report);

  // Hashes of the reports we've already sent. We don't keep track of the report
  // data, as it is large, and we don't consider the reports so important that
  // we need to resolve collisions.
  base::hash_set<uint32> violation_reports_sent_;
  // Callback to send POST requests containing our JSON reports.
  network_bridge::PostSender post_sender_;
  // Keep track of the message loop the object was created on.
  // We must send violations on the document's message loop.
  base::MessageLoop* message_loop_;

  Document* document_;

  DISALLOW_COPY_AND_ASSIGN(CspViolationReporter);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSP_VIOLATION_REPORTER_H_
