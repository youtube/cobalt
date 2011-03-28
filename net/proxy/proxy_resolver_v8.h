// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_H_
#define NET_PROXY_PROXY_RESOLVER_V8_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "net/proxy/proxy_resolver.h"

namespace net {

class ProxyResolverJSBindings;

// Implementation of ProxyResolver that uses V8 to evaluate PAC scripts.
//
// ----------------------------------------------------------------------------
// !!! Important note on threading model:
// ----------------------------------------------------------------------------
// There can be only one instance of V8 running at a time. To enforce this
// constraint, ProxyResolverV8 holds a v8::Locker during execution. Therefore
// it is OK to run multiple instances of ProxyResolverV8 on different threads,
// since only one will be running inside V8 at a time.
//
// It is important that *ALL* instances of V8 in the process be using
// v8::Locker. If not there can be race conditions beween the non-locked V8
// instances and the locked V8 instances used by ProxyResolverV8 (assuming they
// run on different threads).
//
// This is the case with the V8 instance used by chromium's renderer -- it runs
// on a different thread from ProxyResolver (renderer thread vs PAC thread),
// and does not use locking since it expects to be alone.
class ProxyResolverV8 : public ProxyResolver {
 public:
  // Constructs a ProxyResolverV8 with custom bindings. ProxyResolverV8 takes
  // ownership of |custom_js_bindings| and deletes it when ProxyResolverV8
  // is destroyed.
  explicit ProxyResolverV8(ProxyResolverJSBindings* custom_js_bindings);

  virtual ~ProxyResolverV8();

  ProxyResolverJSBindings* js_bindings() const { return js_bindings_.get(); }

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* /*callback*/,
                             RequestHandle* /*request*/,
                             const BoundNetLog& net_log);
  virtual void CancelRequest(RequestHandle request);
  virtual void CancelSetPacScript();
  virtual void PurgeMemory();
  virtual void Shutdown();
  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      CompletionCallback* /*callback*/);

 private:
  // Context holds the Javascript state for the most recently loaded PAC
  // script. It corresponds with the data from the last call to
  // SetPacScript().
  class Context;
  scoped_ptr<Context> context_;

  scoped_ptr<ProxyResolverJSBindings> js_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_H_
