// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dial requires some system information exposed to the world, for example
// the device name, etc. This class lets it be configurable, but throw default
// values from implementation.

#include "net/dial/dial_system_config.h"

#include <openssl/evp.h>

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_string_util.h"

#if defined(STARBOARD)
#include "starboard/client_porting/poem/stdio_poem.h"
#include "starboard/string.h"
#include "starboard/types.h"
#endif

namespace net {

namespace {
const char* kSecret = "v=8FpigqfcvlM";
char s_dial_uuid[23] = {};
}  // namespace

DialSystemConfig* DialSystemConfig::GetInstance() {
  return base::Singleton<DialSystemConfig>::get();
}

DialSystemConfig::DialSystemConfig()
    : friendly_name_(GetFriendlyName()),
      manufacturer_name_(GetManufacturerName()),
      model_name_(GetModelName()) {}

const char* DialSystemConfig::model_uuid() const {
  base::AutoLock lock(lock_);
  if (!strlen(s_dial_uuid)) {
    CreateDialUuid();
  }
  return s_dial_uuid;
}

// static
void DialSystemConfig::CreateDialUuid() {
  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  std::string platform_uuid = GeneratePlatformUuid();
  DCHECK_NE(0, platform_uuid.size());

  EVP_MD_CTX* mdctx;
  mdctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL);
  EVP_DigestUpdate(mdctx, kSecret, strlen(kSecret));
  EVP_DigestUpdate(mdctx, platform_uuid.data(), platform_uuid.size());
  EVP_DigestFinal_ex(mdctx, md_value, &md_len);
  EVP_MD_CTX_destroy(mdctx);

  // Now format the uuid as xxxxxxxx-yyyy-zzzzzzzz.
  // SHA-1 has a digest of 160 bits = 20bytes.
  // For full representation we need 40 hex chars, but we reduce
  // it down to 20 hex chars and then print it out.
  DCHECK_EQ(20, md_len);
  for (unsigned int i = 0; i < md_len / 2; ++i) {
    md_value[i] ^= md_value[i + md_len / 2];
  }

  snprintf(s_dial_uuid, sizeof(s_dial_uuid),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x%02x%02x", md_value[0],
           md_value[1], md_value[2], md_value[3], md_value[4], md_value[5],
           md_value[6], md_value[7], md_value[8], md_value[9]);

  DCHECK_EQ(22, strlen(s_dial_uuid));
}

}  // namespace net
