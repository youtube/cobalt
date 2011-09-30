// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_PARSER_H_
#define MEDIA_WEBM_WEBM_PARSER_H_

#include <string>

#include "base/basictypes.h"

namespace media {

// Interface for receiving WebM parser events.
//
// Each method is called when an element of the specified type is parsed.
// The ID of the element that was parsed is given along with the value
// stored in the element. List elements generate calls at the start and
// end of the list. Any pointers passed to these methods are only guaranteed
// to be valid for the life of that call. Each method returns a bool that
// indicates whether the parsed data is valid. If false is returned
// then the parse is immediately terminated and an error is reported by the
// parser.
class WebMParserClient {
 public:
  virtual ~WebMParserClient();

  virtual bool OnListStart(int id) = 0;
  virtual bool OnListEnd(int id) = 0;
  virtual bool OnUInt(int id, int64 val) = 0;
  virtual bool OnFloat(int id, double val) = 0;
  virtual bool OnBinary(int id, const uint8* data, int size) = 0;
  virtual bool OnString(int id, const std::string& str) = 0;
  virtual bool OnSimpleBlock(int track_num, int timecode,
                             int flags,
                             const uint8* data, int size) = 0;
};

// Parses a single list element that matches |id|. This method fails if the
// buffer points to an element that does not match |id|.
//
// Returns -1 if the parse fails.
// Returns 0 if more data is needed.
// Returns the number of bytes parsed on success.
int WebMParseListElement(const uint8* buf, int size, int id,
                         int level, WebMParserClient* client);

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_PARSER_H_
