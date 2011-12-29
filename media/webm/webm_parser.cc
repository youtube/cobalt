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

enum ElementType {
  UNKNOWN,
  LIST,  // Referred to as Master Element in the Matroska spec.
  UINT,
  FLOAT,
  BINARY,
  STRING,
  SBLOCK,
  SKIP,
};

struct ElementIdInfo {
  ElementType type_;
  int id_;
};

struct ListElementInfo {
  int id_;
  int level_;
  const ElementIdInfo* id_info_;
  int id_info_count_;
};

// The following are tables indicating what IDs are valid sub-elements
// of particular elements. If an element is encountered that doesn't
// appear in the list, a parsing error is signalled. Some elements are
// marked as SKIP because they are valid, but we don't care about them
// right now.
static const ElementIdInfo kClusterIds[] = {
  {UINT, kWebMIdTimecode},
  {SBLOCK, kWebMIdSimpleBlock},
  {LIST, kWebMIdBlockGroup},
};

static const ElementIdInfo kSegmentIds[] = {
  {SKIP, kWebMIdSeekHead},  // TODO(acolwell): add SeekHead info
  {LIST, kWebMIdInfo},
  {LIST, kWebMIdCluster},
  {LIST, kWebMIdTracks},
  {SKIP, kWebMIdCues},  // TODO(acolwell): add CUES info
};

static const ElementIdInfo kInfoIds[] = {
  {SKIP, kWebMIdSegmentUID},
  {UINT, kWebMIdTimecodeScale},
  {FLOAT, kWebMIdDuration},
  {SKIP, kWebMIdDateUTC},
  {SKIP, kWebMIdTitle},
  {SKIP, kWebMIdMuxingApp},
  {SKIP, kWebMIdWritingApp},
};

static const ElementIdInfo kTracksIds[] = {
  {LIST, kWebMIdTrackEntry},
};

static const ElementIdInfo kTrackEntryIds[] = {
  {UINT, kWebMIdTrackNumber},
  {SKIP, kWebMIdTrackUID},
  {UINT, kWebMIdTrackType},
  {SKIP, kWebMIdFlagEnabled},
  {SKIP, kWebMIdFlagDefault},
  {SKIP, kWebMIdFlagForced},
  {UINT, kWebMIdFlagLacing},
  {UINT, kWebMIdDefaultDuration},
  {SKIP, kWebMIdName},
  {SKIP, kWebMIdLanguage},
  {STRING, kWebMIdCodecID},
  {BINARY, kWebMIdCodecPrivate},
  {SKIP, kWebMIdCodecName},
  {LIST, kWebMIdVideo},
  {LIST, kWebMIdAudio},
};

static const ElementIdInfo kVideoIds[] = {
  {SKIP, kWebMIdFlagInterlaced},
  {SKIP, kWebMIdStereoMode},
  {UINT, kWebMIdPixelWidth},
  {UINT, kWebMIdPixelHeight},
  {SKIP, kWebMIdPixelCropBottom},
  {SKIP, kWebMIdPixelCropTop},
  {SKIP, kWebMIdPixelCropLeft},
  {SKIP, kWebMIdPixelCropRight},
  {SKIP, kWebMIdDisplayWidth},
  {SKIP, kWebMIdDisplayHeight},
  {SKIP, kWebMIdDisplayUnit},
  {SKIP, kWebMIdAspectRatioType},
};

static const ElementIdInfo kAudioIds[] = {
  {SKIP, kWebMIdSamplingFrequency},
  {SKIP, kWebMIdOutputSamplingFrequency},
  {UINT, kWebMIdChannels},
  {SKIP, kWebMIdBitDepth},
};

#define LIST_ELEMENT_INFO(id, level, id_info) \
    { (id), (level), (id_info), arraysize(id_info) }

static const ListElementInfo kListElementInfo[] = {
  LIST_ELEMENT_INFO(kWebMIdCluster, 1, kClusterIds),
  LIST_ELEMENT_INFO(kWebMIdSegment, 0, kSegmentIds),
  LIST_ELEMENT_INFO(kWebMIdInfo, 1, kInfoIds),
  LIST_ELEMENT_INFO(kWebMIdTracks, 1, kTracksIds),
  LIST_ELEMENT_INFO(kWebMIdTrackEntry, 2, kTrackEntryIds),
  LIST_ELEMENT_INFO(kWebMIdVideo, 3, kVideoIds),
  LIST_ELEMENT_INFO(kWebMIdAudio, 3, kAudioIds),
};

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

