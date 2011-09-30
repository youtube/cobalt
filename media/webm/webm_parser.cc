// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_parser.h"

// This file contains code to parse WebM file elements. It was created
// from information in the Matroska spec.
// http://www.matroska.org/technical/specs/index.html

#include <iomanip>

#include "base/logging.h"
#include "media/webm/webm_constants.h"

namespace media {

// Maximum depth of WebM elements. Some WebM elements are lists of
// other elements. This limits the number levels of recursion allowed.
static const int kMaxLevelDepth = 6;

enum ElementType {
  LIST,
  UINT,
  FLOAT,
  BINARY,
  STRING,
  SBLOCK,
  SKIP,
};

struct ElementIdInfo {
  int level_;
  ElementType type_;
  int id_;
};

struct ListElementInfo {
  int id_;
  const ElementIdInfo* id_info_;
  int id_info_size_;
};

// The following are tables indicating what IDs are valid sub-elements
// of particular elements. If an element is encountered that doesn't
// appear in the list, a parsing error is signalled. Some elements are
// marked as SKIP because they are valid, but we don't care about them
// right now.
static const ElementIdInfo kClusterIds[] = {
  {2, UINT, kWebMIdTimecode},
  {2, SBLOCK, kWebMIdSimpleBlock},
  {2, LIST, kWebMIdBlockGroup},
};

static const ElementIdInfo kInfoIds[] = {
  {2, SKIP, kWebMIdSegmentUID},
  {2, UINT, kWebMIdTimecodeScale},
  {2, FLOAT, kWebMIdDuration},
  {2, SKIP, kWebMIdDateUTC},
  {2, SKIP, kWebMIdTitle},
  {2, SKIP, kWebMIdMuxingApp},
  {2, SKIP, kWebMIdWritingApp},
};

static const ElementIdInfo kTracksIds[] = {
  {2, LIST, kWebMIdTrackEntry},
};

static const ElementIdInfo kTrackEntryIds[] = {
  {3, UINT, kWebMIdTrackNumber},
  {3, SKIP, kWebMIdTrackUID},
  {3, UINT, kWebMIdTrackType},
  {3, SKIP, kWebMIdFlagEnabled},
  {3, SKIP, kWebMIdFlagDefault},
  {3, SKIP, kWebMIdFlagForced},
  {3, UINT, kWebMIdFlagLacing},
  {3, UINT, kWebMIdDefaultDuration},
  {3, SKIP, kWebMIdName},
  {3, SKIP, kWebMIdLanguage},
  {3, STRING, kWebMIdCodecID},
  {3, BINARY, kWebMIdCodecPrivate},
  {3, SKIP, kWebMIdCodecName},
  {3, LIST, kWebMIdVideo},
  {3, LIST, kWebMIdAudio},
};

static const ElementIdInfo kVideoIds[] = {
  {4, SKIP, kWebMIdFlagInterlaced},
  {4, SKIP, kWebMIdStereoMode},
  {4, UINT, kWebMIdPixelWidth},
  {4, UINT, kWebMIdPixelHeight},
  {4, SKIP, kWebMIdPixelCropBottom},
  {4, SKIP, kWebMIdPixelCropTop},
  {4, SKIP, kWebMIdPixelCropLeft},
  {4, SKIP, kWebMIdPixelCropRight},
  {4, SKIP, kWebMIdDisplayWidth},
  {4, SKIP, kWebMIdDisplayHeight},
  {4, SKIP, kWebMIdDisplayUnit},
  {4, SKIP, kWebMIdAspectRatioType},
};

static const ElementIdInfo kAudioIds[] = {
  {4, SKIP, kWebMIdSamplingFrequency},
  {4, SKIP, kWebMIdOutputSamplingFrequency},
  {4, UINT, kWebMIdChannels},
  {4, SKIP, kWebMIdBitDepth},
};

static const ElementIdInfo kClustersOnly[] = {
  {1, LIST, kWebMIdCluster},
};

static const ListElementInfo kListElementInfo[] = {
  { kWebMIdCluster,    kClusterIds,    sizeof(kClusterIds) },
  { kWebMIdInfo,       kInfoIds,       sizeof(kInfoIds) },
  { kWebMIdTracks,     kTracksIds,     sizeof(kTracksIds) },
  { kWebMIdTrackEntry, kTrackEntryIds, sizeof(kTrackEntryIds) },
  { kWebMIdVideo,      kVideoIds,      sizeof(kVideoIds) },
  { kWebMIdAudio,      kAudioIds,      sizeof(kAudioIds) },
};

// Number of elements in kListElementInfo.
const int kListElementInfoCount =
    sizeof(kListElementInfo) / sizeof(ListElementInfo);

WebMParserClient::~WebMParserClient() {}

// Parses an element header id or size field. These fields are variable length
// encoded. The first byte indicates how many bytes the field occupies.
// |buf|  - The buffer to parse.
// |size| - The number of bytes in |buf|
// |max_bytes| - The maximum number of bytes the field can be. ID fields
//               set this to 4 & element size fields set this to 8. If the
//               first byte indicates a larger field size than this it is a
//               parser error.
// |mask_first_byte| - For element size fields the field length encoding bits
//                     need to be masked off. This parameter is true for
//                     element size fields and is false for ID field values.
//
// Returns: The number of bytes parsed on success. -1 on error.
static int ParseWebMElementHeaderField(const uint8* buf, int size,
                                       int max_bytes, bool mask_first_byte,
                                       int64* num) {
  DCHECK(buf);
  DCHECK(num);

  if (size < 0)
    return -1;

  if (size == 0)
    return 0;

  int mask = 0x80;
  uint8 ch = buf[0];
  int extra_bytes = -1;
  for (int i = 0; i < max_bytes; ++i) {
    if ((ch & mask) == mask) {
      *num = mask_first_byte ? ch & ~mask : ch;
      extra_bytes = i;
      break;
    }
    mask >>= 1;
  }

  if (extra_bytes == -1)
    return -1;

  // Return 0 if we need more data.
  if ((1 + extra_bytes) > size)
    return 0;

  int bytes_used = 1;

  for (int i = 0; i < extra_bytes; ++i)
    *num = (*num << 8) | (0xff & buf[bytes_used++]);

  return bytes_used;
}

// Parses an element header & returns the ID and element size.
//
// Returns: The number of bytes parsed on success. -1 on error.
// |*id| contains the element ID on success & undefined on error.
// |*element_size| contains the element size on success & undefined on error.
static int ParseWebMElementHeader(const uint8* buf, int size,
                                  int* id, int64* element_size) {
  DCHECK(buf);
  DCHECK_GE(size, 0);
  DCHECK(id);
  DCHECK(element_size);

  if (size == 0)
    return 0;

  int64 tmp = 0;
  int num_id_bytes = ParseWebMElementHeaderField(buf, size, 4, false, &tmp);

  if (num_id_bytes <= 0)
    return num_id_bytes;

  *id = static_cast<int>(tmp);

  int num_size_bytes = ParseWebMElementHeaderField(buf + num_id_bytes,
                                                   size - num_id_bytes,
                                                   8, true, &tmp);

  if (num_size_bytes <= 0)
    return num_size_bytes;

  *element_size = tmp;
  return num_id_bytes + num_size_bytes;
}

// Finds ElementIdInfo for a specific ID.
static const ElementIdInfo* FindIdInfo(int id,
                                       const ElementIdInfo* id_info,
                                       int id_info_size) {
  int count = id_info_size / sizeof(*id_info);
  for (int i = 0; i < count; ++i) {
    if (id == id_info[i].id_)
      return &id_info[i];
  }

  return NULL;
}

// Finds ListElementInfo for a specific ID.
static const ListElementInfo* FindListInfo(int id) {
  for (int i = 0; i < kListElementInfoCount; ++i) {
    if (id == kListElementInfo[i].id_)
      return &kListElementInfo[i];
  }

  return NULL;
}

static int ParseSimpleBlock(const uint8* buf, int size,
                            WebMParserClient* client) {
  if (size < 4)
    return -1;

  // Return an error if the trackNum > 127. We just aren't
  // going to support large track numbers right now.
  if ((buf[0] & 0x80) != 0x80) {
    VLOG(1) << "TrackNumber over 127 not supported";
    return -1;
  }

  int track_num = buf[0] & 0x7f;
  int timecode = buf[1] << 8 | buf[2];
  int flags = buf[3] & 0xff;
  int lacing = (flags >> 1) & 0x3;

  if (lacing != 0) {
    VLOG(1) << "Lacing " << lacing << " not supported yet.";
    return -1;
  }

  // Sign extend negative timecode offsets.
  if (timecode & 0x8000)
    timecode |= (-1 << 16);

  const uint8* frame_data = buf + 4;
  int frame_size = size - (frame_data - buf);
  if (!client->OnSimpleBlock(track_num, timecode, flags,
                             frame_data, frame_size)) {
    return -1;
  }

  return size;
}

static int ParseElements(const ElementIdInfo* id_info,
                         int id_info_size,
                         const uint8* buf, int size, int level,
                         WebMParserClient* client);

static int ParseElementList(const uint8* buf, int size,
                            int id, int level,
                            WebMParserClient* client) {
  const ListElementInfo* list_info = FindListInfo(id);

  if (!list_info) {
    VLOG(1) << "Failed to find list info for ID " << std::hex << id;
    return -1;
  }

  if (!client->OnListStart(id))
    return -1;

  int result = ParseElements(list_info->id_info_,
                             list_info->id_info_size_,
                             buf, size,
                             level + 1,
                             client);

  if (result <= 0)
    return result;

  if (!client->OnListEnd(id))
    return -1;

  DCHECK_EQ(result, size);
  return result;
}

static int ParseUInt(const uint8* buf, int size, int id,
                     WebMParserClient* client) {
  if ((size <= 0) || (size > 8))
    return -1;

  // Read in the big-endian integer.
  int64 value = 0;
  for (int i = 0; i < size; ++i)
    value = (value << 8) | buf[i];

  if (!client->OnUInt(id, value))
    return -1;

  return size;
}

static int ParseFloat(const uint8* buf, int size, int id,
                      WebMParserClient* client) {

  if ((size != 4) && (size != 8))
    return -1;

  double value = -1;

  // Read the bytes from big-endian form into a native endian integer.
  int64 tmp = 0;
  for (int i = 0; i < size; ++i)
    tmp = (tmp << 8) | buf[i];

  // Use a union to convert the integer bit pattern into a floating point
  // number.
  if (size == 4) {
    union {
      int32 src;
      float dst;
    } tmp2;
    tmp2.src = static_cast<int32>(tmp);
    value = tmp2.dst;
  } else if (size == 8) {
    union {
      int64 src;
      double dst;
    } tmp2;
    tmp2.src = tmp;
    value = tmp2.dst;
  } else {
    return -1;
  }

  if (!client->OnFloat(id, value))
    return -1;

  return size;
}

static int ParseElements(const ElementIdInfo* id_info,
                         int id_info_size,
                         const uint8* buf, int size, int level,
                         WebMParserClient* client) {
  DCHECK_GE(id_info_size, 0);
  DCHECK_GE(size, 0);
  DCHECK_GE(level, 0);

  const uint8* cur = buf;
  int cur_size = size;
  int used = 0;

  if (level > kMaxLevelDepth)
    return -1;

  while (cur_size > 0) {
    int id = 0;
    int64 element_size = 0;
    int result = ParseWebMElementHeader(cur, cur_size, &id, &element_size);

    if (result <= 0)
      return result;

    cur += result;
    cur_size -= result;
    used += result;

    // Check to see if the element is larger than the remaining data.
    if (element_size > cur_size)
      return 0;

    const ElementIdInfo* info = FindIdInfo(id, id_info, id_info_size);

    if (info == NULL) {
      VLOG(1) << "No info for ID " << std::hex << id;

      // TODO(acolwell): Change this to return -1 after the API has solidified.
      // We don't want to allow elements we don't recognize.
      cur += element_size;
      cur_size -= element_size;
      used += element_size;
      continue;
    }

    if (info->level_ != level) {
      VLOG(1) << "ID " << std::hex << id << std::dec << " at level "
              << level << " instead of " << info->level_;
      return -1;
    }

    switch(info->type_) {
      case SBLOCK:
        if (ParseSimpleBlock(cur, element_size, client) <= 0)
          return -1;
        break;
      case LIST:
        if (ParseElementList(cur, element_size, id, level, client) < 0)
          return -1;
        break;
      case UINT:
        if (ParseUInt(cur, element_size, id, client) <= 0)
          return -1;
        break;
      case FLOAT:
        if (ParseFloat(cur, element_size, id, client) <= 0)
          return -1;
        break;
      case BINARY:
        if (!client->OnBinary(id, cur, element_size))
          return -1;
        break;
      case STRING:
        if (!client->OnString(id,
                              std::string(reinterpret_cast<const char*>(cur),
                                          element_size)))
          return -1;
        break;
      case SKIP:
        // Do nothing.
        break;
      default:
        VLOG(1) << "Unhandled id type " << info->type_;
        return -1;
    };

    cur += element_size;
    cur_size -= element_size;
    used += element_size;
  }

  return used;
}

// Parses a single list element that matches |id|. This method fails if the
// buffer points to an element that does not match |id|.
int WebMParseListElement(const uint8* buf, int size, int id,
                         int level, WebMParserClient* client) {
  if (size < 0)
    return -1;

  if (size == 0)
    return 0;

  const uint8* cur = buf;
  int cur_size = size;
  int bytes_parsed = 0;
  int element_id = 0;
  int64 element_size = 0;
  int result = ParseWebMElementHeader(cur, cur_size, &element_id,
                                      &element_size);

  if (result <= 0)
    return result;

  cur += result;
  cur_size -= result;
  bytes_parsed += result;

  if (element_id != id)
    return -1;

  if (element_size > cur_size)
    return 0;

  if (element_size > 0) {
    result = ParseElementList(cur, element_size, element_id, level, client);

    if (result <= 0)
      return result;

    cur += result;
    cur_size -= result;
    bytes_parsed += result;
  }

  return bytes_parsed;
}

}  // namespace media
