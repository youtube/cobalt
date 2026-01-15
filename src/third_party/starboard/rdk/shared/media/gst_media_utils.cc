//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#include <memory>
#include <map>
#include <type_traits>

#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/common/log.h"
#include "third_party/starboard/rdk/shared/media/gst_media_utils.h"
#include "third_party/starboard/rdk/shared/log_override.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace media {
namespace {

GST_DEBUG_CATEGORY(cobalt_gst_media_utils);
#define GST_CAT_DEFAULT cobalt_gst_media_utils

struct FeatureListDeleter {
  void operator()(GList* p) { gst_plugin_feature_list_free(p); }
};

struct CapsDeleter {
  void operator()(GstCaps* p) { gst_caps_unref(p); }
};

struct BufferDeleter {
  void operator()(GstBuffer* p) { gst_buffer_unref(p); }
};

using UniqueFeatureList = std::unique_ptr<GList, FeatureListDeleter>;
using UniqueCaps = std::unique_ptr<GstCaps, CapsDeleter>;
using UniqueBuffer = std::unique_ptr<GstBuffer, BufferDeleter>;

std::vector<UniqueBuffer>
ParseXiphStreamHeaders (const void* codec_data, gsize codec_data_size) {
  // Based on isomp4/matroska demuxers
  std::vector<UniqueBuffer> res;
  const guint8 *p = reinterpret_cast<const guint8*>(codec_data);
  gint i, offset;
  guint last, num_packets;
  std::vector<guint> length;

  if (codec_data == nullptr || codec_data_size == 0)
    return {};

  num_packets = p[0] + 1;
  // unlikely number of packets
  if (num_packets > 16) {
    GST_WARNING("too many packets in Xiph header.");
    return {};
  }

  length.resize(num_packets);
  last = 0;
  offset = 1;

  // first packets, read length values
  for (i = 0; i < num_packets - 1; i++) {
    length[i] = 0;
    while (offset < codec_data_size) {
      length[i] += p[offset];
      if (p[offset++] != 0xff)
        break;
    }
    last += length[i];
  }

  if (offset + last > codec_data_size) {
    GST_WARNING("Xiph header is out of codec data bounds.");
    return { };
  }

  // last packet is the remaining size
  length[i] = codec_data_size - offset - last;

  for (i = 0; i < num_packets; i++) {
    GstBuffer *header;
    if (offset + length[i] > codec_data_size) {
      GST_WARNING("Xiph header is out of codec data bounds.");
      return { };
    }
    header = gst_buffer_new_wrapped (g_memdup (p + offset, length[i]), length[i]);
    GST_BUFFER_FLAG_SET (header, GST_BUFFER_FLAG_HEADER);
    res.push_back(UniqueBuffer { header });
    offset += length[i];
  }

  return res;
}

bool IsAudioRawCaps(const UniqueCaps& caps) {
  const gchar* media_type = gst_structure_get_name (gst_caps_get_structure(caps.get(), 0));
  return g_strcmp0 ("audio/x-raw", media_type) == 0;
}

UniqueFeatureList GetFactoryForCaps(GList* elements,
                                    const UniqueCaps& caps,
                                    GstPadDirection direction) {
  SB_DCHECK(direction != GST_PAD_UNKNOWN);
#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *caps_str = gst_caps_to_string(caps.get());
    GST_DEBUG ("caps: %s", caps_str);
    g_free(caps_str);
  }
#endif
  UniqueFeatureList candidates{
      gst_element_factory_list_filter(elements, caps.get(), direction, false)};
  return candidates;
}

