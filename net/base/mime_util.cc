// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "net/base/mime_util.h"
#include "net/base/platform_mime_util.h"

#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

using std::string;

namespace {

struct MediaType {
  const char name[12];
  const char matcher[13];
};

static const MediaType kIanaMediaTypes[] = {
  { "application", "application/" },
  { "audio", "audio/" },
  { "example", "example/" },
  { "image", "image/" },
  { "message", "message/" },
  { "model", "model/" },
  { "multipart", "multipart/" },
  { "text", "text/" },
  { "video", "video/" },
};

}  // namespace

namespace net {

// Singleton utility class for mime types.
class MimeUtil : public PlatformMimeUtil {
 public:
  bool GetMimeTypeFromExtension(const FilePath::StringType& ext,
                                std::string* mime_type) const;

  bool GetMimeTypeFromFile(const FilePath& file_path,
                           std::string* mime_type) const;

  bool GetWellKnownMimeTypeFromExtension(const FilePath::StringType& ext,
                                         std::string* mime_type) const;

  bool IsSupportedImageMimeType(const std::string& mime_type) const;
  bool IsSupportedMediaMimeType(const std::string& mime_type) const;
  bool IsSupportedNonImageMimeType(const std::string& mime_type) const;
  bool IsUnsupportedTextMimeType(const std::string& mime_type) const;
  bool IsSupportedJavascriptMimeType(const std::string& mime_type) const;

  bool IsViewSourceMimeType(const std::string& mime_type) const;

  bool IsSupportedMimeType(const std::string& mime_type) const;

  bool MatchesMimeType(const std::string &mime_type_pattern,
                       const std::string &mime_type) const;

  bool IsMimeType(const std::string& type_string) const;

  bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs) const;

  void ParseCodecString(const std::string& codecs,
                        std::vector<std::string>* codecs_out,
                        bool strip);

  bool IsStrictMediaMimeType(const std::string& mime_type) const;
  bool IsSupportedStrictMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs) const;

 private:
  friend struct base::DefaultLazyInstanceTraits<MimeUtil>;

  typedef base::hash_set<std::string> MimeMappings;
  typedef std::map<std::string, MimeMappings> StrictMappings;

  MimeUtil();

  // Returns true if |codecs| is nonempty and all the items in it are present in
  // |supported_codecs|.
  static bool AreSupportedCodecs(const MimeMappings& supported_codecs,
                                 const std::vector<std::string>& codecs);

  // For faster lookup, keep hash sets.
  void InitializeMimeTypeMaps();

  bool GetMimeTypeFromExtensionHelper(const FilePath::StringType& ext,
                                      bool include_platform_types,
                                      std::string* mime_type) const;

  MimeMappings image_map_;
  MimeMappings media_map_;
  MimeMappings non_image_map_;
  MimeMappings unsupported_text_map_;
  MimeMappings javascript_map_;
  MimeMappings view_source_map_;
  MimeMappings codecs_map_;

  StrictMappings strict_format_map_;
};  // class MimeUtil

// This variable is Leaky because we need to access it from WorkerPool threads.
static base::LazyInstance<MimeUtil>::Leaky g_mime_util =
    LAZY_INSTANCE_INITIALIZER;

struct MimeInfo {
  const char* mime_type;
  const char* extensions;  // comma separated list
};

static const MimeInfo primary_mappings[] = {
  { "text/html", "html,htm" },
  { "text/css", "css" },
  { "text/xml", "xml" },
  { "image/gif", "gif" },
  { "image/jpeg", "jpeg,jpg" },
  { "image/webp", "webp" },
  { "image/png", "png" },
  { "video/mp4", "mp4,m4v" },
  { "audio/x-m4a", "m4a" },
  { "audio/mp3", "mp3" },
  { "video/ogg", "ogv,ogm" },
  { "audio/ogg", "ogg,oga" },
  { "video/webm", "webm" },
  { "audio/webm", "webm" },
  { "audio/wav", "wav" },
  { "application/xhtml+xml", "xhtml,xht" },
  { "application/x-chrome-extension", "crx" },
  { "multipart/related", "mhtml,mht" }
};

