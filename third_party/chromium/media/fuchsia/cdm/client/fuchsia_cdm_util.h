// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_

#include <memory>

namespace blink {
class BrowserInterfaceBrokerProxy;
}

namespace media_m96 {
class CdmFactory;

std::unique_ptr<CdmFactory> CreateFuchsiaCdmFactory(
    blink::BrowserInterfaceBrokerProxy* interface_broker);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_
