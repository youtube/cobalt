// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_
#define COBALT_MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/media/base/cdm_config.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleAccess.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace media {

class WebEncryptedMediaClientImpl;

class WebContentDecryptionModuleAccessImpl
    : public blink::WebContentDecryptionModuleAccess {
 public:
  static WebContentDecryptionModuleAccessImpl* Create(
      const blink::WebString& key_system,
      const blink::WebSecurityOrigin& security_origin,
      const blink::WebMediaKeySystemConfiguration& configuration,
      const CdmConfig& cdm_config,
      const base::WeakPtr<WebEncryptedMediaClientImpl>& client);
  ~WebContentDecryptionModuleAccessImpl() OVERRIDE;

  // blink::WebContentDecryptionModuleAccess interface.
  blink::WebMediaKeySystemConfiguration getConfiguration() OVERRIDE;
  void createContentDecryptionModule(
      blink::WebContentDecryptionModuleResult result) OVERRIDE;

 private:
  WebContentDecryptionModuleAccessImpl(
      const blink::WebString& key_system,
      const blink::WebSecurityOrigin& security_origin,
      const blink::WebMediaKeySystemConfiguration& configuration,
      const CdmConfig& cdm_config,
      const base::WeakPtr<WebEncryptedMediaClientImpl>& client);

  const blink::WebString key_system_;
  const blink::WebSecurityOrigin security_origin_;
  const blink::WebMediaKeySystemConfiguration configuration_;
  const CdmConfig cdm_config_;

  // Keep a WeakPtr as client is owned by render_frame_impl.
  base::WeakPtr<WebEncryptedMediaClientImpl> client_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleAccessImpl);
};

}  // namespace media

#endif  // COBALT_MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_