static const MimeInfo secondary_mappings[] = {
  { "application/octet-stream", "exe,com,bin" },
  { "application/gzip", "gz" },
  { "application/pdf", "pdf" },
  { "application/postscript", "ps,eps,ai" },
  { "application/x-javascript", "js" },
  { "application/x-font-woff", "woff" },
  { "image/bmp", "bmp" },
  { "image/x-icon", "ico" },
  { "image/jpeg", "jfif,pjpeg,pjp" },
  { "image/tiff", "tiff,tif" },
  { "image/x-xbitmap", "xbm" },
  { "image/svg+xml", "svg,svgz" },
  { "message/rfc822", "eml" },
  { "text/plain", "txt,text" },
  { "text/html", "shtml,ehtml" },
  { "application/rss+xml", "rss" },
  { "application/rdf+xml", "rdf" },
  { "text/xml", "xsl,xbl" },
  { "application/vnd.mozilla.xul+xml", "xul" },
  { "application/x-shockwave-flash", "swf,swl" },
  { "application/pkcs7-mime", "p7m,p7c,p7z" },
  { "application/pkcs7-signature", "p7s" }
};

static const char* FindMimeType(const MimeInfo* mappings,
                                size_t mappings_len,
                                const char* ext) {
  size_t ext_len = strlen(ext);

  for (size_t i = 0; i < mappings_len; ++i) {
    const char* extensions = mappings[i].extensions;
    for (;;) {
      size_t end_pos = strcspn(extensions, ",");
      if (end_pos == ext_len &&
          base::strncasecmp(extensions, ext, ext_len) == 0)
        return mappings[i].mime_type;
      extensions += end_pos;
      if (!*extensions)
        break;
      extensions += 1;  // skip over comma
    }
  }
  return NULL;
}

bool MimeUtil::GetMimeTypeFromExtension(const FilePath::StringType& ext,
                                        string* result) const {
  return GetMimeTypeFromExtensionHelper(ext, true, result);
}

bool MimeUtil::GetWellKnownMimeTypeFromExtension(
    const FilePath::StringType& ext,
    string* result) const {
  return GetMimeTypeFromExtensionHelper(ext, false, result);
}

bool MimeUtil::GetMimeTypeFromFile(const FilePath& file_path,
                                   string* result) const {
  FilePath::StringType file_name_str = file_path.Extension();
  if (file_name_str.empty())
    return false;
  return GetMimeTypeFromExtension(file_name_str.substr(1), result);
}

