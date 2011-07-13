// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace net {

// Singleton utility class for mime types.
class MimeUtil : public PlatformMimeUtil {
 public:
  bool GetMimeTypeFromExtension(const FilePath::StringType& ext,
                                std::string* mime_type) const;

  bool GetMimeTypeFromFile(const FilePath& file_path,
                           std::string* mime_type) const;

  bool IsSupportedImageMimeType(const char* mime_type) const;
  bool IsSupportedMediaMimeType(const char* mime_type) const;
  bool IsSupportedNonImageMimeType(const char* mime_type) const;
  bool IsSupportedJavascriptMimeType(const char* mime_type) const;

  bool IsViewSourceMimeType(const char* mime_type) const;

  bool IsSupportedMimeType(const std::string& mime_type) const;

  bool MatchesMimeType(const std::string &mime_type_pattern,
                       const std::string &mime_type) const;

  bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs) const;

  void ParseCodecString(const std::string& codecs,
                        std::vector<std::string>* codecs_out,
                        bool strip);

  bool IsStrictMediaMimeType(const std::string& mime_type) const;
  bool IsSupportedStrictMediaMimeType(const std::string& mime_type,
      const std::vector<std::string>& codecs) const;

 private:
  friend struct base::DefaultLazyInstanceTraits<MimeUtil>;
  MimeUtil() {
    InitializeMimeTypeMaps();
  }

  // For faster lookup, keep hash sets.
  void InitializeMimeTypeMaps();

  typedef base::hash_set<std::string> MimeMappings;
  MimeMappings image_map_;
  MimeMappings media_map_;
  MimeMappings non_image_map_;
  MimeMappings javascript_map_;
  MimeMappings view_source_map_;
  MimeMappings codecs_map_;

  typedef std::map<std::string, base::hash_set<std::string> > StrictMappings;
  StrictMappings strict_format_map_;
};  // class MimeUtil

static base::LazyInstance<MimeUtil> g_mime_util(base::LINKER_INITIALIZED);

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
  { "application/x-chrome-extension", "crx" }
};