int WebMParseElementHeader(const uint8* buf, int size,
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

// Finds ElementType for a specific ID.
static ElementType FindIdType(int id,
                              const ElementIdInfo* id_info,
                              int id_info_count) {

  // Check for global element IDs that can be anywhere.
  if (id == kWebMIdVoid || id == kWebMIdCRC32)
    return SKIP;

  for (int i = 0; i < id_info_count; ++i) {
    if (id == id_info[i].id_)
      return id_info[i].type_;
  }

  return UNKNOWN;
}

// Finds ListElementInfo for a specific ID.
static const ListElementInfo* FindListInfo(int id) {
  for (size_t i = 0; i < arraysize(kListElementInfo); ++i) {
    if (id == kListElementInfo[i].id_)
      return &kListElementInfo[i];
  }

  return NULL;
}

static int FindListLevel(int id) {
  const ListElementInfo* list_info = FindListInfo(id);
  if (list_info)
    return list_info->level_;

  return -1;
}

static int ParseSimpleBlock(const uint8* buf, int size,
                            WebMParserClient* client) {
  if (size < 4)
    return -1;

  // Return an error if the trackNum > 127. We just aren't
  // going to support large track numbers right now.
  if ((buf[0] & 0x80) != 0x80) {
    DVLOG(1) << "TrackNumber over 127 not supported";
    return -1;
  }

  int track_num = buf[0] & 0x7f;
  int timecode = buf[1] << 8 | buf[2];
  int flags = buf[3] & 0xff;
  int lacing = (flags >> 1) & 0x3;

  if (lacing != 0) {
    DVLOG(1) << "Lacing " << lacing << " not supported yet.";
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

static int ParseBinary(const uint8* buf, int size, int id,
                       WebMParserClient* client) {
  return client->OnBinary(id, buf, size) ? size : -1;
}

static int ParseString(const uint8* buf, int size, int id,
                       WebMParserClient* client) {
  std::string str(reinterpret_cast<const char*>(buf), size);
  return client->OnString(id, str) ? size : -1;
}

static int ParseNonListElement(ElementType type, int id, int64 element_size,
                               const uint8* buf, int size,
                               WebMParserClient* client) {
  DCHECK_GE(size, element_size);

  int result = -1;
  switch(type) {
    case SBLOCK:
      result = ParseSimpleBlock(buf, element_size, client);
      break;
    case LIST:
      NOTIMPLEMENTED();
      result = -1;
      break;
    case UINT:
      result = ParseUInt(buf, element_size, id, client);
      break;
    case FLOAT:
      result = ParseFloat(buf, element_size, id, client);
      break;
    case BINARY:
      result = ParseBinary(buf, element_size, id, client);
      break;
    case STRING:
      result = ParseString(buf, element_size, id, client);
      break;
    case SKIP:
      result = element_size;
      break;
    default:
      DVLOG(1) << "Unhandled ID type " << type;
      return -1;
  };

  DCHECK_LE(result, size);
  return result;
}

WebMParserClient::WebMParserClient() {}
WebMParserClient::~WebMParserClient() {}

WebMParserClient* WebMParserClient::OnListStart(int id) {
  DVLOG(1) << "Unexpected list element start with ID " << std::hex << id;
  return NULL;
}

bool WebMParserClient::OnListEnd(int id) {
  DVLOG(1) << "Unexpected list element end with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnUInt(int id, int64 val) {
  DVLOG(1) << "Unexpected unsigned integer element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnFloat(int id, double val) {
  DVLOG(1) << "Unexpected float element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnBinary(int id, const uint8* data, int size) {
  DVLOG(1) << "Unexpected binary element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnString(int id, const std::string& str) {
  DVLOG(1) << "Unexpected string element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnSimpleBlock(int track_num, int timecode, int flags,
                                     const uint8* data, int size) {
  DVLOG(1) << "Unexpected simple block element";
  return false;
}

WebMListParser::WebMListParser(int id, WebMParserClient* client)
    : state_(NEED_LIST_HEADER),
      root_id_(id),
      root_level_(FindListLevel(id)),
      root_client_(client) {
  DCHECK_GE(root_level_, 0);
  DCHECK(client);
}

WebMListParser::~WebMListParser() {}

void WebMListParser::Reset() {
  ChangeState(NEED_LIST_HEADER);
  list_state_stack_.clear();
}

int WebMListParser::Parse(const uint8* buf, int size) {
  DCHECK(buf);

  if (size < 0 || state_ == PARSE_ERROR || state_ == DONE_PARSING_LIST)
    return -1;

  if (size == 0)
    return 0;

  const uint8* cur = buf;
  int cur_size = size;
  int bytes_parsed = 0;

  while (cur_size > 0 && state_ != PARSE_ERROR && state_ != DONE_PARSING_LIST) {
    int element_id = 0;
    int64 element_size = 0;
    int result = WebMParseElementHeader(cur, cur_size, &element_id,
                                        &element_size);

    if (result < 0)
      return result;

    if (result == 0)
      return bytes_parsed;

    switch(state_) {
      case NEED_LIST_HEADER: {
        if (element_id != root_id_) {
          ChangeState(PARSE_ERROR);
          return -1;
        }

        // TODO(acolwell): Add support for lists of unknown size.
        if (element_size == kWebMUnknownSize) {
          ChangeState(PARSE_ERROR);
          return -1;
        }

        ChangeState(INSIDE_LIST);
        if (!OnListStart(root_id_, element_size))
          return -1;

        break;
      }

      case INSIDE_LIST: {
        int header_size = result;
        const uint8* element_data = cur + header_size;
        int element_data_size = cur_size - header_size;

        if (element_size < element_data_size)
          element_data_size = element_size;

        result = ParseListElement(header_size, element_id, element_size,
                                  element_data, element_data_size);

        DCHECK_LE(result, header_size + element_data_size);
        if (result < 0) {
          ChangeState(PARSE_ERROR);
          return -1;
        }

        if (result == 0)
          return bytes_parsed;

        break;
      }
      case DONE_PARSING_LIST:
      case PARSE_ERROR:
        // Shouldn't be able to get here.
        NOTIMPLEMENTED();
        break;
    }

    cur += result;
    cur_size -= result;
    bytes_parsed += result;
  }

  return (state_ == PARSE_ERROR) ? -1 : bytes_parsed;
}

bool WebMListParser::IsParsingComplete() const {
  return state_ == DONE_PARSING_LIST;
}

void WebMListParser::ChangeState(State new_state) {
  state_ = new_state;
}

int WebMListParser::ParseListElement(int header_size,
                                     int id, int64 element_size,
                                     const uint8* data, int size) {
  DCHECK_GT(list_state_stack_.size(), 0u);

  ListState& list_state = list_state_stack_.back();
  DCHECK(list_state.element_info_);

  const ListElementInfo* element_info = list_state.element_info_;
  ElementType id_type =
      FindIdType(id, element_info->id_info_, element_info->id_info_count_);

  // Unexpected ID.
  if (id_type == UNKNOWN) {
    DVLOG(1) << "No ElementType info for ID 0x" << std::hex << id;
    return -1;
  }

  // Make sure the whole element can fit inside the current list.
  int64 total_element_size = header_size + element_size;
  if (list_state.size_ != kWebMUnknownSize &&
      list_state.size_ < list_state.bytes_parsed_ + total_element_size) {
    return -1;
  }

  if (id_type == LIST) {
    list_state.bytes_parsed_ += header_size;

    if (!OnListStart(id, element_size))
      return -1;
    return header_size;
  }

  // Make sure we have the entire element before trying to parse a non-list
  // element.
  if (size < element_size)
    return 0;

  int bytes_parsed = ParseNonListElement(id_type, id, element_size,
                                         data, size, list_state.client_);
  DCHECK_LE(bytes_parsed, size);

  // Return if an error occurred or we need more data.
  // Note: bytes_parsed is 0 for a successful parse of a size 0 element. We
  // need to check the element_size to disambiguate the "need more data" case
  // from a successful parse.
  if (bytes_parsed < 0 || (bytes_parsed == 0 && element_size != 0))
    return bytes_parsed;

  int result = header_size + bytes_parsed;
  list_state.bytes_parsed_ += result;

  // See if we have reached the end of the current list.
  if (list_state.bytes_parsed_ == list_state.size_) {
    if (!OnListEnd())
      return -1;
  }

  return result;
}

bool WebMListParser::OnListStart(int id, int64 size) {
  const ListElementInfo* element_info = FindListInfo(id);
  if (!element_info)
    return false;

  int current_level = root_level_ + list_state_stack_.size() - 1;
  if (current_level + 1 != element_info->level_)
    return false;

  WebMParserClient* current_list_client = NULL;
  if (!list_state_stack_.empty()) {
    // Make sure the new list doesn't go past the end of the current list.
    ListState current_list_state = list_state_stack_.back();
    if (current_list_state.size_ != kWebMUnknownSize &&
        current_list_state.size_ < current_list_state.bytes_parsed_ + size)
      return false;
    current_list_client = current_list_state.client_;
  } else {
    current_list_client = root_client_;
  }

  WebMParserClient* new_list_client = current_list_client->OnListStart(id);
  if (!new_list_client)
    return false;

  ListState new_list_state = { id, size, 0, element_info, new_list_client };
  list_state_stack_.push_back(new_list_state);

  if (size == 0)
    return OnListEnd();

  return true;
}

bool WebMListParser::OnListEnd() {
  int lists_ended = 0;
  for (; !list_state_stack_.empty(); ++lists_ended) {
    const ListState& list_state = list_state_stack_.back();

    if (list_state.bytes_parsed_ != list_state.size_)
      break;

    list_state_stack_.pop_back();

    int64 bytes_parsed = list_state.bytes_parsed_;
    WebMParserClient* client = NULL;
    if (!list_state_stack_.empty()) {
      // Update the bytes_parsed_ for the parent element.
      list_state_stack_.back().bytes_parsed_ += bytes_parsed;
      client = list_state_stack_.back().client_;
    } else {
      client = root_client_;
    }

    if (!client->OnListEnd(list_state.id_))
      return false;
  }

  DCHECK_GE(lists_ended, 1);

  if (list_state_stack_.empty())
    ChangeState(DONE_PARSING_LIST);

  return true;
}

}  // namespace media