bool MimeUtil::GetMimeTypeFromExtensionHelper(const FilePath::StringType& ext,
                                              bool include_platform_types,
                                              string* result) const {
  // Avoids crash when unable to handle a long file path. See crbug.com/48733.
  const unsigned kMaxFilePathSize = 65536;
  if (ext.length() > kMaxFilePathSize)
    return false;

  // We implement the same algorithm as Mozilla for mapping a file extension to
  // a mime type.  That is, we first check a hard-coded list (that cannot be
  // overridden), and then if not found there, we defer to the system registry.
  // Finally, we scan a secondary hard-coded list to catch types that we can
  // deduce but that we also want to allow the OS to override.

#if defined(OS_WIN)
  string ext_narrow_str = WideToUTF8(ext);
#elif defined(OS_POSIX)
  const string& ext_narrow_str = ext;
#endif
  const char* mime_type;

  mime_type = FindMimeType(primary_mappings, arraysize(primary_mappings),
                           ext_narrow_str.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  if (include_platform_types && GetPlatformMimeTypeFromExtension(ext, result))
    return true;

  mime_type = FindMimeType(secondary_mappings, arraysize(secondary_mappings),
                           ext_narrow_str.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  return false;
}

// From WebKit's WebCore/platform/MIMETypeRegistry.cpp:

static const char* const supported_image_types[] = {
  "image/jpeg",
  "image/pjpeg",
  "image/jpg",
  "image/webp",
  "image/png",
  "image/gif",
  "image/bmp",
  "image/x-icon",    // ico
  "image/x-xbitmap"  // xbm
};

// A list of media types: http://en.wikipedia.org/wiki/Internet_media_type
// A comprehensive mime type list: http://plugindoc.mozdev.org/winmime.php
// This set of codecs is supported by all variations of Chromium.
static const char* const common_media_types[] = {
  // Ogg.
  "audio/ogg",
  "application/ogg",
#if defined(ENABLE_MEDIA_CODEC_THEORA)
  "video/ogg",
#endif

  // WebM.
  "video/webm",
  "audio/webm",

  // Wav.
  "audio/wav",
  "audio/x-wav",
};

// List of proprietary types only supported by Google Chrome.
static const char* const proprietary_media_types[] = {
  // MPEG-4.
  "video/mp4",
  "video/x-m4v",
  "audio/mp4",
  "audio/x-m4a",

  // MP3.
  "audio/mp3",
  "audio/x-mp3",
  "audio/mpeg",
};

// List of supported codecs when passed in with <source type="...">.
// This set of codecs is supported by all variations of Chromium.
//
// Refer to http://wiki.whatwg.org/wiki/Video_type_parameters#Browser_Support
// for more information.
//
// The codecs for WAV are integers as defined in Appendix A of RFC2361:
// http://tools.ietf.org/html/rfc2361
static const char* const common_media_codecs[] = {
#if defined(ENABLE_MEDIA_CODEC_THEORA)
  "theora",
#endif
  "vorbis",
  "vp8",
  "1"  // WAVE_FORMAT_PCM.
};

// List of proprietary codecs only supported by Google Chrome.
static const char* const proprietary_media_codecs[] = {
  "avc1",
  "mp4a"
};

// Note: does not include javascript types list (see supported_javascript_types)
static const char* const supported_non_image_types[] = {
  "text/cache-manifest",
  "text/html",
  "text/xml",
  "text/xsl",
  "text/plain",
  // Many users complained about css files served for
  // download instead of displaying in the browser:
  // http://code.google.com/p/chromium/issues/detail?id=7192
  // So, by including "text/css" into this list we choose Firefox
  // behavior - css files will be displayed:
  "text/css",
  "text/vnd.chromium.ftp-dir",
  "text/",
  "image/svg+xml",  // SVG is text-based XML, even though it has an image/ type
  "application/xml",
  "application/xhtml+xml",
  "application/json",
  "application/x-x509-user-cert",
  "multipart/related",  // For MHTML support.
  "multipart/x-mixed-replace"
  // Note: ADDING a new type here will probably render it AS HTML. This can
  // result in cross site scripting.
};

// These types are excluded from the logic that allows all text/ types because
// while they are technically text, it's very unlikely that a user expects to
// see them rendered in text form.
static const char* const unsupported_text_types[] = {
  "text/calendar",
  "text/x-calendar",
  "text/x-vcalendar",
  "text/vcalendar",
  "text/vcard",
  "text/x-vcard",
  "text/directory",
  "text/ldif",
  "text/qif",
  "text/x-qif",
  "text/x-csv",
  "text/x-vcf",
  "text/rtf",
};

//  Mozilla 1.8 and WinIE 7 both accept text/javascript and text/ecmascript.
//  Mozilla 1.8 accepts application/javascript, application/ecmascript, and
// application/x-javascript, but WinIE 7 doesn't.
//  WinIE 7 accepts text/javascript1.1 - text/javascript1.3, text/jscript, and
// text/livescript, but Mozilla 1.8 doesn't.
//  Mozilla 1.8 allows leading and trailing whitespace, but WinIE 7 doesn't.
//  Mozilla 1.8 and WinIE 7 both accept the empty string, but neither accept a
// whitespace-only string.
//  We want to accept all the values that either of these browsers accept, but
// not other values.
static const char* const supported_javascript_types[] = {
  "text/javascript",
  "text/ecmascript",
  "application/javascript",
  "application/ecmascript",
  "application/x-javascript",
  "text/javascript1.1",
  "text/javascript1.2",
  "text/javascript1.3",
  "text/jscript",
  "text/livescript"
};

static const char* const view_source_types[] = {
  "text/xml",
  "text/xsl",
  "application/xml",
  "application/rss+xml",
  "application/atom+xml",
  "image/svg+xml"
};

struct MediaFormatStrict {
  const char* mime_type;
  const char* codecs_list;
};

static const MediaFormatStrict format_codec_mappings[] = {
  { "video/webm", "vorbis,vp8,vp8.0" },
  { "audio/webm", "vorbis" },
  { "audio/wav", "1" }
};

MimeUtil::MimeUtil() {
  InitializeMimeTypeMaps();
}

// static
bool MimeUtil::AreSupportedCodecs(const MimeMappings& supported_codecs,
                                  const std::vector<std::string>& codecs) {
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (supported_codecs.find(codecs[i]) == supported_codecs.end())
      return false;
  }
  return !codecs.empty();
}

void MimeUtil::InitializeMimeTypeMaps() {
  for (size_t i = 0; i < arraysize(supported_image_types); ++i)
    image_map_.insert(supported_image_types[i]);

  // Initialize the supported non-image types.
  for (size_t i = 0; i < arraysize(supported_non_image_types); ++i)
    non_image_map_.insert(supported_non_image_types[i]);
  for (size_t i = 0; i < arraysize(unsupported_text_types); ++i)
    unsupported_text_map_.insert(unsupported_text_types[i]);
  for (size_t i = 0; i < arraysize(supported_javascript_types); ++i)
    non_image_map_.insert(supported_javascript_types[i]);
  for (size_t i = 0; i < arraysize(common_media_types); ++i)
    non_image_map_.insert(common_media_types[i]);
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  for (size_t i = 0; i < arraysize(proprietary_media_types); ++i)
    non_image_map_.insert(proprietary_media_types[i]);
#endif

  // Initialize the supported media types.
  for (size_t i = 0; i < arraysize(common_media_types); ++i)
    media_map_.insert(common_media_types[i]);
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  for (size_t i = 0; i < arraysize(proprietary_media_types); ++i)
    media_map_.insert(proprietary_media_types[i]);
#endif

  for (size_t i = 0; i < arraysize(supported_javascript_types); ++i)
    javascript_map_.insert(supported_javascript_types[i]);

  for (size_t i = 0; i < arraysize(view_source_types); ++i)
    view_source_map_.insert(view_source_types[i]);

  for (size_t i = 0; i < arraysize(common_media_codecs); ++i)
    codecs_map_.insert(common_media_codecs[i]);
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  for (size_t i = 0; i < arraysize(proprietary_media_codecs); ++i)
    codecs_map_.insert(proprietary_media_codecs[i]);
#endif

  // Initialize the strict supported media types.
  for (size_t i = 0; i < arraysize(format_codec_mappings); ++i) {
    std::vector<std::string> mime_type_codecs;
    ParseCodecString(format_codec_mappings[i].codecs_list,
                     &mime_type_codecs,
                     false);

    MimeMappings codecs;
    for (size_t j = 0; j < mime_type_codecs.size(); ++j)
      codecs.insert(mime_type_codecs[j]);
    strict_format_map_[format_codec_mappings[i].mime_type] = codecs;
  }
}

bool MimeUtil::IsSupportedImageMimeType(const std::string& mime_type) const {
  return image_map_.find(mime_type) != image_map_.end();
}

bool MimeUtil::IsSupportedMediaMimeType(const std::string& mime_type) const {
  return media_map_.find(mime_type) != media_map_.end();
}

bool MimeUtil::IsSupportedNonImageMimeType(const std::string& mime_type) const {
  return non_image_map_.find(mime_type) != non_image_map_.end() ||
      (mime_type.compare(0, 5, "text/") == 0 &&
       !IsUnsupportedTextMimeType(mime_type));
}

bool MimeUtil::IsUnsupportedTextMimeType(const std::string& mime_type) const {
  return unsupported_text_map_.find(mime_type) != unsupported_text_map_.end();
}

bool MimeUtil::IsSupportedJavascriptMimeType(
    const std::string& mime_type) const {
  return javascript_map_.find(mime_type) != javascript_map_.end();
}

bool MimeUtil::IsViewSourceMimeType(const std::string& mime_type) const {
  return view_source_map_.find(mime_type) != view_source_map_.end();
}

// Mirrors WebViewImpl::CanShowMIMEType()
bool MimeUtil::IsSupportedMimeType(const std::string& mime_type) const {
  return (mime_type.compare(0, 6, "image/") == 0 &&
          IsSupportedImageMimeType(mime_type)) ||
         IsSupportedNonImageMimeType(mime_type);
}

bool MimeUtil::MatchesMimeType(const std::string& mime_type_pattern,
                               const std::string& mime_type) const {
  // verify caller is passing lowercase
  DCHECK_EQ(StringToLowerASCII(mime_type_pattern), mime_type_pattern);
  DCHECK_EQ(StringToLowerASCII(mime_type), mime_type);

  // This comparison handles absolute maching and also basic
  // wildcards.  The plugin mime types could be:
  //      application/x-foo
  //      application/*
  //      application/*+xml
  //      *
  if (mime_type_pattern.empty())
    return false;

  const std::string::size_type star = mime_type_pattern.find('*');

  if (star == std::string::npos)
    return mime_type_pattern == mime_type;

  // Test length to prevent overlap between |left| and |right|.
  if (mime_type.length() < mime_type_pattern.length() - 1)
    return false;

  const std::string left(mime_type_pattern.substr(0, star));
  const std::string right(mime_type_pattern.substr(star + 1));

  if (mime_type.find(left) != 0)
    return false;

  if (!right.empty() &&
      mime_type.rfind(right) != mime_type.length() - right.length())
    return false;

  return true;
}

// See http://www.iana.org/assignments/media-types/index.html
static const char* legal_top_level_types[] = {
  "application/",
  "audio/",
  "example/",
  "image/",
  "message/",
  "model/",
  "multipart/",
  "text/",
  "video/",
};

bool MimeUtil::IsMimeType(const std::string& type_string) const {
  // MIME types are always ASCII and case-insensitive (at least, the top-level
  // and secondary types we care about).
  if (!IsStringASCII(type_string))
    return false;

  if (type_string == "*/*" || type_string == "*")
    return true;

  for (size_t i = 0; i < arraysize(legal_top_level_types); ++i) {
    if (StartsWithASCII(type_string, legal_top_level_types[i], false) &&
        type_string.length() > strlen(legal_top_level_types[i])) {
      return true;
    }
  }

  // If there's a "/" separator character, and the token before it is
  // "x-" + (ascii characters), it is also a MIME type.
  size_t slash = type_string.find('/');
  if (slash < 3 ||
      slash == std::string::npos || slash == type_string.length() - 1) {
    return false;
  }

  if (StartsWithASCII(type_string, "x-", false))
    return true;

  return false;
}

bool MimeUtil::AreSupportedMediaCodecs(
    const std::vector<std::string>& codecs) const {
  return AreSupportedCodecs(codecs_map_, codecs);
}

void MimeUtil::ParseCodecString(const std::string& codecs,
                                std::vector<std::string>* codecs_out,
                                bool strip) {
  std::string no_quote_codecs;
  TrimString(codecs, "\"", &no_quote_codecs);
  base::SplitString(no_quote_codecs, ',', codecs_out);

  if (!strip)
    return;

  // Strip everything past the first '.'
  for (std::vector<std::string>::iterator it = codecs_out->begin();
       it != codecs_out->end();
       ++it) {
    size_t found = it->find_first_of('.');
    if (found != std::string::npos)
      it->resize(found);
  }
}

bool MimeUtil::IsStrictMediaMimeType(const std::string& mime_type) const {
  if (strict_format_map_.find(mime_type) == strict_format_map_.end())
    return false;
  return true;
}

bool MimeUtil::IsSupportedStrictMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs) const {
  StrictMappings::const_iterator it = strict_format_map_.find(mime_type);
  return (it != strict_format_map_.end()) &&
      AreSupportedCodecs(it->second, codecs);
}