template <typename C>
bool GstRegistryHasElementForCodecImpl(C codec) {
  static_assert(std::is_same<C, SbMediaVideoCodec>::value ||
                std::is_same<C, SbMediaAudioCodec>::value, "Invalid codec");
  GST_DEBUG_CATEGORY_INIT(cobalt_gst_media_utils, "cobaltmediautils", 0,
                          "Cobalt GStreamer Utils");
  auto type = std::is_same<C, SbMediaVideoCodec>::value
                  ? GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO
                  : GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO;
  UniqueFeatureList decoder_factories{gst_element_factory_list_get_elements(
      GST_ELEMENT_FACTORY_TYPE_DECODER | type, GST_RANK_MARGINAL)};

  UniqueFeatureList elements;

  UniqueCaps caps { CodecToGstCaps(codec) };
  if (!caps) {
    GST_DEBUG("No caps for codec: %u", codec);
    return false;
  }

  elements = std::move(GetFactoryForCaps(decoder_factories.get(), caps, GST_PAD_SINK));
  if (elements) {
    // Decoder is there.
    GST_DEBUG("Found decoder for codec: %u, caps: %" GST_PTR_FORMAT, codec, caps.get());
    return true;
  }

  if (IsAudioRawCaps (caps)) {
    UniqueFeatureList sink_factories{gst_element_factory_list_get_elements(
            GST_ELEMENT_FACTORY_TYPE_SINK | type, GST_RANK_MARGINAL)};
    elements = std::move(GetFactoryForCaps(sink_factories.get(), caps, GST_PAD_SINK));
    if (elements) {
      GST_DEBUG("Found sink for codec: %u, caps: %" GST_PTR_FORMAT, codec, caps.get());
      return true;
    }
  }

  UniqueFeatureList parser_factories{gst_element_factory_list_get_elements(
      GST_ELEMENT_FACTORY_TYPE_PARSER | type, GST_RANK_MARGINAL)};

  GST_DEBUG("No decoder for codec: %u. Falling back to parsers.", codec);
  // No decoder. Check if there's a parser and a decoder accepting its caps.
  elements = std::move(GetFactoryForCaps(parser_factories.get(), caps, GST_PAD_SINK));
  if (elements) {
    for (GList* iter = elements.get(); iter; iter = iter->next) {
      GstElementFactory* gst_element_factory =
        static_cast<GstElementFactory*>(iter->data);
      const GList* pad_templates =
        gst_element_factory_get_static_pad_templates(gst_element_factory);
      for (const GList* pad_templates_iter = pad_templates;
         pad_templates_iter;
         pad_templates_iter = pad_templates_iter->next) {
        GstStaticPadTemplate* pad_template =
          static_cast<GstStaticPadTemplate*>(pad_templates_iter->data);
        if (pad_template->direction == GST_PAD_SRC) {
          UniqueCaps pad_caps{gst_static_pad_template_get_caps(pad_template)};
          if (GetFactoryForCaps(decoder_factories.get(), pad_caps, GST_PAD_SINK)) {
            GST_DEBUG("Found parser and decoder for codec: %u with caps: %" GST_PTR_FORMAT " "
                      "accepting parser's src caps: %" GST_PTR_FORMAT ".",
                      codec, caps.get(), pad_caps.get());
            return true;
          }
        }
      }
    }
  }

  SB_LOG(WARNING) << "Can not play codec: " << codec
                  << ", media type: " << gst_structure_get_name(gst_caps_get_structure(caps.get(), 0));
  return false;
}

template <typename C>
bool GstRegistryHasElementForCodec(C codec) {
  static std::map<C, bool> cache;
  auto it = cache.find(codec);
  if (it != cache.end())
    return it->second;
  bool r = GstRegistryHasElementForCodecImpl(codec);
  cache[codec] = r;
  return r;
}

}  // namespace

bool GstRegistryHasElementForMediaType(SbMediaVideoCodec codec) {
  return GstRegistryHasElementForCodec(codec);
}

bool GstRegistryHasElementForMediaType(SbMediaAudioCodec codec) {
  return GstRegistryHasElementForCodec(codec);
}

