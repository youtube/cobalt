// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/fake_media_resources.h"

#include "base/utf_string_conversions.h"
#include "cobalt/media/base/media_resources.h"

namespace cobalt {
namespace media {

base::string16 FakeLocalizedStringProvider(MessageId message_id) {
  if (message_id == DEFAULT_AUDIO_DEVICE_NAME)
    return base::ASCIIToUTF16("Default");

  return base::ASCIIToUTF16("FakeString");
}

void SetUpFakeMediaResources() {
  SetLocalizedStringProvider(FakeLocalizedStringProvider);
}

}  // namespace media
}  // namespace cobalt