//----------------------------------------------------------------------------
// Wrappers for the singleton
//----------------------------------------------------------------------------

bool GetMimeTypeFromExtension(const FilePath::StringType& ext,
                              std::string* mime_type) {
  return g_mime_util.Get().GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const FilePath& file_path, std::string* mime_type) {
  return g_mime_util.Get().GetMimeTypeFromFile(file_path, mime_type);
}

bool GetWellKnownMimeTypeFromExtension(const FilePath::StringType& ext,
                                       std::string* mime_type) {
  return g_mime_util.Get().GetWellKnownMimeTypeFromExtension(ext, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      FilePath::StringType* extension) {
  return g_mime_util.Get().GetPreferredExtensionForMimeType(mime_type,
                                                            extension);
}

bool IsSupportedImageMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedImageMimeType(mime_type);
}

bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedMediaMimeType(mime_type);
}

bool IsSupportedNonImageMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedNonImageMimeType(mime_type);
}

bool IsUnsupportedTextMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsUnsupportedTextMimeType(mime_type);
}

bool IsSupportedJavascriptMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedJavascriptMimeType(mime_type);
}

bool IsViewSourceMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsViewSourceMimeType(mime_type);
}

bool IsSupportedMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedMimeType(mime_type);
}

bool MatchesMimeType(const std::string& mime_type_pattern,
                     const std::string& mime_type) {
  return g_mime_util.Get().MatchesMimeType(mime_type_pattern, mime_type);
}