GstCaps* CodecToGstCaps(SbMediaVideoCodec codec) {
  switch (codec) {
    default:
    case kSbMediaVideoCodecNone:
      return nullptr;

    case kSbMediaVideoCodecH264:
      return gst_caps_new_simple ("video/x-h264",
       "stream-format", G_TYPE_STRING, "byte-stream",
       "alignment", G_TYPE_STRING, "nal", nullptr);

    case kSbMediaVideoCodecH265:
      return gst_caps_new_simple ("video/x-h265",
       "stream-format", G_TYPE_STRING, "byte-stream",
       "alignment", G_TYPE_STRING, "nal", nullptr);

    case kSbMediaVideoCodecMpeg2:
      return gst_caps_new_simple ("video/mpeg",
       "mpegversion", G_TYPE_INT, 2, nullptr);

    case kSbMediaVideoCodecTheora:
      return gst_caps_new_empty_simple ("video/x-theora");

    case kSbMediaVideoCodecVc1:
      return gst_caps_new_empty_simple ("video/x-vc1");

    case kSbMediaVideoCodecAv1:
      return gst_caps_new_empty_simple ("video/x-av1");

    case kSbMediaVideoCodecVp8:
      return gst_caps_new_empty_simple ("video/x-vp8");

    case kSbMediaVideoCodecVp9:
      return gst_caps_new_empty_simple ("video/x-vp9");
  }
}

