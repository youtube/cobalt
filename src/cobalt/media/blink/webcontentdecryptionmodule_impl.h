// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
#define COBALT_MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULE_IMPL_H_

#include <memory>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "cobalt/media/blink/media_blink_export.h"
#include "starboard/types.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"

namespace blink {
#if defined(ENABLE_PEPPER_CDMS)
class WebLocalFrame;
#endif
class WebSecurityOrigin;
}

namespace cobalt {
namespace media {

struct CdmConfig;
class CdmFactory;
class CdmSessionAdapter;
class MediaKeys;
class WebContentDecryptionModuleSessionImpl;

class MEDIA_BLINK_EXPORT WebContentDecryptionModuleImpl
    : public blink::WebContentDecryptionModule {
 public:
  static void Create(
      CdmFactory* cdm_factory, const base::string16& key_system,
      const blink::WebSecurityOrigin& security_origin,
      const CdmConfig& cdm_config,
      std::unique_ptr<blink::WebContentDecryptionModuleResult> result);

  ~WebContentDecryptionModuleImpl() OVERRIDE;

  // blink::WebContentDecryptionModule implementation.
  blink::WebContentDecryptionModuleSession* createSession() OVERRIDE;

  void setServerCertificate(
      const uint8_t* server_certificate, size_t server_certificate_length,
      blink::WebContentDecryptionModuleResult result) OVERRIDE;

  // Returns a reference to the CDM used by |adapter_|.
  scoped_refptr<MediaKeys> GetCdm();

 private:
  friend class CdmSessionAdapter;

  // Takes reference to |adapter|.
  explicit WebContentDecryptionModuleImpl(
      scoped_refptr<CdmSessionAdapter> adapter);

  scoped_refptr<CdmSessionAdapter> adapter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleImpl);
};

// Allow typecasting from blink type as this is the only implementation.
inline WebContentDecryptionModuleImpl* ToWebContentDecryptionModuleImpl(
    blink::WebContentDecryptionModule* cdm) {
  return static_cast<WebContentDecryptionModuleImpl*>(cdm);
}

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