bool IsMimeType(const std::string& type_string) {
  return g_mime_util.Get().IsMimeType(type_string);
}

bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs) {
  return g_mime_util.Get().AreSupportedMediaCodecs(codecs);
}

bool IsStrictMediaMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsStrictMediaMimeType(mime_type);
}

bool IsSupportedStrictMediaMimeType(const std::string& mime_type,
                                    const std::vector<std::string>& codecs) {
  return g_mime_util.Get().IsSupportedStrictMediaMimeType(mime_type, codecs);
}

void ParseCodecString(const std::string& codecs,
                      std::vector<std::string>* codecs_out,
                      const bool strip) {
  g_mime_util.Get().ParseCodecString(codecs, codecs_out, strip);
}

namespace {

// From http://www.w3schools.com/media/media_mimeref.asp and
// http://plugindoc.mozdev.org/winmime.php
static const char* kStandardImageTypes[] = {
  "image/bmp",
  "image/cis-cod",
  "image/gif",
  "image/ief",
  "image/jpeg",
  "image/webp",
  "image/pict",
  "image/pipeg",
  "image/png",
  "image/svg+xml",
  "image/tiff",
  "image/x-cmu-raster",
  "image/x-cmx",
  "image/x-icon",
  "image/x-portable-anymap",
  "image/x-portable-bitmap",
  "image/x-portable-graymap",
  "image/x-portable-pixmap",
  "image/x-rgb",
  "image/x-xbitmap",
  "image/x-xpixmap",
  "image/x-xwindowdump"
};
static const char* kStandardAudioTypes[] = {
  "audio/aac",
  "audio/aiff",
  "audio/amr",
  "audio/basic",
  "audio/midi",
  "audio/mp3",
  "audio/mp4",
  "audio/mpeg",
  "audio/mpeg3",
  "audio/ogg",
  "audio/vorbis",
  "audio/wav",
  "audio/webm",
  "audio/x-m4a",
  "audio/x-ms-wma",
  "audio/vnd.rn-realaudio",
  "audio/vnd.wave"
};
static const char* kStandardVideoTypes[] = {
  "video/avi",
  "video/divx",
  "video/flc",
  "video/mp4",
  "video/mpeg",
  "video/ogg",
  "video/quicktime",
  "video/sd-video",
  "video/webm",
  "video/x-dv",
  "video/x-m4v",
  "video/x-mpeg",
  "video/x-ms-asf",
  "video/x-ms-wmv"
};

struct StandardType {
  const char* leading_mime_type;
  const char** standard_types;
  size_t standard_types_len;
};
static const StandardType kStandardTypes[] = {
  { "image/", kStandardImageTypes, arraysize(kStandardImageTypes) },
  { "audio/", kStandardAudioTypes, arraysize(kStandardAudioTypes) },
  { "video/", kStandardVideoTypes, arraysize(kStandardVideoTypes) },
  { NULL, NULL, 0 }
};

void GetExtensionsFromHardCodedMappings(
    const MimeInfo* mappings,
    size_t mappings_len,
    const std::string& leading_mime_type,
    base::hash_set<FilePath::StringType>* extensions) {
  FilePath::StringType extension;
  for (size_t i = 0; i < mappings_len; ++i) {
    if (StartsWithASCII(mappings[i].mime_type, leading_mime_type, false)) {
      std::vector<string> this_extensions;
      base::SplitStringUsingSubstr(mappings[i].extensions, ",",
                                   &this_extensions);
      for (size_t j = 0; j < this_extensions.size(); ++j) {
#if defined(OS_WIN)
        FilePath::StringType extension(UTF8ToWide(this_extensions[j]));
#else
        FilePath::StringType extension(this_extensions[j]);
#endif
        extensions->insert(extension);
      }
    }
  }
}

void GetExtensionsHelper(const char** standard_types,
                         size_t standard_types_len,
                         const std::string& leading_mime_type,
                         base::hash_set<FilePath::StringType>* extensions) {
  for (size_t i = 0; i < standard_types_len; ++i) {
    g_mime_util.Get().GetPlatformExtensionsForMimeType(standard_types[i],
                                                       extensions);
  }

  // Also look up the extensions from hard-coded mappings in case that some
  // supported extensions are not registered in the system registry, like ogg.
  GetExtensionsFromHardCodedMappings(primary_mappings,
                                     arraysize(primary_mappings),
                                     leading_mime_type,
                                     extensions);

  GetExtensionsFromHardCodedMappings(secondary_mappings,
                                     arraysize(secondary_mappings),
                                     leading_mime_type,
                                     extensions);
}

// Note that the elements in the source set will be appended to the target
// vector.
template<class T>
void HashSetToVector(base::hash_set<T>* source, std::vector<T>* target) {
  size_t old_target_size = target->size();
  target->resize(old_target_size + source->size());
  size_t i = 0;
  for (typename base::hash_set<T>::iterator iter = source->begin();
       iter != source->end(); ++iter, ++i)
    (*target)[old_target_size + i] = *iter;
}
}

