// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mapped_host_resolver.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

namespace net {

MappedHostResolver::MappedHostResolver(HostResolver* impl)
    : impl_(impl) {
}

MappedHostResolver::~MappedHostResolver() {
}

int MappedHostResolver::Resolve(const RequestInfo& info,
                                AddressList* addresses,
                                CompletionCallback* callback,
                                RequestHandle* out_req,
                                const BoundNetLog& net_log) {
  // Modify the request before forwarding it to |impl_|.
  RequestInfo modified_info = info;
  HostPortPair host_port(info.host_port_pair());
  if (rules_.RewriteHost(&host_port))
    modified_info.set_host_port_pair(host_port);
  return impl_->Resolve(modified_info, addresses, callback, out_req, net_log);
}

void MappedHostResolver::CancelRequest(RequestHandle req) {
  impl_->CancelRequest(req);
}

void MappedHostResolver::AddObserver(Observer* observer) {
  impl_->AddObserver(observer);
}

void MappedHostResolver::RemoveObserver(Observer* observer) {
  impl_->RemoveObserver(observer);
}

HostResolverImpl* MappedHostResolver::GetAsHostResolverImpl() {
  return impl_->GetAsHostResolverImpl();
}

}  // namespace net
