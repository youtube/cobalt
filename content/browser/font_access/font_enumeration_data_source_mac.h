// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_MAC_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_MAC_H_

#include <string>

#include "base/sequence_checker.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// Mac implementation of FontEnumerationDataSource.
class FontEnumerationDataSourceMac : public FontEnumerationDataSource {
 public:
  FontEnumerationDataSourceMac();

  FontEnumerationDataSourceMac(const FontEnumerationDataSourceMac&) = delete;
  FontEnumerationDataSourceMac& operator=(const FontEnumerationDataSourceMac&) =
      delete;

  ~FontEnumerationDataSourceMac() override;

  // FontEnumerationDataSource:
  blink::FontEnumerationTable GetFonts(const std::string& locale) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_MAC_H_
