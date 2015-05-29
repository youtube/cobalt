// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PLAYER_MIME_UTIL_H__
#define MEDIA_PLAYER_MIME_UTIL_H__

#include <string>
#include <vector>

#include "base/file_path.h"
#include "media/base/media_export.h"

namespace media {

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists.
MEDIA_EXPORT bool GetMimeTypeFromExtension(const FilePath::StringType& ext,
                                           std::string* mime_type);

// Get the mime type (if any) that is associated with the given file extension.
// Returns true if a corresponding mime type exists. In this method,
// the search for a mime type is constrained to a limited set of
// types known to the net library, the OS/registry is not consulted.
MEDIA_EXPORT bool GetWellKnownMimeTypeFromExtension(
    const FilePath::StringType& ext,
    std::string* mime_type);

// Get the mime type (if any) that is associated with the given file.  Returns
// true if a corresponding mime type exists.
MEDIA_EXPORT bool GetMimeTypeFromFile(const FilePath& file_path,
                                      std::string* mime_type);

// Get the preferred extension (if any) associated with the given mime type.
// Returns true if a corresponding file extension exists.  The extension is
// returned without a prefixed dot, ex "html".
MEDIA_EXPORT bool GetPreferredExtensionForMimeType(
    const std::string& mime_type,
    FilePath::StringType* extension);

// Check to see if a particular MIME type is in our list.
MEDIA_EXPORT bool IsSupportedImageMimeType(const std::string& mime_type);
MEDIA_EXPORT bool IsSupportedMediaMimeType(const std::string& mime_type);
MEDIA_EXPORT bool IsSupportedNonImageMimeType(const std::string& mime_type);
MEDIA_EXPORT bool IsUnsupportedTextMimeType(const std::string& mime_type);
MEDIA_EXPORT bool IsSupportedJavascriptMimeType(const std::string& mime_type);
MEDIA_EXPORT bool IsSupportedCertificateMimeType(const std::string& mime_type);

// Get whether this mime type should be displayed in view-source mode.
// (For example, XML.)
MEDIA_EXPORT bool IsViewSourceMimeType(const std::string& mime_type);

// Convenience function.
MEDIA_EXPORT bool IsSupportedMimeType(const std::string& mime_type);

// Returns true if this the mime_type_pattern matches a given mime-type.
// Checks for absolute matching and wildcards.  mime-types should be in
// lower case.
MEDIA_EXPORT bool MatchesMimeType(const std::string& mime_type_pattern,
                                  const std::string& mime_type);

// Returns true if the |type_string| is a correctly-formed mime type specifier.
// Allows strings of the form x/y[;params], where "x" is a legal mime type name.
// Also allows wildcard types -- "x/*", "*/*", and "*".
MEDIA_EXPORT bool IsMimeType(const std::string& type_string);

// Returns true if and only if all codecs are supported, false otherwise.
MEDIA_EXPORT bool AreSupportedMediaCodecs(
    const std::vector<std::string>& codecs);

// Parses a codec string, populating |codecs_out| with the prefix of each codec
// in the string |codecs_in|. For example, passed "aaa.b.c,dd.eee", if
// |strip| == true |codecs_out| will contain {"aaa", "dd"}, if |strip| == false
// |codecs_out| will contain {"aaa.b.c", "dd.eee"}.
// See http://www.ietf.org/rfc/rfc4281.txt.
MEDIA_EXPORT void ParseCodecString(const std::string& codecs,
                                   std::vector<std::string>* codecs_out,
                                   bool strip);

// Check to see if a particular MIME type is in our list which only supports a
// certain subset of codecs.
MEDIA_EXPORT bool IsStrictMediaMimeType(const std::string& mime_type);

// Check to see if a particular MIME type is in our list which only supports a
// certain subset of codecs. Returns true if and only if all codecs are
// supported for that specific MIME type, false otherwise. If this returns
// false you will still need to check if the media MIME tpyes and codecs are
// supported.
MEDIA_EXPORT bool IsSupportedStrictMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs);

// Get the extensions associated with the given mime type. This should be passed
// in lower case. There could be multiple extensions for a given mime type, like
// "html,htm" for "text/html", or "txt,text,html,..." for "text/*".
// Note that we do not erase the existing elements in the the provided vector.
// Instead, we append the result to it.
MEDIA_EXPORT void GetExtensionsForMimeType(
    const std::string& mime_type,
    std::vector<FilePath::StringType>* extensions);

// Test only methods that return lists of proprietary media types and codecs
// that are not supported by all variations of Chromium.
// These types and codecs must be blacklisted to ensure consistent layout test
// results across all Chromium variations.
MEDIA_EXPORT void GetMediaTypesBlacklistedForTests(
    std::vector<std::string>* types);
MEDIA_EXPORT void GetMediaCodecsBlacklistedForTests(
    std::vector<std::string>* codecs);

// Returns the IANA media type contained in |mime_type|, or an empty
// string if |mime_type| does not specifify a known media type.
// Supported media types are defined at:
// http://www.iana.org/assignments/media-types/index.html
MEDIA_EXPORT const std::string GetIANAMediaType(const std::string& mime_type);

// A list of supported certificate-related mime types.
enum CertificateMimeType {
#define CERTIFICATE_MIME_TYPE(name, value) CERTIFICATE_MIME_TYPE_ ## name = value,
#include "media/player/mime_util_certificate_type_list.h"
#undef CERTIFICATE_MIME_TYPE
};

MEDIA_EXPORT CertificateMimeType GetCertificateMimeTypeForMimeType(
    const std::string& mime_type);

}  // namespace media

#endif  // MEDIA_PLAYER_MIME_UTIL_H__