static const MimeInfo secondary_mappings[] = {
  { "application/octet-stream", "exe,com,bin" },
  { "application/gzip", "gz" },
  { "application/pdf", "pdf" },
  { "application/postscript", "ps,eps,ai" },
  { "application/x-javascript", "js" },
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
  { "application/x-shockwave-flash", "swf,swl" }
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

  if (GetPlatformMimeTypeFromExtension(ext, result))
    return true;

  mime_type = FindMimeType(secondary_mappings, arraysize(secondary_mappings),
                           ext_narrow_str.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  return false;
}

bool MimeUtil::GetMimeTypeFromFile(const FilePath& file_path,
                                   string* result) const {
  FilePath::StringType file_name_str = file_path.Extension();
  if (file_name_str.empty())
    return false;
  return GetMimeTypeFromExtension(file_name_str.substr(1), result);
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
static const char* const supported_media_types[] = {
  // Ogg.
  "video/ogg",
  "audio/ogg",
  "application/ogg",
  "video/webm",
  "audio/webm",
  "audio/wav",
  "audio/x-wav",

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  // MPEG-4.
  "video/mp4",
  "video/x-m4v",
  "audio/mp4",
  "audio/x-m4a",

  // MP3.
  "audio/mp3",
  "audio/x-mp3",
  "audio/mpeg",
#endif
};

// List of supported codecs when passed in with <source type="...">.
//
// Refer to http://wiki.whatwg.org/wiki/Video_type_parameters#Browser_Support
// for more information.
static const char* const supported_media_codecs[] = {
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  "avc1",
  "mp4a",
#endif
  "theora",
  "vorbis",
  "vp8",
  "1"  // PCM for WAV.
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
  "application/rss+xml",
  "application/atom+xml",
  "application/json",
  "application/x-x509-user-cert",
  "multipart/x-mixed-replace"
  // Note: ADDING a new type here will probably render it AS HTML. This can
  // result in cross site scripting.
};
COMPILE_ASSERT(arraysize(supported_non_image_types) == 16,
               supported_non_images_types_must_equal_16);

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

void MimeUtil::InitializeMimeTypeMaps() {
  for (size_t i = 0; i < arraysize(supported_image_types); ++i)
    image_map_.insert(supported_image_types[i]);

  // Initialize the supported non-image types.
  for (size_t i = 0; i < arraysize(supported_non_image_types); ++i)
    non_image_map_.insert(supported_non_image_types[i]);
  for (size_t i = 0; i < arraysize(supported_javascript_types); ++i)
    non_image_map_.insert(supported_javascript_types[i]);
  for (size_t i = 0; i < arraysize(supported_media_types); ++i)
    non_image_map_.insert(supported_media_types[i]);

  // Initialize the supported media types.
  for (size_t i = 0; i < arraysize(supported_media_types); ++i)
    media_map_.insert(supported_media_types[i]);

  for (size_t i = 0; i < arraysize(supported_javascript_types); ++i)
    javascript_map_.insert(supported_javascript_types[i]);

  for (size_t i = 0; i < arraysize(view_source_types); ++i)
    view_source_map_.insert(view_source_types[i]);

  for (size_t i = 0; i < arraysize(supported_media_codecs); ++i)
    codecs_map_.insert(supported_media_codecs[i]);

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

bool MimeUtil::IsSupportedImageMimeType(const char* mime_type) const {
  return image_map_.find(mime_type) != image_map_.end();
}

bool MimeUtil::IsSupportedMediaMimeType(const char* mime_type) const {
  return media_map_.find(mime_type) != media_map_.end();
}

bool MimeUtil::IsSupportedNonImageMimeType(const char* mime_type) const {
  return non_image_map_.find(mime_type) != non_image_map_.end();
}

bool MimeUtil::IsSupportedJavascriptMimeType(const char* mime_type) const {
  return javascript_map_.find(mime_type) != javascript_map_.end();
}

bool MimeUtil::IsViewSourceMimeType(const char* mime_type) const {
  return view_source_map_.find(mime_type) != view_source_map_.end();
}

// Mirrors WebViewImpl::CanShowMIMEType()
bool MimeUtil::IsSupportedMimeType(const std::string& mime_type) const {
  return (mime_type.compare(0, 6, "image/") == 0 &&
          IsSupportedImageMimeType(mime_type.c_str())) ||
         IsSupportedNonImageMimeType(mime_type.c_str());
}

bool MimeUtil::MatchesMimeType(const std::string &mime_type_pattern,
                               const std::string &mime_type) const {
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

bool MimeUtil::AreSupportedMediaCodecs(
    const std::vector<std::string>& codecs) const {
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs_map_.find(codecs[i]) == codecs_map_.end()) {
      return false;
    }
  }
  return true;
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

bool MimeUtil::IsSupportedStrictMediaMimeType(const std::string& mime_type,
    const std::vector<std::string>& codecs) const {
  StrictMappings::const_iterator it = strict_format_map_.find(mime_type);

  if (it == strict_format_map_.end())
    return false;

  const MimeMappings strict_codecs_map = it->second;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (strict_codecs_map.find(codecs[i]) == strict_codecs_map.end()) {
      return false;
    }
  }
  return true;
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

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      FilePath::StringType* extension) {
  return g_mime_util.Get().GetPreferredExtensionForMimeType(mime_type,
                                                            extension);
}

bool IsSupportedImageMimeType(const char* mime_type) {
  return g_mime_util.Get().IsSupportedImageMimeType(mime_type);
}

bool IsSupportedMediaMimeType(const char* mime_type) {
  return g_mime_util.Get().IsSupportedMediaMimeType(mime_type);
}

bool IsSupportedNonImageMimeType(const char* mime_type) {
  return g_mime_util.Get().IsSupportedNonImageMimeType(mime_type);
}

bool IsSupportedJavascriptMimeType(const char* mime_type) {
  return g_mime_util.Get().IsSupportedJavascriptMimeType(mime_type);
}

bool IsViewSourceMimeType(const char* mime_type) {
  return g_mime_util.Get().IsViewSourceMimeType(mime_type);
}

bool IsSupportedMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedMimeType(mime_type);
}

bool MatchesMimeType(const std::string &mime_type_pattern,
                     const std::string &mime_type) {
  return g_mime_util.Get().MatchesMimeType(mime_type_pattern, mime_type);
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

void GetExtensionsFromHardCodedMappings(
    const MimeInfo* mappings,
    size_t mappings_len,
    const std::string& leading_mime_type,
    base::hash_set<FilePath::StringType>* extensions) {
  FilePath::StringType extension;
  for (size_t i = 0; i < mappings_len; ++i) {
    if (StartsWithASCII(mappings[i].mime_type, leading_mime_type, false)) {
      std::vector<string> this_extensions;
      base::SplitStringUsingSubstr(mappings[i].extensions,
                                   ",",
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

void GetExtensionsHelper(
    const char** standard_types,
    size_t standard_types_len,
    const std::string& leading_mime_type,
    base::hash_set<FilePath::StringType>* extensions) {
  FilePath::StringType extension;
  for (size_t i = 0; i < standard_types_len; ++i) {
    if (GetPreferredExtensionForMimeType(standard_types[i], &extension))
      extensions->insert(extension);
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
       iter != source->end(); ++iter, ++i) {
    target->at(old_target_size + i) = *iter;
  }
}
}

void GetImageExtensions(std::vector<FilePath::StringType>* extensions) {
  base::hash_set<FilePath::StringType> unique_extensions;
  GetExtensionsHelper(kStandardImageTypes,
                      arraysize(kStandardImageTypes),
                      "image/",
                      &unique_extensions);
  HashSetToVector(&unique_extensions, extensions);
}

void GetAudioExtensions(std::vector<FilePath::StringType>* extensions) {
  base::hash_set<FilePath::StringType> unique_extensions;
  GetExtensionsHelper(kStandardAudioTypes,
                      arraysize(kStandardAudioTypes),
                      "audio/",
                      &unique_extensions);
  HashSetToVector(&unique_extensions, extensions);
}

void GetVideoExtensions(std::vector<FilePath::StringType>* extensions) {
  base::hash_set<FilePath::StringType> unique_extensions;
  GetExtensionsHelper(kStandardVideoTypes,
                      arraysize(kStandardVideoTypes),
                      "video/",
                      &unique_extensions);
  HashSetToVector(&unique_extensions, extensions);
}

void GetExtensionsForMimeType(const std::string& mime_type,
                              std::vector<FilePath::StringType>* extensions) {
  base::hash_set<FilePath::StringType> unique_extensions;
  FilePath::StringType extension;
  if (GetPreferredExtensionForMimeType(mime_type, &extension))
    unique_extensions.insert(extension);

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

  HashSetToVector(&unique_extensions, extensions);
}

}  // namespace net
