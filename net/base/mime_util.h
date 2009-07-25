// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MIME_UTIL_H__
#define NET_BASE_MIME_UTIL_H__

#include <string>
#include <vector>

#include "base/file_path.h"

namespace net {

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists.
bool GetMimeTypeFromExtension(const FilePath::StringType& ext,
                              std::string* mime_type);

// Get the mime type (if any) that is associated with the given file.  Returns
// true if a corresponding mime type exists.
bool GetMimeTypeFromFile(const FilePath& file_path, std::string* mime_type);

// Get the preferred extension (if any) associated with the given mime type.
// Returns true if a corresponding file extension exists.  The extension is
// returned without a prefixed dot, ex "html".
bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      FilePath::StringType* extension);

// Check to see if a particular MIME type is in our list.
bool IsSupportedImageMimeType(const char* mime_type);
bool IsSupportedMediaMimeType(const char* mime_type);
bool IsSupportedNonImageMimeType(const char* mime_type);
bool IsSupportedJavascriptMimeType(const char* mime_type);

// Get whether this mime type should be displayed in view-source mode.
// (For example, XML.)
bool IsViewSourceMimeType(const char* mime_type);

// Convenience function.
bool IsSupportedMimeType(const std::string& mime_type);

// Returns true if this the mime_type_pattern matches a given mime-type.
// Checks for absolute matching and wildcards.  mime-types should be in
// lower case.
bool MatchesMimeType(const std::string &mime_type_pattern,
                     const std::string &mime_type);

// Returns true if and only if all codecs are supported, false otherwise.
bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs);

// Parses a codec string, populating |codecs_out| with the prefix of each codec
// in the string |codecs_in|. For example, passed "aaa.b.c,dd.eee", |codecs_out|
// will contain {"aaa", "dd"}.
// See http://www.ietf.org/rfc/rfc4281.txt.
void ParseCodecString(const std::string& codecs,
                      std::vector<std::string>* codecs_out);

}  // namespace net

#endif  // NET_BASE_MIME_UTIL_H__