GstCaps* CodecToGstCaps(SbMediaAudioCodec codec, const SbMediaAudioStreamInfo* info) {
  GstCaps* gst_caps = nullptr;
  if (info) {
    GST_DEBUG ("audio stream info: codec %u, mime '%s', channels %u, rate %u, bps %u",
               codec, info->mime,
               static_cast<uint32_t> (info->number_of_channels),
               static_cast<uint32_t> (info->samples_per_second),
               static_cast<uint32_t> (info->bits_per_sample));
    GST_MEMDUMP ("audio specific config",
                 reinterpret_cast<const guint8*>(info->audio_specific_config),
                 info->audio_specific_config_size);
  }

  switch (codec) {
    default:
    case kSbMediaAudioCodecNone:
      break;

    case kSbMediaAudioCodecAac: {
      gst_caps = gst_caps_new_simple (
        "audio/mpeg",
        "mpegversion", G_TYPE_INT, 4,
        nullptr);
      if (info) {
        gst_caps_set_simple (gst_caps,
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);
        if (info->audio_specific_config_size >= 2) {
          uint16_t codec_priv_size = info->audio_specific_config_size;
          const guint8* codec_priv = reinterpret_cast<const guint8*>(info->audio_specific_config);
          gst_codec_utils_aac_caps_set_level_and_profile(gst_caps, codec_priv, codec_priv_size);
        }
      }
      break;
    }

    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3: {
      gst_caps = gst_caps_new_empty_simple ("audio/x-eac3");
      if (info) {
        gst_caps_set_simple(gst_caps,
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);
      }
      break;
    }

    case kSbMediaAudioCodecOpus: {
      gst_caps = gst_caps_new_simple ( "audio/x-opus",
        "channel-mapping-family", G_TYPE_INT, 0, nullptr);
      if (info && info->audio_specific_config_size >= 19) {
        uint16_t codec_priv_size = info->audio_specific_config_size;
        const void* codec_priv = info->audio_specific_config;

        GstBuffer *tmp = gst_buffer_new_wrapped (g_memdup (codec_priv, codec_priv_size), codec_priv_size);
        gst_caps_take (&gst_caps, gst_codec_utils_opus_create_caps_from_header (tmp, NULL));
        gst_buffer_unref (tmp);
      }
      break;
    }

    case kSbMediaAudioCodecVorbis: {
      gst_caps = gst_caps_new_empty_simple ( "audio/x-vorbis" );
      if (info) {
        gst_caps_set_simple(gst_caps,
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);

        auto headers = ParseXiphStreamHeaders(info->audio_specific_config, info->audio_specific_config_size);
        if (!headers.empty()) {
          GValue array = G_VALUE_INIT;
          GValue value = G_VALUE_INIT;

          g_value_init (&array, GST_TYPE_ARRAY);
          g_value_init (&value, GST_TYPE_BUFFER);

          for (const auto &hdr : headers) {
            gst_value_set_buffer (&value, hdr.get());
            gst_value_array_append_value (&array, &value);
          }

          gst_caps_set_value(gst_caps, "streamheader", &array);
          g_value_reset (&value);
          g_value_reset (&array);
        }
      }
      break;
    }

    case kSbMediaAudioCodecMp3: {
      gst_caps = gst_caps_new_simple ( "audio/mpeg",
        "mpegversion", G_TYPE_INT, 1,
        "layer", G_TYPE_INT, 3,
        nullptr);
      if (info) {
        gst_caps_set_simple(gst_caps,
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);
      }
      break;
    }

    case kSbMediaAudioCodecFlac: {
      gst_caps = gst_caps_new_empty_simple ( "audio/x-flac" );
      if (info) {
        gst_caps_set_simple(gst_caps,
          "channels", G_TYPE_INT, info->number_of_channels,
          "rate", G_TYPE_INT, info->samples_per_second,
          nullptr);

        // MP4 parser includes stream info block in audio_specific_config,
        // see third_party/chromium/media/formats/mp4/box_definitions.cc:bool FlacSpecificBox::Parse
        const uint16_t kFlacStreaminfoSize = 34;
        if (info->audio_specific_config_size == kFlacStreaminfoSize) {
          GValue array = G_VALUE_INIT;
          GValue value = G_VALUE_INIT;
          g_value_init (&array, GST_TYPE_ARRAY);
          g_value_init (&value, GST_TYPE_BUFFER);

          GstBuffer *block;
          uint16_t block_plus_hdr_size = info->audio_specific_config_size + 4 + 4;
          block = gst_buffer_new_allocate(nullptr, block_plus_hdr_size, nullptr);
          if (block) {
            GstMapInfo write_info;
            gst_buffer_map (block, &write_info, GST_MAP_WRITE);

            memcpy (write_info.data, "fLaC", 4); // marker
            write_info.data[4] = 0x80; // is_last = true, type = 0x0
            write_info.data[5] = 0x00;
            write_info.data[6] = (info->audio_specific_config_size & 0xFF00) >> 8;
            write_info.data[7] = (info->audio_specific_config_size & 0x00FF) >> 0;
            memcpy(&write_info.data[8], info->audio_specific_config, info->audio_specific_config_size);

            gst_buffer_unmap (block, &write_info);

            GST_BUFFER_FLAG_SET (block, GST_BUFFER_FLAG_HEADER);

            gst_value_set_buffer (&value, block);
            gst_value_array_append_value (&array, &value);
            gst_buffer_unref (block);

            gst_caps_set_value(gst_caps, "streamheader", &array);
          }

          g_value_reset (&value);
          g_value_reset (&array);
        }
      }
      break;
    }

    case kSbMediaAudioCodecPcm: {
      gst_caps = gst_caps_new_empty_simple ( "audio/x-raw" );
      if (info) {
        // Starboard doesn't specify audio format in stream info.
        // There are 2 types defined in SbMediaAudioSampleType, so below is the best guess...
        const char* format = info->bits_per_sample == 32 ? "F32LE" : "S16LE";
        int channels = info->number_of_channels;
        gst_caps_set_simple(gst_caps,
          "format", G_TYPE_STRING, format,
          "rate", G_TYPE_INT, info->samples_per_second,
          "channels", G_TYPE_INT, channels,
          "layout", G_TYPE_STRING, "interleaved",
          "channel-mask", GST_TYPE_BITMASK, gst_audio_channel_get_fallback_mask(channels),
          nullptr);
      }
      break;
    }
  }

#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *caps_str = gst_caps ? gst_caps_to_string(gst_caps) : nullptr;
    GST_DEBUG ("codec: %u, caps: %s", codec, caps_str);
    g_free(caps_str);
  }
#endif

  return gst_caps;

}

}  // namespace media
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