void GetExtensionsForMimeType(const std::string& unsafe_mime_type,
                              std::vector<FilePath::StringType>* extensions) {
  if (unsafe_mime_type == "*/*" || unsafe_mime_type == "*")
    return;

  const std::string mime_type = StringToLowerASCII(unsafe_mime_type);
  base::hash_set<FilePath::StringType> unique_extensions;

  if (EndsWith(mime_type, "/*", true)) {
    std::string leading_mime_type = mime_type.substr(0, mime_type.length() - 2);

    // Find the matching StandardType from within kStandardTypes, or fall
    // through to the last (default) StandardType.
    const StandardType* type = NULL;
    for (size_t i = 0; i < arraysize(kStandardTypes); ++i) {
      type = &(kStandardTypes[i]);
      if (type->leading_mime_type &&
          leading_mime_type == type->leading_mime_type)
        break;
    }
    DCHECK(type);
    GetExtensionsHelper(type->standard_types,
                        type->standard_types_len,
                        leading_mime_type,
                        &unique_extensions);
  } else {
    g_mime_util.Get().GetPlatformExtensionsForMimeType(mime_type,
                                                       &unique_extensions);

    // Also look up the extensions from hard-coded mappings in case that some
    // supported extensions are not registered in the system registry, like ogg.
    GetExtensionsFromHardCodedMappings(primary_mappings,
                                       arraysize(primary_mappings),
                                       mime_type,
                                       &unique_extensions);

    GetExtensionsFromHardCodedMappings(secondary_mappings,
                                       arraysize(secondary_mappings),
                                       mime_type,
                                       &unique_extensions);
  }

  HashSetToVector(&unique_extensions, extensions);
}

