// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ModuleProxy.h"

#include "wtf/StdLibExtras.h"

namespace blink {

ModuleProxy& ModuleProxy::moduleProxy()
{
    DEFINE_STATIC_LOCAL(ModuleProxy, moduleProxy, ());
    return moduleProxy;
}

void ModuleProxy::didLeaveScriptContextForRecursionScope(v8::Isolate* isolate)
{
    RELEASE_ASSERT(m_didLeaveScriptContextForRecursionScope);
    (*m_didLeaveScriptContextForRecursionScope)(isolate);
}

void ModuleProxy::registerDidLeaveScriptContextForRecursionScope(void (*didLeaveScriptContext)(v8::Isolate*))
{
    m_didLeaveScriptContextForRecursionScope = didLeaveScriptContext;
}

} // namespace blink