void GetMediaTypesBlacklistedForTests(std::vector<std::string>* types) {
  types->clear();

// Unless/until WebM files are added to the media layout tests, we need to avoid
// blacklisting mp4 and H.264 when Theora is not supported (and proprietary
// codecs are) so that the media tests can still run.
#if defined(ENABLE_MEDIA_CODEC_THEORA) || !defined(USE_PROPRIETARY_CODECS)
  for (size_t i = 0; i < arraysize(proprietary_media_types); ++i)
    types->push_back(proprietary_media_types[i]);
#endif
}

void GetMediaCodecsBlacklistedForTests(std::vector<std::string>* codecs) {
  codecs->clear();

// Unless/until WebM files are added to the media layout tests, we need to avoid
// blacklisting mp4 and H.264 when Theora is not supported (and proprietary
// codecs are) so that the media tests can still run.
#if defined(ENABLE_MEDIA_CODEC_THEORA) || !defined(USE_PROPRIETARY_CODECS)
  for (size_t i = 0; i < arraysize(proprietary_media_codecs); ++i)
    codecs->push_back(proprietary_media_codecs[i]);
#endif
}

const std::string GetIANAMediaType(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kIanaMediaTypes); ++i) {
    if (StartsWithASCII(mime_type, kIanaMediaTypes[i].matcher, true)) {
      return kIanaMediaTypes[i].name;
    }
  }
  return "";
}

}  // namespace net
