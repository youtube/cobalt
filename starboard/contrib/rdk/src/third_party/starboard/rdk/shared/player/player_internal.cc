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
#include "third_party/starboard/rdk/shared/player/player_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <math.h>

#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/audio/audio.h>
#include <gst/audio/streamvolume.h>
#include <gst/base/gstbytewriter.h>
#include <gst/base/gstflowcombiner.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbaseparse.h>

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstring>
#include <thread>
#include <limits>
#include <condition_variable>
#include <mutex>

#include "starboard/common/once.h"
#include "starboard/thread.h"
#include "starboard/common/time.h"
#include "starboard/drm.h"
#include "starboard/common/log.h"
#include "third_party/starboard/rdk/shared/media/gst_media_utils.h"
#include "third_party/starboard/rdk/shared/hang_detector.h"
#include "third_party/starboard/rdk/shared/drm/gst_decryptor_ocdm.h"
#if defined(HAS_OCDM)
#include "third_party/starboard/rdk/shared/drm/drm_system_ocdm.h"
#endif
#include "third_party/starboard/rdk/shared/player/elements/gst_audio_clipping.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace player {

static constexpr int kMaxNumberOfSamplesPerWrite = 1;
static const char kCustomInstantRateChangeEventName[] = "custom-instant-rate-change";
static const char kDidReceiveFirstSegmentMsgName[] = "did-receive-first-segment";
static const char kDidReachBufferingTargetMsgName[] = "did-reach-buffering-target";

// static
int Player::MaxNumberOfSamplesPerWrite() {
  return kMaxNumberOfSamplesPerWrite;
}

using third_party::starboard::rdk::shared::drm::CreateDecryptorElement;
using third_party::starboard::rdk::shared::media::CodecToGstCaps;

// **************************** GST/GLIB Helpers **************************** //

namespace {

GST_DEBUG_CATEGORY(cobalt_gst_player_debug);
#define GST_CAT_DEFAULT cobalt_gst_player_debug

#if !defined(GST_HAS_HDR_SUPPORT) && GST_CHECK_VERSION(1, 18, 0)
#define GST_HAS_HDR_SUPPORT 1
#endif

static void PrintGstCaps(GstCaps* caps);
static GstElement* CreatePayloader();
static GstElement* CreateGstElement(const gchar* factory_name, const gchar* name_format, ...) G_GNUC_PRINTF (2, 3);

static GSourceFuncs SourceFunctions = {
    // prepare
    nullptr,
    // check
    nullptr,
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean {
      if (g_source_get_ready_time(source) == -1)
        return G_SOURCE_CONTINUE;
      g_source_set_ready_time(source, -1);
      return callback(userData);
    },
    // finalize
    nullptr,
    // closure_callback
    nullptr,
    // closure_marshall
    nullptr,
};

unsigned getGstPlayFlag(const char* nick) {
  static GFlagsClass* flagsClass = static_cast<GFlagsClass*>(
      g_type_class_ref(g_type_from_name("GstPlayFlags")));
  SB_DCHECK(flagsClass);

  GFlagsValue* flag = g_flags_get_value_by_nick(flagsClass, nick);
  if (!flag)
    return 0;

  return flag->value;
}

bool isRialtoEnabled() {
  static bool is_rialto_enabled = false;
  static gsize init = 0;

  if (g_once_init_enter (&init)) {
    GstElementFactory *factory = gst_element_factory_find("rialtomsevideosink");
    if (factory) {
      guint rank = gst_plugin_feature_get_rank((GstPluginFeature *)factory);
      gst_object_unref(GST_OBJECT(factory));
      is_rialto_enabled = rank >= GST_RANK_PRIMARY;
    }
    g_once_init_leave (&init, 1);
  }

  return is_rialto_enabled;
}

bool enableNativeAudio() {
  static bool enable_native_audio = false;
  static gsize init = 0;

  if (g_once_init_enter (&init)) {
    GstElementFactory* factory = gst_element_factory_find("brcmaudiosink");
    if (factory) {
      gst_object_unref(GST_OBJECT(factory));
      enable_native_audio = true;
    }
    else {
      enable_native_audio = isRialtoEnabled();
    }
    g_once_init_leave (&init, 1);
  }

  return enable_native_audio;
}

G_BEGIN_DECLS

#define GST_COBALT_TYPE_SRC (gst_cobalt_src_get_type())
#define GST_COBALT_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_COBALT_TYPE_SRC, GstCobaltSrc))
#define GST_COBALT_SRC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_COBALT_TYPE_SRC, GstCobaltSrcClass))
#define GST_IS_COLABT_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_COBALT_TYPE_SRC))
#define GST_IS_COBALT_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_COBALT_TYPE_SRC))

typedef struct _GstCobaltSrc GstCobaltSrc;
typedef struct _GstCobaltSrcClass GstCobaltSrcClass;
typedef struct _GstCobaltSrcPrivate GstCobaltSrcPrivate;

struct _GstCobaltSrc {
  GstBin parent;
  GstCobaltSrcPrivate* priv;
};

struct _GstCobaltSrcClass {
  GstBinClass parentClass;
};

GType gst_cobalt_src_get_type(void);

G_END_DECLS

struct _GstCobaltSrcPrivate {
  gchar* uri;
  guint pad_number;
  gboolean async_start;
  gboolean async_done;
  GstFlowCombiner* flow_combiner;
};

enum { PROP_0, PROP_LOCATION };

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE("src_%u",
                            GST_PAD_SRC,
                            GST_PAD_SOMETIMES,
                            GST_STATIC_CAPS_ANY);

static void gst_cobalt_src_uri_handler_init(gpointer gIface,
                                            gpointer ifaceData);
#define gst_cobalt_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(GstCobaltSrc,
                        gst_cobalt_src,
                        GST_TYPE_BIN,
                        G_ADD_PRIVATE(GstCobaltSrc)
                        G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER,
                                              gst_cobalt_src_uri_handler_init));

static void gst_cobalt_src_init(GstCobaltSrc* src) {
  GstCobaltSrcPrivate* priv = (GstCobaltSrcPrivate*)gst_cobalt_src_get_instance_private(src);
  new (priv) GstCobaltSrcPrivate();
  src->priv = priv;
  src->priv->pad_number = 0;
  src->priv->async_start = FALSE;
  src->priv->async_done = FALSE;
  src->priv->flow_combiner = gst_flow_combiner_new();
  g_object_set(GST_BIN(src), "message-forward", TRUE, NULL);
}

static void gst_cobalt_src_dispose(GObject* object) {
  GST_CALL_PARENT(G_OBJECT_CLASS, dispose, (object));
}

static void gst_cobalt_src_finalize(GObject* object) {
  GstCobaltSrc* src = GST_COBALT_SRC(object);
  GstCobaltSrcPrivate* priv = src->priv;

  g_free(priv->uri);
  gst_flow_combiner_free(priv->flow_combiner);
  priv->~GstCobaltSrcPrivate();

  GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void gst_cobalt_src_set_property(GObject* object,
                                        guint propID,
                                        const GValue* value,
                                        GParamSpec* pspec) {
  GstCobaltSrc* src = GST_COBALT_SRC(object);

  switch (propID) {
    case PROP_LOCATION:
      gst_uri_handler_set_uri(reinterpret_cast<GstURIHandler*>(src),
                              g_value_get_string(value), 0);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
      break;
  }
}

static void gst_cobalt_src_get_property(GObject* object,
                                        guint propID,
                                        GValue* value,
                                        GParamSpec* pspec) {
  GstCobaltSrc* src = GST_COBALT_SRC(object);
  GstCobaltSrcPrivate* priv = src->priv;

  GST_OBJECT_LOCK(src);
  switch (propID) {
    case PROP_LOCATION:
      g_value_set_string(value, priv->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
      break;
  }
  GST_OBJECT_UNLOCK(src);
}

// uri handler interface
static GstURIType gst_cobalt_src_uri_get_type(GType) {
  return GST_URI_SRC;
}

const gchar* const* gst_cobalt_src_get_protocols(GType) {
  static const char* protocols[] = {"cobalt", 0};
  return protocols;
}

static gchar* gst_cobalt_src_get_uri(GstURIHandler* handler) {
  GstCobaltSrc* src = GST_COBALT_SRC(handler);
  gchar* ret;

  GST_OBJECT_LOCK(src);
  ret = g_strdup(src->priv->uri);
  GST_OBJECT_UNLOCK(src);
  return ret;
}

static gboolean gst_cobalt_src_set_uri(GstURIHandler* handler,
                                       const gchar* uri,
                                       GError** error) {
  GstCobaltSrc* src = GST_COBALT_SRC(handler);
  GstCobaltSrcPrivate* priv = src->priv;

  if (GST_STATE(src) >= GST_STATE_PAUSED) {
    GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
    return FALSE;
  }

  GST_OBJECT_LOCK(src);

  g_free(priv->uri);
  priv->uri = 0;

  if (!uri) {
    GST_OBJECT_UNLOCK(src);
    return TRUE;
  }

  priv->uri = g_strdup(uri);
  GST_OBJECT_UNLOCK(src);
  return TRUE;
}

static void gst_cobalt_src_uri_handler_init(gpointer gIface, gpointer) {
  GstURIHandlerInterface* iface = (GstURIHandlerInterface*)gIface;

  iface->get_type = gst_cobalt_src_uri_get_type;
  iface->get_protocols = gst_cobalt_src_get_protocols;
  iface->get_uri = gst_cobalt_src_get_uri;
  iface->set_uri = gst_cobalt_src_set_uri;
}

static gboolean gst_cobalt_src_query_with_parent(GstPad* pad,
                                                 GstObject* parent,
                                                 GstQuery* query) {
  gboolean result = FALSE;

  switch (GST_QUERY_TYPE(query)) {
    default: {
      GstPad* target = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad));
      // Forward the query to the proxy target pad.
      if (target)
        result = gst_pad_query(target, query);
      gst_object_unref(target);
      break;
    }
  }

  return result;
}

static GstFlowReturn gst_cobalt_src_chain_with_parent(GstPad* pad, GstObject* parent, GstBuffer* buffer) {
  GstCobaltSrc* src = GST_COBALT_SRC(gst_object_get_parent(parent));
  GstFlowReturn ret = gst_proxy_pad_chain_default(pad, GST_OBJECT(src), buffer);
  if (ret != GST_FLOW_FLUSHING)
    ret = gst_flow_combiner_update_pad_flow(src->priv->flow_combiner, pad, ret);
  gst_object_unref(src);
  return ret;
}

static void gst_cobalt_src_handle_message(GstBin* bin, GstMessage* message) {
  GstCobaltSrc* src = GST_COBALT_SRC(GST_ELEMENT(bin));

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS: {
      gboolean emit_eos = TRUE;
      GstPad* pad = gst_element_get_static_pad(
          GST_ELEMENT(GST_MESSAGE_SRC(message)), "src");

      GST_DEBUG_OBJECT(src, "EOS received from %s",
                       GST_MESSAGE_SRC_NAME(message));
      g_object_set_data(G_OBJECT(pad), "is-eos", GINT_TO_POINTER(1));
      gst_object_unref(pad);
      for (guint i = 0; i < src->priv->pad_number; i++) {
        gchar* name = g_strdup_printf("src_%u", i);
        GstPad* src_pad = gst_element_get_static_pad(GST_ELEMENT(src), name);
        GstPad* target = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(src_pad));
        gint is_eos =
            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(target), "is-eos"));
        gst_object_unref(target);
        gst_object_unref(src_pad);
        g_free(name);

        if (!is_eos) {
          emit_eos = FALSE;
          break;
        }
      }

      gst_message_unref(message);

      if (emit_eos) {
        GST_DEBUG_OBJECT(src,
                         "All appsrc elements are EOS, emitting event now.");
        gst_element_send_event(GST_ELEMENT(bin), gst_event_new_eos());
      }
      break;
    }
    default:
      GST_BIN_CLASS(parent_class)->handle_message(bin, message);
      break;
  }
}

void gst_cobalt_src_setup_and_add_app_src(SbMediaType media_type,
                                          GstElement* element,
                                          GstElement* appsrc,
                                          GstAppSrcCallbacks* callbacks,
                                          gpointer user_data,
                                          gboolean inject_decryptor) {
  const uint32_t kAudioMaxBytes = 256 * 1024;
  const uint32_t kVideoMaxBytes = 8 * 1024 * 1024;

  uint32_t max_bytes = (media_type == kSbMediaTypeVideo) ? kVideoMaxBytes : kAudioMaxBytes;

  g_object_set(appsrc,
               "block", FALSE,
               "format", GST_FORMAT_TIME,
               "min-percent", 50,
               nullptr);
  gst_app_src_set_stream_type(GST_APP_SRC(appsrc), GST_APP_STREAM_TYPE_SEEKABLE);
  gst_app_src_set_emit_signals(GST_APP_SRC(appsrc), FALSE);
  gst_app_src_set_callbacks(GST_APP_SRC(appsrc), callbacks, user_data, nullptr);
  gst_app_src_set_max_bytes(GST_APP_SRC(appsrc), max_bytes);
  gst_base_src_set_format(GST_BASE_SRC(appsrc), GST_FORMAT_TIME);

  GstCobaltSrc* src = GST_COBALT_SRC(element);
  gchar* name = g_strdup_printf("src_%u", src->priv->pad_number);
  src->priv->pad_number++;
  gst_bin_add(GST_BIN(element), appsrc);

  GstElement* src_elem = appsrc;
  GstElement* decryptor = inject_decryptor ? CreateDecryptorElement(nullptr) : nullptr;
  GstElement* payloader = nullptr;
  if (decryptor && media_type == kSbMediaTypeVideo && !isRialtoEnabled()) {
    payloader = CreatePayloader();
  }

  if (decryptor) {
    GST_DEBUG_OBJECT(element, "Injecting decryptor element %" GST_PTR_FORMAT, decryptor);

    gst_bin_add(GST_BIN(element), decryptor);
    gst_element_sync_state_with_parent(decryptor);
    gst_element_link(src_elem, decryptor);
    src_elem = decryptor;
  }

  if (payloader) {
    GST_DEBUG_OBJECT(element, "Injecting payloader element %" GST_PTR_FORMAT, payloader);

    if (GST_IS_BASE_TRANSFORM(payloader)) {
      gst_base_transform_set_in_place(GST_BASE_TRANSFORM(payloader), TRUE);
    }

    gst_bin_add(GST_BIN(element), payloader);
    gst_element_sync_state_with_parent(payloader);
    gst_element_link(src_elem, payloader);
    src_elem = payloader;
  }

  {
    GstElement* queue = gst_element_factory_make("queue", nullptr);
    g_object_set (
      G_OBJECT (queue),
      "max-size-buffers", 60,
      "max-size-bytes", 0,
      "max-size-time", (gint64) 0,
      "silent", TRUE,
      nullptr);
    gst_bin_add(GST_BIN(element), queue);
    gst_element_sync_state_with_parent(queue);
    gst_element_link(src_elem, queue);
    src_elem = queue;
  }

  GstPad* target_pad = gst_element_get_static_pad(src_elem, "src");
  GstPad* pad = gst_ghost_pad_new(name, target_pad);

  GstMessage *msg = gst_message_new_application(
    GST_OBJECT(appsrc), gst_structure_new_empty(kDidReceiveFirstSegmentMsgName));
  gst_pad_add_probe (
    pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
    [](GstPad * pad, GstPadProbeInfo * info, gpointer data) -> GstPadProbeReturn {
      GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
      if ( GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT ) {
        GstPad *peer_pad = gst_pad_get_peer(pad);
        if (peer_pad) {
          gst_pad_send_event(peer_pad, gst_event_ref(event));
          gst_event_replace (
            (GstEvent **) &info->data,
            gst_event_new_sink_message(kDidReceiveFirstSegmentMsgName, GST_MESSAGE(data)));
          gst_object_unref(peer_pad);
        }
        return GST_PAD_PROBE_REMOVE;
      }
      return GST_PAD_PROBE_OK;
    }, msg, [](gpointer data) { gst_message_unref(GST_MESSAGE(data)); });

  // Allocation query can stall streaming thread if sent during pre-roll.
  // Drop the query since we don't use proposed allocation anyway.
  gst_pad_add_probe (
    pad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
    [](GstPad * pad, GstPadProbeInfo * info, gpointer data) -> GstPadProbeReturn {
      GstQuery* query = GST_PAD_PROBE_INFO_QUERY(info);
      if (GST_QUERY_TYPE (query) == GST_QUERY_ALLOCATION) {
        GST_TRACE_OBJECT(pad, "Drop allocation query");
        return GST_PAD_PROBE_DROP;
      }
      return GST_PAD_PROBE_OK;
    }, nullptr, nullptr);

  auto proxypad = GST_PAD(gst_proxy_pad_get_internal(GST_PROXY_PAD(pad)));
  gst_flow_combiner_add_pad(src->priv->flow_combiner, proxypad);
  gst_pad_set_chain_function(proxypad, static_cast<GstPadChainFunction>(gst_cobalt_src_chain_with_parent));
  gst_object_unref(proxypad);

  gst_pad_set_query_function(pad, gst_cobalt_src_query_with_parent);
  gst_pad_set_active(pad, TRUE);

  gst_element_add_pad(element, pad);
  GST_OBJECT_FLAG_SET(pad, GST_PAD_FLAG_NEED_PARENT);

  gst_element_sync_state_with_parent(appsrc);

  g_free(name);
  gst_object_unref(target_pad);
}

static void gst_cobalt_src_do_async_start(GstCobaltSrc* src) {
  GstCobaltSrcPrivate* priv = src->priv;
  if (priv->async_done)
    return;
  priv->async_start = TRUE;
  GST_BIN_CLASS(parent_class)
      ->handle_message(GST_BIN(src),
                       gst_message_new_async_start(GST_OBJECT(src)));
}

static void gst_cobalt_src_do_async_done(GstCobaltSrc* src) {
  GstCobaltSrcPrivate* priv = src->priv;
  if (priv->async_start) {
    GST_BIN_CLASS(parent_class)
        ->handle_message(
            GST_BIN(src),
            gst_message_new_async_done(GST_OBJECT(src), GST_CLOCK_TIME_NONE));
    priv->async_start = FALSE;
    priv->async_done = TRUE;
  }
}

void gst_cobalt_src_all_app_srcs_added(GstElement* element) {
  GstCobaltSrc* src = GST_COBALT_SRC(element);

  GST_DEBUG_OBJECT(src,
                   "===> All sources registered, completing state-change "
                   "(TID:%d)",
                   SbThreadGetId());
  gst_element_no_more_pads(element);
  gst_cobalt_src_do_async_done(src);
}

static GstStateChangeReturn gst_cobalt_src_change_state(
    GstElement* element,
    GstStateChange transition) {
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstCobaltSrc* src = GST_COBALT_SRC(element);
  GstCobaltSrcPrivate* priv = src->priv;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_cobalt_src_do_async_start(src);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
  if (G_UNLIKELY(ret == GST_STATE_CHANGE_FAILURE)) {
    gst_cobalt_src_do_async_done(src);
    return ret;
  }

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED: {
      if (!priv->async_done)
        ret = GST_STATE_CHANGE_ASYNC;
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY: {
      gst_cobalt_src_do_async_done(src);
      break;
    }
    default:
      break;
  }

  return ret;
}

static void gst_cobalt_src_class_init(GstCobaltSrcClass* klass) {
  GObjectClass* oklass = G_OBJECT_CLASS(klass);
  GstElementClass* eklass = GST_ELEMENT_CLASS(klass);
  GstBinClass* bklass = GST_BIN_CLASS(klass);

  oklass->dispose = gst_cobalt_src_dispose;
  oklass->finalize = gst_cobalt_src_finalize;
  oklass->set_property = gst_cobalt_src_set_property;
  oklass->get_property = gst_cobalt_src_get_property;

  gst_element_class_add_pad_template(
      eklass, gst_static_pad_template_get(&src_template));
  gst_element_class_set_metadata(eklass, "Cobalt source element", "Source",
                                 "Handles data incoming from the Cobalt player",
                                 "Pawel Stanek <p.stanek@metrological.com>");
  g_object_class_install_property(
      oklass, PROP_LOCATION,
      g_param_spec_string(
          "location", "location", "Location to read from", 0,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  bklass->handle_message = GST_DEBUG_FUNCPTR(gst_cobalt_src_handle_message);
  eklass->change_state = GST_DEBUG_FUNCPTR(gst_cobalt_src_change_state);
}

void ParseMaxVideoCapabilities(const char* caps_str, uint32_t *w, uint32_t *h, uint32_t *f) {
  auto parse_entry = [&w, &h, &f](std::string& entry) {
    auto end = std::remove_if(entry.begin(), entry.end(),
      [](unsigned char c) { return std::isspace(c) || c == '"' || c == '\''; });
    if (end != entry.end()) {
      *end = '\0';
    }
    auto mid = std::find(entry.begin(), end, '=');
    if (mid == entry.begin() || (mid + 1) == end) {
      return;
    }
    auto key_len = static_cast<size_t>(std::distance(entry.begin(), mid));
    if ( f && ::strncasecmp("framerate", entry.c_str(), key_len) == 0 ) {
      *f = strtoul(&(*(mid + 1)), nullptr, 10);
    }
    else if ( w && ::strncasecmp("width", entry.c_str(), key_len) == 0 ) {
      *w = strtoul(&(*(mid + 1)), nullptr, 10);
    }
    else if ( h && ::strncasecmp("height", entry.c_str(), key_len) == 0 ) {
      *h = strtoul(&(*(mid + 1)), nullptr, 10);
    }
  };
  std::istringstream ss(caps_str);
  std::string txt;
  while(std::getline(ss, txt, ';')) {
    parse_entry(txt);
  }
}

#if defined(GST_HAS_HDR_SUPPORT) && GST_HAS_HDR_SUPPORT
static GstVideoColorRange RangeIdToGstVideoColorRange(SbMediaRangeId value) {
  switch (value) {
    case kSbMediaRangeIdLimited:
      return GST_VIDEO_COLOR_RANGE_16_235;
    case kSbMediaRangeIdFull:
      return GST_VIDEO_COLOR_RANGE_0_255;
    default:
    case kSbMediaRangeIdUnspecified:
      return GST_VIDEO_COLOR_RANGE_UNKNOWN;
  }
}

static GstVideoColorMatrix MatrixIdToGstVideoColorMatrix(SbMediaMatrixId value) {
  switch (value) {
    case kSbMediaMatrixIdRgb:
      return GST_VIDEO_COLOR_MATRIX_RGB;
    case kSbMediaMatrixIdBt709:
      return GST_VIDEO_COLOR_MATRIX_BT709;
    case kSbMediaMatrixIdFcc:
      return GST_VIDEO_COLOR_MATRIX_FCC;
    case kSbMediaMatrixIdBt470Bg:
    case kSbMediaMatrixIdSmpte170M:
      return GST_VIDEO_COLOR_MATRIX_BT601;
    case kSbMediaMatrixIdSmpte240M:
      return GST_VIDEO_COLOR_MATRIX_SMPTE240M;
    case kSbMediaMatrixIdBt2020NonconstantLuminance:
      return GST_VIDEO_COLOR_MATRIX_BT2020;
    case kSbMediaMatrixIdUnspecified:
    default:
      return GST_VIDEO_COLOR_MATRIX_UNKNOWN;
  }
}

static GstVideoTransferFunction TransferIdToGstVideoTransferFunction(SbMediaTransferId value) {
  switch (value) {
    case kSbMediaTransferIdBt709:
    case kSbMediaTransferIdSmpte170M:
      return GST_VIDEO_TRANSFER_BT709;
    case kSbMediaTransferIdGamma22:
      return GST_VIDEO_TRANSFER_GAMMA22;
    case kSbMediaTransferIdGamma28:
      return GST_VIDEO_TRANSFER_GAMMA28;
    case kSbMediaTransferIdSmpte240M:
      return GST_VIDEO_TRANSFER_SMPTE240M;
    case kSbMediaTransferIdLinear:
      return GST_VIDEO_TRANSFER_GAMMA10;
    case kSbMediaTransferIdLog:
      return GST_VIDEO_TRANSFER_LOG100;
    case kSbMediaTransferIdLogSqrt:
      return GST_VIDEO_TRANSFER_LOG316;
    case kSbMediaTransferIdIec6196621:
      return GST_VIDEO_TRANSFER_SRGB;
    case kSbMediaTransferId10BitBt2020:
      return GST_VIDEO_TRANSFER_BT2020_10;
    case kSbMediaTransferId12BitBt2020:
      return GST_VIDEO_TRANSFER_BT2020_12;
    case kSbMediaTransferIdSmpteSt2084:
      return GST_VIDEO_TRANSFER_SMPTE2084;
    case kSbMediaTransferIdAribStdB67:
      return GST_VIDEO_TRANSFER_ARIB_STD_B67;
    case kSbMediaTransferIdUnspecified:
    default:
      return GST_VIDEO_TRANSFER_UNKNOWN;
  }
}

static GstVideoColorPrimaries PrimaryIdToGstVideoColorPrimaries(SbMediaPrimaryId value) {
  switch (value) {
    case kSbMediaPrimaryIdBt709:
      return GST_VIDEO_COLOR_PRIMARIES_BT709;
    case kSbMediaPrimaryIdBt470M:
      return GST_VIDEO_COLOR_PRIMARIES_BT470M;
    case kSbMediaPrimaryIdBt470Bg:
      return GST_VIDEO_COLOR_PRIMARIES_BT470BG;
    case kSbMediaPrimaryIdSmpte170M:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTE170M;
    case kSbMediaPrimaryIdSmpte240M:
      return GST_VIDEO_COLOR_PRIMARIES_SMPTE240M;
    case kSbMediaPrimaryIdFilm:
      return GST_VIDEO_COLOR_PRIMARIES_FILM;
    case kSbMediaPrimaryIdBt2020:
      return GST_VIDEO_COLOR_PRIMARIES_BT2020;
    case kSbMediaPrimaryIdUnspecified:
    default:
      return GST_VIDEO_COLOR_PRIMARIES_UNKNOWN;
  }
}

static void AddColorMetadataToGstCaps(GstCaps* caps, const SbMediaColorMetadata& color_metadata) {
  GstVideoColorimetry colorimetry;
  colorimetry.range = RangeIdToGstVideoColorRange(color_metadata.range);
  colorimetry.matrix = MatrixIdToGstVideoColorMatrix(color_metadata.matrix);
  colorimetry.transfer = TransferIdToGstVideoTransferFunction(color_metadata.transfer);
  colorimetry.primaries = PrimaryIdToGstVideoColorPrimaries(color_metadata.primaries);

  if (colorimetry.range != GST_VIDEO_COLOR_RANGE_UNKNOWN ||
      colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_UNKNOWN ||
      colorimetry.transfer != GST_VIDEO_TRANSFER_UNKNOWN ||
      colorimetry.primaries != GST_VIDEO_COLOR_PRIMARIES_UNKNOWN) {
    gchar *tmp =
      gst_video_colorimetry_to_string (&colorimetry);
    gst_caps_set_simple (caps, "colorimetry", G_TYPE_STRING, tmp, NULL);
    GST_DEBUG ("Setting \"colorimetry\" to %s", tmp);
    g_free (tmp);
  }
  GstVideoMasteringDisplayInfo mastering_display_info;
  gst_video_mastering_display_info_init (&mastering_display_info);

  mastering_display_info.display_primaries[0].x = (guint16)(color_metadata.mastering_metadata.primary_r_chromaticity_x * 50000);
  mastering_display_info.display_primaries[0].y = (guint16)(color_metadata.mastering_metadata.primary_r_chromaticity_y * 50000);
  mastering_display_info.display_primaries[1].x = (guint16)(color_metadata.mastering_metadata.primary_g_chromaticity_x * 50000);
  mastering_display_info.display_primaries[1].y = (guint16)(color_metadata.mastering_metadata.primary_g_chromaticity_y * 50000);
  mastering_display_info.display_primaries[2].x = (guint16)(color_metadata.mastering_metadata.primary_b_chromaticity_x * 50000);
  mastering_display_info.display_primaries[2].y = (guint16)(color_metadata.mastering_metadata.primary_b_chromaticity_y * 50000);
  mastering_display_info.white_point.x = (guint16)(color_metadata.mastering_metadata.white_point_chromaticity_x * 50000);
  mastering_display_info.white_point.y = (guint16)(color_metadata.mastering_metadata.white_point_chromaticity_y * 50000);
  mastering_display_info.max_display_mastering_luminance = (guint32)ceil(color_metadata.mastering_metadata.luminance_max);
  mastering_display_info.min_display_mastering_luminance = (guint32)ceil(color_metadata.mastering_metadata.luminance_min);

  gchar *tmp =
      gst_video_mastering_display_info_to_string(&mastering_display_info);
  gst_caps_set_simple (caps, "mastering-display-info", G_TYPE_STRING, tmp, NULL);
  GST_DEBUG ("Setting \"mastering-display-info\" to %s", tmp);
  g_free (tmp);
  if (color_metadata.max_cll && color_metadata.max_fall) {
    GstVideoContentLightLevel content_light_level;
    content_light_level.max_content_light_level = color_metadata.max_cll;
    content_light_level.max_frame_average_light_level = color_metadata.max_fall;
    gchar *tmp = gst_video_content_light_level_to_string(&content_light_level);
    gst_caps_set_simple (caps, "content-light-level", G_TYPE_STRING, tmp, NULL);
    GST_DEBUG ("setting \"content-light-level\" to %s", tmp);
    g_free (tmp);
  }
}
#else
static void AddColorMetadataToGstCaps(GstCaps*, const SbMediaColorMetadata&) {}
#endif

static int CompareColorMetadata(const SbMediaColorMetadata& lhs, const SbMediaColorMetadata& rhs) {
  return memcmp(&lhs, &rhs, sizeof(SbMediaColorMetadata));
}

static void AddVideoInfoToGstCaps(const SbMediaVideoStreamInfo& info, GstCaps* caps) {
  AddColorMetadataToGstCaps(caps, info.color_metadata);
  gst_caps_set_simple (caps,
    "width", G_TYPE_INT, info.frame_width,
    "height", G_TYPE_INT, info.frame_height,
    NULL);

  if (info.max_video_capabilities && *info.max_video_capabilities) {
    uint32_t framerate = 0;
    ParseMaxVideoCapabilities(info.max_video_capabilities, nullptr, nullptr, &framerate);
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION, framerate, 1, NULL);
  }
}

static void PrintPositionPerSink(GstElement* element, GstDebugLevel level)
{
#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) < level)
    return;
#endif

  auto fold_func = [](const GValue *vitem, GValue*, gpointer data) -> gboolean {
    GstDebugLevel level = (GstDebugLevel)GPOINTER_TO_INT(data);
    GstObject *item = GST_OBJECT(g_value_get_object (vitem));
    if (GST_IS_BIN (item)) {
      PrintPositionPerSink(GST_ELEMENT(item), level);
    }
    else if (GST_IS_BASE_SINK(item)) {
      GstElement* el = GST_ELEMENT(item);
      gint64 position = -1;
      GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);
      if (gst_element_query(el, query)) {
        gst_query_parse_position(query, 0, &position);
      }
      gst_query_unref(query);
      GST_CAT_LEVEL_LOG (GST_CAT_DEFAULT, level, el,
        "Position from %s : %" GST_TIME_FORMAT, GST_ELEMENT_NAME(el), GST_TIME_ARGS(position));
    }
    return TRUE;
  };

  GstBin *bin = GST_BIN_CAST (element);
  GstIterator *iter = gst_bin_iterate_sinks (bin);

  bool keep_going = true;
  while (keep_going) {
    GstIteratorResult ires;
    ires = gst_iterator_fold (iter, fold_func, NULL, GINT_TO_POINTER(level));
    switch (ires) {
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      default:
        keep_going = false;
        break;
    }
  }
  gst_iterator_free (iter);
}

static void PrintGstCaps(GstCaps* caps) {
#ifndef GST_DISABLE_GST_DEBUG
  if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) >= GST_LEVEL_INFO) {
    gchar *caps_str = gst_caps_to_string(caps);
    GST_INFO("caps: %s", caps_str);
    g_free(caps_str);
  }
#endif
}

static GstElement* CreatePayloader() {
  static GstElementFactory* factory = nullptr;
  static gsize init = 0;

  if (g_once_init_enter (&init)) {
    factory = gst_element_factory_find("svppay");
    g_once_init_leave (&init, 1);
  }

  if (!factory) {
    GST_DEBUG("svppay not found");
    return nullptr;
  }

  return gst_element_factory_create(factory, nullptr);
}


static GstElement* CreateGstElement(const gchar* factory_name, const gchar* name_format, ...) {
  GstElement *result;
  gchar *name;
  va_list args;
  va_start(args, name_format);
  name = g_strdup_vprintf(name_format, args);
  va_end(args);
  result = gst_element_factory_make(factory_name, name);
  g_free(name);
  return result;
}

static void PushStreamHeaders(GstAppSrc* src, GstCaps* caps) {
  const GstStructure* s = gst_caps_get_structure (caps, 0);
  const GValue *streamheader = gst_structure_get_value (s, "streamheader");
  g_return_if_fail (G_VALUE_HOLDS (streamheader, GST_TYPE_ARRAY));
  for (guint i = 0; i < gst_value_array_get_size (streamheader); ++i) {
    const GValue *header = gst_value_array_get_value (streamheader, i);
    g_return_if_fail (G_VALUE_HOLDS (header, GST_TYPE_BUFFER));
    gst_app_src_push_buffer(src, gst_buffer_ref(gst_value_get_buffer (header)));
  }
}

}  // namespace

// ********************************* Player ******************************** //
namespace {

const int kMaxIvSize = 16;

enum class MediaType {
  kNone = 0,
  kAudio = 1,
  kVideo = 2,
  kBoth = kAudio | kVideo
};

struct Task {
  virtual ~Task() {}
  virtual void Do() = 0;
  virtual void PrintInfo() = 0;
};

static const char* PlayerStateToStr(SbPlayerState state) {
#define CASE(x) case x: return #x
    switch(state) {
        CASE(kSbPlayerStateInitialized);
        CASE(kSbPlayerStatePrerolling);
        CASE(kSbPlayerStatePresenting);
        CASE(kSbPlayerStateEndOfStream);
        CASE(kSbPlayerStateDestroyed);
    }
#undef CASE
    return "unknown";
}

static const char* DecoderStateToStr(SbPlayerDecoderState state) {
#define CASE(x) case x: return #x
    switch(state) {
        CASE(kSbPlayerDecoderStateNeedsData);
    }
#undef CASE
    return "unknown";
}

class PlayerStatusTask : public Task {
 public:
  PlayerStatusTask(SbPlayerStatusFunc func,
                   SbPlayer player,
                   int ticket,
                   void* ctx,
                   SbPlayerState state) {
    this->func_ = func;
    this->player_ = player;
    this->ticket_ = ticket;
    this->ctx_ = ctx;
    this->state_ = state;
  }

  ~PlayerStatusTask() override {}

  void Do() override { func_(player_, ctx_, state_, ticket_); }

  void PrintInfo() override {
    GST_DEBUG("PlayerStatusTask state:%d (%s), ticket:%d", state_, PlayerStateToStr(state_), ticket_);
  }

 private:
  SbPlayerStatusFunc func_;
  SbPlayer player_;
  int ticket_;
  void* ctx_;
  SbPlayerState state_;
};

class PlayerDestroyedTask : public PlayerStatusTask {
 public:
  PlayerDestroyedTask(SbPlayerStatusFunc func,
                      SbPlayer player,
                      int ticket,
                      void* ctx,
                      GMainLoop* loop)
      : PlayerStatusTask(func, player, ticket, ctx, kSbPlayerStateDestroyed) {
    this->loop_ = loop;
  }

  ~PlayerDestroyedTask() override {}

  void Do() override {
    PlayerStatusTask::Do();
    g_main_loop_quit(loop_);
  }

  void PrintInfo() override {
    GST_TRACE("PlayerDestroyedTask: START");
    PlayerStatusTask::PrintInfo();
    GST_TRACE("PlayerDestroyedTask: END");
  }

 private:
  GMainLoop* loop_;
};

class DecoderStatusTask : public Task {
 public:
  DecoderStatusTask(SbPlayerDecoderStatusFunc func,
                    SbPlayer player,
                    int ticket,
                    void* ctx,
                    SbPlayerDecoderState state,
                    MediaType media) {
    this->func_ = func;
    this->player_ = player;
    this->ticket_ = ticket;
    this->ctx_ = ctx;
    this->state_ = state;
    this->media_ = media;
  }

  ~DecoderStatusTask() override {}

  void Do() override {
    if ((static_cast<int>(media_) & static_cast<int>(MediaType::kAudio)) != 0)
      func_(player_, ctx_, kSbMediaTypeAudio, state_, ticket_);
    if ((static_cast<int>(media_) & static_cast<int>(MediaType::kVideo)) != 0)
      func_(player_, ctx_, kSbMediaTypeVideo, state_, ticket_);
  }

  void PrintInfo() override {
    GST_TRACE("DecoderStatusTask state:%d (%s), ticket:%d, media:%d", state_,
              DecoderStateToStr(state_), ticket_, static_cast<int>(media_));
  }

 private:
  SbPlayerDecoderStatusFunc func_;
  SbPlayer player_;
  int ticket_;
  void* ctx_;
  SbPlayerDecoderState state_;
  MediaType media_;
};

class PlayerErrorTask : public Task {
 public:
  PlayerErrorTask(SbPlayerErrorFunc func,
                  SbPlayer player,
                  void* ctx,
                  SbPlayerError error,
                  const char* msg) {
    this->func_ = func;
    this->player_ = player;
    this->ctx_ = ctx;
    this->error_ = error;
    this->msg_ = msg;
  }

  ~PlayerErrorTask() override {}

  void Do() override { func_(player_, ctx_, error_, msg_.c_str()); }

  void PrintInfo() override { GST_TRACE("PlayerErrorTask"); }

 private:
  SbPlayerErrorFunc func_;
  SbPlayer player_;
  SbPlayerError error_;
  void* ctx_;
  std::string msg_;
};

class FunctionTask: public Task {
public:
  FunctionTask(std::function<void()> && fn, const char debug_msg[])
    : func_(std::move(fn))
    , msg_(debug_msg) { }

  ~FunctionTask() override {}

  void Do() override { func_(); }

  void PrintInfo() override { GST_TRACE("FunctionTask: %s", msg_); }
private:
  std::function<void()> func_;
  const char* msg_;
};

class PlayerImpl : public Player {
 public:
  PlayerImpl(SbPlayer player,
             SbWindow window,
             SbMediaVideoCodec video_codec,
             SbMediaAudioCodec audio_codec,
             SbDrmSystem drm_system,
             const SbMediaAudioStreamInfo& audio_info,
             const char* max_video_capabilities,
             SbPlayerDeallocateSampleFunc sample_deallocate_func,
             SbPlayerDecoderStatusFunc decoder_status_func,
             SbPlayerStatusFunc player_status_func,
             SbPlayerErrorFunc player_error_func,
             void* context,
             SbPlayerOutputMode output_mode,
             SbDecodeTargetGraphicsContextProvider* provider);
  ~PlayerImpl() override;

  // Player
  void MarkEOS(SbMediaType stream_type) override;
  void WriteSample(SbMediaType sample_type,
                   const SbPlayerSampleInfo* sample_infos,
                   int number_of_sample_infos) override;
  void SetVolume(double volume) override;
  void Seek(int64_t seek_to_timestamp, int ticket) override;
  bool SetRate(double rate) override;
  void GetInfo(SbPlayerInfo* info) override;
  void SetBounds(int zindex, int x, int y, int w, int h) override;

  GstElement* GetPipeline() const { return pipeline_;  }
  bool IsValid() const { return playback_thread_.joinable(); }
  bool HasMaxVideoCaps() const { return !max_video_capabilities_.empty(); }
  void AudioConfigurationChanged();

 private:
  enum class State {
    kNull,
    kInitial,
    kInitialPreroll,
    kPrerollAfterSeek,
    kPresenting,
    kEnded,
  };

  static const char* PrivatePlayerStateToStr(State state) {
#define CASE(x) case x: return #x
    switch(state) {
        CASE(State::kNull);
        CASE(State::kInitial);
        CASE(State::kInitialPreroll);
        CASE(State::kPrerollAfterSeek);
        CASE(State::kPresenting);
        CASE(State::kEnded);
    }
#undef CASE
    return "unknown";
  }

  enum MediaTimestampIndex {
    kAudioIndex,
    kVideoIndex,
    kMediaNumber,
  };

  class PendingSample {
   public:
    PendingSample() = delete;
    PendingSample& operator=(const PendingSample&) = delete;
    PendingSample(const PendingSample&) = delete;

    PendingSample& operator=(PendingSample&& other) {
      type_ = other.type_;
      buffer_ = other.buffer_;
      other.buffer_ = nullptr;
      serial_ = other.serial_;
      other.serial_ = 0;
      timestamp_ = other.timestamp_;
      other.timestamp_ = GST_CLOCK_TIME_NONE;
      return *this;
    }

    PendingSample(PendingSample&& other) { operator=(std::move(other)); }

    PendingSample(SbMediaType type,
                  GstBuffer* buffer,
                  uint64_t serial)
        : type_(type),
          buffer_(buffer),
          serial_(serial),
          timestamp_(GST_BUFFER_TIMESTAMP(buffer_)) {
      SB_DCHECK(gst_buffer_is_writable(buffer));
    }

    ~PendingSample() {
      if (buffer_)
        gst_buffer_unref(buffer_);
    }

    SbMediaType Type() const { return type_; }
    GstClockTime Timestamp() const { return timestamp_; }
    GstBuffer* CopyBuffer() const { return gst_buffer_copy_deep(buffer_); }
    GstBuffer* TakeBuffer() { GstBuffer* res = buffer_; buffer_ = nullptr; return res; }
    uint64_t SerialID() const { return serial_; }

   private:
    SbMediaType type_;
    GstBuffer* buffer_;
    uint64_t serial_;
    GstClockTime timestamp_;
  };

  struct PendingBounds {
    PendingBounds() : x{0}, y{0}, w{0}, h{0} {}
    PendingBounds(int ix, int iy, int iw, int ih)
        : x{ix}, y{iy}, w{iw}, h{ih} {}
    bool IsEmpty() { return w == 0 && h == 0; }
    int x;
    int y;
    int w;
    int h;
  };

  using PendingSamples = std::vector<PendingSample>;
  using SamplesPendingKey = std::map<std::string, PendingSamples>;

  static gboolean BusMessageCallback(GstBus* bus,
                                     GstMessage* message,
                                     gpointer user_data);
  static void* ThreadEntryPoint(void* context);
  static gboolean WorkerTask(gpointer user_data);
  static gboolean FinishSourceSetup(gpointer user_data);
  static void AppSrcNeedData(GstAppSrc* src, guint length, gpointer user_data);
  static void AppSrcEnoughData(GstAppSrc* src, gpointer user_data);
  static gboolean AppSrcSeekData(GstAppSrc* src,
                                 guint64 offset,
                                 gpointer user_data);
  static void SetupSource(GstElement* pipeline,
                          GstElement* source,
                          PlayerImpl* self);
  static void SetupElement(GstElement* pipeline,
                           GstElement* element,
                           PlayerImpl* self);
  static void OnVideoBufferUnderflow(PlayerImpl* self);

  bool ChangePipelineState(GstState state);
  guint DispatchOnWorkerThread(Task* task) const;
  GstClockTime GetPosition();
  bool WriteSample(SbMediaType sample_type,
                   GstBuffer* buffer,
                   uint64_t serial_id);
  MediaType GetBothMediaTypeTakingCodecsIntoAccount() const;
  void RecordTimestamp(SbMediaType type, GstClockTime timestamp);
  GstClockTime MinTimestamp(MediaType* origin) const;

  void DecoderNeedsData(MediaType media) const {
    SB_DCHECK(media != MediaType::kNone);
    int need_data = static_cast<int>(media) & ~decoder_state_data_;
    if (need_data == 0) {
      GST_LOG_OBJECT(pipeline_, "Already sent 'kSbPlayerDecoderStateNeedsData' for media type: %d, ignoring new request", static_cast<int>(media));
      return;
    }
    if ((eos_data_ & need_data) == need_data) {
      GST_LOG_OBJECT(pipeline_, "Stream(%d) already ended, ignoring needs data request", need_data);
      return;
    }

    decoder_state_data_ |= need_data;

#if 0
    if ((need_data & static_cast<int>(MediaType::kAudio)) != 0)
      decoder_status_func_(player_, context_, kSbMediaTypeAudio, kSbPlayerDecoderStateNeedsData, ticket_);
    if ((need_data & static_cast<int>(MediaType::kVideo)) != 0)
      decoder_status_func_(player_, context_, kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData, ticket_);
#else
    DispatchOnWorkerThread(new DecoderStatusTask(
      decoder_status_func_, player_, ticket_, context_,
      kSbPlayerDecoderStateNeedsData, static_cast<MediaType>(need_data)));
#endif
  }

  gboolean HandleBusMessage(GstBus* bus, GstMessage* message);
  void HandleApplicationMessage(GstBus* bus, GstMessage* message);
  void UpdatePresentingState();
  void WritePendingSamples();
  void CheckBuffering(GstClockTime position);
  void ConfigureLimitedVideo();
  void SchedulePlayingStateUpdate();
  void AddBufferingProbe(GstClockTime target, int ticket);
  void HandleInititialSeek();
  void DidEnd();

  SbPlayer player_;
  SbWindow window_;
  SbMediaVideoCodec video_codec_;
  SbMediaAudioCodec audio_codec_;
  SbDrmSystem drm_system_;
  std::string max_video_capabilities_;
  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
  SbPlayerErrorFunc player_error_func_;
  void* context_{nullptr};
  SbPlayerOutputMode output_mode_;
  SbDecodeTargetGraphicsContextProvider* provider_{nullptr};
  GMainLoop* main_loop_{nullptr};
  GMainContext* main_loop_context_{nullptr};
  GstElement* source_{nullptr};
  GstElement* video_appsrc_{nullptr};
  GstElement* audio_appsrc_{nullptr};
  GstElement* pipeline_{nullptr};
  guint source_setup_id_{0};
  guint bus_watch_id_{0};
  std::thread playback_thread_;
  std::mutex mutex_;
  std::mutex source_setup_mutex_;
  std::mutex seek_mutex_;
  double rate_{1.0};
  int ticket_{SB_PLAYER_INITIAL_TICKET};
  mutable GstClockTime seek_position_{GST_CLOCK_TIME_NONE};
  GstClockTime max_sample_timestamps_[kMediaNumber]{0};
  GstClockTime min_sample_timestamp_{GST_CLOCK_TIME_NONE};
  MediaType min_sample_timestamp_origin_{MediaType::kNone};
  bool is_seek_pending_{false};
  double pending_rate_{.0};
  int has_enough_data_{static_cast<int>(MediaType::kBoth)};
  mutable int decoder_state_data_{static_cast<int>(MediaType::kNone)};
  int eos_data_{static_cast<int>(MediaType::kNone)};
  int total_video_frames_{0};
  int dropped_video_frames_{0};
  int frame_width_{0};
  int frame_height_{0};
  State state_{State::kNull};
  PendingSamples pending_samples_;
  mutable GstClockTime cached_position_ns_{GST_CLOCK_TIME_NONE};
  PendingBounds pending_bounds_;
  SbMediaColorMetadata color_metadata_{};
  bool force_stop_ { false };
  uint64_t samples_serial_[kMediaNumber] { 0 };

  bool has_oob_write_pending_{false};
  std::condition_variable pending_oob_write_condition_;

  guint hang_monitor_source_id_ { 0 };
  HangMonitor hang_monitor_ { "Player" };
  GstCaps* audio_caps_ { nullptr };
  GstCaps* video_caps_ { nullptr };
  GstClockTime buf_target_min_ts_ { GST_CLOCK_TIME_NONE };
  bool need_instant_rate_change_ { false };
  int need_first_segment_ack_ { static_cast<int>(MediaType::kBoth) };
  int buffering_state_ { 0 };
  mutable guint playing_state_update_source_id_{0u};
};

struct PlayerRegistry
{
  std::mutex mutex_;
  std::vector<PlayerImpl*> players_;

  void Add(PlayerImpl *p) {
    std::lock_guard lock(mutex_);
    auto it = std::find(players_.begin(), players_.end(), p);
    if (it == players_.end()) {
      players_.push_back(p);
    }
  }

  void Remove(PlayerImpl *p) {
    std::lock_guard lock(mutex_);
    players_.erase(std::remove(players_.begin(), players_.end(), p), players_.end());
  }

  void ForceStop() {
    std::vector<GstElement*> pipelines;
    {
      std::lock_guard lock(mutex_);
      for(const auto& p: players_) {
        GstElement* pipeline = p->GetPipeline();
        if (pipeline) {
          gst_object_ref(pipeline);
          pipelines.push_back(pipeline);
        }
      }
    }
    for (GstElement* pipeline : pipelines) {
      GstStructure* structure = gst_structure_new_empty("force-stop");
      gst_element_post_message(pipeline, gst_message_new_application(GST_OBJECT(pipeline), structure));
      gst_object_unref(pipeline);
    }
  }

  bool CanCreate(const char* max_video_capabilities) {
    #if !defined(COBALT_BUILD_TYPE_GOLD)
    bool has_max_video_caps_set = (max_video_capabilities && *max_video_capabilities);
    std::lock_guard lock(mutex_);
    for(const auto& p: players_) {
      if (p->HasMaxVideoCaps() == has_max_video_caps_set)
        return false;
    }
    #endif
    return true;
  }

  void AudioConfigurationChanged() {
    std::lock_guard lock(mutex_);
    for(const auto& p: players_) {
      p->AudioConfigurationChanged();
    }
  }
};
SB_ONCE_INITIALIZE_FUNCTION(PlayerRegistry, GetPlayerRegistry);

PlayerImpl::PlayerImpl(SbPlayer player,
                       SbWindow window,
                       SbMediaVideoCodec video_codec,
                       SbMediaAudioCodec audio_codec,
                       SbDrmSystem drm_system,
                       const SbMediaAudioStreamInfo& audio_info,
                       const char* max_video_capabilities,
                       SbPlayerDeallocateSampleFunc sample_deallocate_func,
                       SbPlayerDecoderStatusFunc decoder_status_func,
                       SbPlayerStatusFunc player_status_func,
                       SbPlayerErrorFunc player_error_func,
                       void* context,
                       SbPlayerOutputMode output_mode,
                       SbDecodeTargetGraphicsContextProvider* provider)
    : player_(player),
      window_(window),
      video_codec_(video_codec),
      audio_codec_(audio_codec),
      drm_system_(drm_system),
      sample_deallocate_func_(sample_deallocate_func),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      context_(context) {

  GST_DEBUG_CATEGORY_INIT(cobalt_gst_player_debug, "gstplayer", 0,
                          "Cobalt player");

  static bool disable_audio = !!getenv("COBALT_DISABLE_AUDIO");
  if (disable_audio)
    audio_codec_ = kSbMediaAudioCodecNone;

  main_loop_context_ = g_main_context_new ();
  g_main_context_push_thread_default(main_loop_context_);
  main_loop_ = g_main_loop_new(main_loop_context_, FALSE);

  GSource* src = g_timeout_source_new(hang_monitor_.GetResetInterval().count());
  g_source_set_callback(src, [] (gpointer data) ->gboolean {
    PlayerImpl& player = *static_cast<PlayerImpl*>(data);
    GstState state, pending;
    GstStateChangeReturn result = gst_element_get_state(player.pipeline_, &state, &pending, 0);
    GstClockTime position = player.GetPosition();
    GST_INFO_OBJECT(
      player.pipeline_,
      "Player state: %s (pending: %s, result: %s), position: %" GST_TIME_FORMAT "",
      gst_element_state_get_name(state),
      gst_element_state_get_name(pending),
      gst_element_state_change_return_get_name(result),
      GST_TIME_ARGS(position));
    player.hang_monitor_.Reset();
    return G_SOURCE_CONTINUE;
  }, this, nullptr);
  hang_monitor_source_id_ = g_source_attach(src, main_loop_context_);
  g_source_unref(src);

  GST_INFO_OBJECT(pipeline_,"Creating player with max capabilities: '%s', audio codec: 0x%x, video codec: 0x%x",
                  max_video_capabilities, audio_codec_, video_codec_);

  GstElementFactory* src_factory = gst_element_factory_find("cobaltsrc");
  if (!src_factory) {
    gst_element_register(0, "cobaltsrc", GST_RANK_PRIMARY + 100,
                         GST_COBALT_TYPE_SRC);
  } else {
    gst_object_unref(src_factory);
  }

  static int player_id = 0;
  player_id++;

  pipeline_ = CreateGstElement("playbin", "media-pipeline-%d", player_id);

  unsigned flagAudio = getGstPlayFlag("audio");
  unsigned flagVideo = getGstPlayFlag("video");
  unsigned flagNativeVideo = getGstPlayFlag("native-video");
  unsigned flagNativeAudio = enableNativeAudio() ? getGstPlayFlag("native-audio") : 0;

  g_object_set(pipeline_, "flags",
               flagAudio | flagVideo | flagNativeVideo | flagNativeAudio,
               nullptr);
  g_signal_connect(pipeline_, "source-setup",
                   G_CALLBACK(&PlayerImpl::SetupSource), this);
  g_signal_connect(pipeline_, "element-setup",
                   G_CALLBACK(&PlayerImpl::SetupElement), this);
  g_object_set(pipeline_, "uri", "cobalt://", nullptr);

  if (max_video_capabilities && *max_video_capabilities) {
    max_video_capabilities_ = max_video_capabilities;
    ConfigureLimitedVideo();
  }

  if (audio_codec_ == kSbMediaAudioCodecNone) {
    has_enough_data_ &= ~static_cast<int>(MediaType::kAudio);
  }

  if (video_codec_ == kSbMediaVideoCodecNone) {
    has_enough_data_ &= ~static_cast<int>(MediaType::kVideo);
  }

  need_first_segment_ack_ = has_enough_data_;

  GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  bus_watch_id_ = gst_bus_add_watch(bus, &PlayerImpl::BusMessageCallback, this);
  gst_object_unref(bus);

  video_appsrc_ = CreateGstElement("appsrc", "vidsrc-%d", player_id);
  audio_appsrc_ = CreateGstElement("appsrc", "audsrc-%d", player_id);

  GstElement* playsink = (gst_bin_get_by_name(GST_BIN(pipeline_), "playsink"));
  if (playsink) {
    g_object_set(G_OBJECT(playsink), "send-event-mode", 0, nullptr);
    g_object_unref(playsink);
  } else {
    GST_WARNING_OBJECT(pipeline_, "No playsink ?!?!?");
  }

  if (audio_codec_ == kSbMediaAudioCodecPcm) {
    GstElement* filter = elements::CreateAudioClippingElement(nullptr);
    g_object_set(pipeline_, "audio-filter", filter, nullptr);
  }

  if (drm_system_) {
#if defined(HAS_OCDM)
    using third_party::starboard::rdk::shared::drm::DrmSystemOcdm;
    reinterpret_cast<DrmSystemOcdm*>( drm_system_ )->AddRef();
#endif
    GstContext* context = gst_context_new("cobalt-drm-system", FALSE);
    GstStructure* context_structure = gst_context_writable_structure(context);
    gst_structure_set(context_structure, "drm-system-instance", G_TYPE_POINTER, drm_system_, nullptr);
    gst_element_set_context(GST_ELEMENT(pipeline_), context);
    gst_context_unref(context);
  }

  ChangePipelineState(GST_STATE_READY);
  g_main_context_pop_thread_default(main_loop_context_);

  if (gst_element_get_state(pipeline_, nullptr, nullptr, 0) == GST_STATE_CHANGE_FAILURE)
    return;

  playback_thread_ = std::thread(&PlayerImpl::ThreadEntryPoint, this);

  if (playback_thread_.joinable()) {
    while(!g_main_loop_is_running(main_loop_))
      g_usleep(1);
  }
  GetPlayerRegistry()->Add(this);
}

PlayerImpl::~PlayerImpl() {
  GetPlayerRegistry()->Remove(this);

  GST_INFO_OBJECT(pipeline_, "Destroying player");
  {
    std::lock_guard lock(source_setup_mutex_);
    if (source_setup_id_ > 0) {
      GSource* src = g_main_context_find_source_by_id(main_loop_context_, source_setup_id_);
      g_source_destroy(src);
    }
  }
  if (bus_watch_id_ > 0) {
    GSource* src = g_main_context_find_source_by_id(main_loop_context_, bus_watch_id_);
    g_source_destroy(src);
  }
  if (hang_monitor_source_id_ > 0) {
    GSource* src = g_main_context_find_source_by_id(main_loop_context_, hang_monitor_source_id_);
    g_source_destroy(src);
    hang_monitor_.Reset();
  }
  ChangePipelineState(GST_STATE_NULL);
  GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
  gst_object_unref(bus);
  if (playback_thread_.joinable()) {
    DispatchOnWorkerThread(new PlayerDestroyedTask(
      player_status_func_, player_, ticket_, context_, main_loop_));
    playback_thread_.join();
  }
  if (audio_caps_) {
    gst_caps_unref(audio_caps_);
  }
  if (video_caps_) {
    gst_caps_unref(video_caps_);
  }
  g_main_loop_unref(main_loop_);
  g_main_context_unref(main_loop_context_);
  GST_INFO_OBJECT(pipeline_, "BYE BYE player");
  g_object_unref(pipeline_);
  if (drm_system_) {
#if defined(HAS_OCDM)
    using third_party::starboard::rdk::shared::drm::DrmSystemOcdm;
    reinterpret_cast<DrmSystemOcdm*>( drm_system_ )->Release();
#endif
  }
}

// static
gboolean PlayerImpl::BusMessageCallback(GstBus* bus,
                                        GstMessage* message,
                                        gpointer user_data) {
  PlayerImpl* self = static_cast<PlayerImpl*>(user_data);
  return self->HandleBusMessage(bus, message);
}

gboolean PlayerImpl::HandleBusMessage(GstBus* bus, GstMessage* message) {
  GST_TRACE("%d", SbThreadGetId());
  GST_LOG_OBJECT(pipeline_, "Got GST message '%s' from '%s'", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_APPLICATION: {
      HandleApplicationMessage(bus, message);
      break;
    }

    case GST_MESSAGE_EOS:
      if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline_)) {
        GST_INFO_OBJECT(pipeline_, "EOS");
        DidEnd();
      }
      break;

    case GST_MESSAGE_ERROR: {
      GError* err = nullptr;
      gchar* debug = nullptr;
      gst_message_parse_error(message, &err, &debug);

      std::string file_name = "cobalt_";
      file_name += (GST_OBJECT_NAME(pipeline_));
      file_name += "_err_";
      file_name += std::to_string(err->code);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipeline_),
                                        GST_DEBUG_GRAPH_SHOW_ALL,
                                        file_name.c_str());


      bool is_eos = (eos_data_ == (int)GetBothMediaTypeTakingCodecsIntoAccount());
      if (err->domain == GST_STREAM_ERROR && is_eos) {
        GST_WARNING_OBJECT(pipeline_, "Got stream error. But all streams are ended, so reporting EOS. Error code %d: %s (%s).",
          err->code, err->message, debug);
        DidEnd();
      } else {
        GST_ERROR_OBJECT(pipeline_, "Error %d: %s (%s)", err->code, err->message, debug);
        DispatchOnWorkerThread(new PlayerErrorTask(
          player_error_func_, player_, context_,
          kSbPlayerErrorDecode, err->message));
      }
      g_free(debug);
      g_error_free(err);
      break;
    }

    case GST_MESSAGE_STATE_CHANGED: {
      if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline_)) {
        GstState old_state, new_state, pending;
        gst_message_parse_state_changed(message, &old_state, &new_state,
                                        &pending);
        GST_INFO_OBJECT(GST_MESSAGE_SRC(message),
                        "===> State changed (old: %s, new: %s, pending: %s)",
                        gst_element_state_get_name(old_state),
                        gst_element_state_get_name(new_state),
                        gst_element_state_get_name(pending));
        std::string file_name = "cobalt_";
        file_name += (GST_OBJECT_NAME(pipeline_));
        file_name += "_";
        file_name += gst_element_state_get_name(old_state);
        file_name += "_";
        file_name += gst_element_state_get_name(new_state);
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(pipeline_),
                                          GST_DEBUG_GRAPH_SHOW_ALL,
                                          file_name.c_str());

        if (GST_STATE(pipeline_) >= GST_STATE_PAUSED) {
          int ticket = 0;
          bool is_seek_pending = false;
          bool is_rate_pending = false;
          double rate = 0.;
          GstClockTime pending_seek_pos = GST_CLOCK_TIME_NONE;

          {
            std::lock_guard lock(mutex_);
            ticket = ticket_;
            is_seek_pending = is_seek_pending_;
            is_rate_pending = pending_rate_ != .0;
            pending_seek_pos = seek_position_;
            SB_DCHECK(!is_seek_pending || GST_CLOCK_TIME_IS_VALID(seek_position_));
            rate = pending_rate_;
            if (is_seek_pending && is_rate_pending) {
              is_rate_pending = false;
              rate_ = rate;
              pending_rate_ = .0;
            }
            if (state_ == State::kPrerollAfterSeek ||
                state_ == State::kInitialPreroll) {
              has_oob_write_pending_ |= is_seek_pending;
            }
          }

          if (video_codec_ != kSbMediaVideoCodecNone && !pending_bounds_.IsEmpty()) {
            PendingBounds bounds = pending_bounds_;
            pending_bounds_ = {};
            SetBounds(0, bounds.x, bounds.y, bounds.w, bounds.h);
          }

          if (is_rate_pending && GST_STATE(pipeline_) == GST_STATE_PLAYING) {
            GST_INFO_OBJECT(pipeline_,"Sending pending SetRate(rate=%lf)", rate);
            SetRate(rate);
          } else if (is_seek_pending) {
            GST_INFO_OBJECT(pipeline_, "Sending pending Seek(position=%" GST_TIME_FORMAT ", ticket=%d)", GST_TIME_ARGS(pending_seek_pos), ticket);
            Seek(static_cast<int64_t>(pending_seek_pos / GST_USECOND), ticket);
          }
        }
      }
    } break;

    case GST_MESSAGE_ASYNC_DONE: {
      if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline_)) {
        GST_INFO_OBJECT(pipeline_, "===> ASYNC-DONE, pipeline state: %s, player state: %s",
                 gst_element_state_get_name(GST_STATE(pipeline_)),
                 PrivatePlayerStateToStr(state_));

        std::mutex &mutex = mutex_;
        std::lock_guard lock(mutex);

        if (state_ == State::kPrerollAfterSeek ||
            state_ == State::kInitialPreroll) {

          bool is_seek_pending = is_seek_pending_;
          bool has_pending_samples = (pending_samples_.empty() == false) || has_oob_write_pending_;

          if (!is_seek_pending && has_pending_samples) {

            int prev_has_data = static_cast<int>(has_enough_data_);
            has_enough_data_ = static_cast<int>(MediaType::kBoth);

            mutex.unlock();
            GST_INFO_OBJECT(pipeline_, "===> Writing pending samples");
            WritePendingSamples();
            mutex.lock();

            if (has_enough_data_ == static_cast<int>(MediaType::kBoth))
              has_enough_data_ = prev_has_data;
            has_oob_write_pending_ = false;
            pending_oob_write_condition_.notify_all();
          }
          GST_INFO_OBJECT(pipeline_, "===> Asuming preroll done");
        }

        SchedulePlayingStateUpdate();
        UpdatePresentingState();
      }
    } break;

    case GST_MESSAGE_CLOCK_LOST:
      ChangePipelineState(GST_STATE_PAUSED);
      ChangePipelineState(GST_STATE_PLAYING);
      break;

    case GST_MESSAGE_LATENCY:
      gst_bin_recalculate_latency(GST_BIN(pipeline_));
      break;

    case GST_MESSAGE_QOS: {
      const gchar *klass;
      klass = gst_element_class_get_metadata (
          GST_ELEMENT_GET_CLASS (GST_MESSAGE_SRC(message)),
          GST_ELEMENT_METADATA_KLASS);
      if (g_strrstr(klass, "Video")) {
        GstFormat format;
        guint64 dropped = 0, processed = 0;
        GstDebugLevel log_level = GST_LEVEL_DEBUG;
        gst_message_parse_qos_stats(message, &format, &processed, &dropped);
        if (format == GST_FORMAT_BUFFERS) {
          std::lock_guard lock(mutex_);
          if (dropped_video_frames_ != static_cast<int>(dropped)) {
            log_level = GST_LEVEL_INFO;
            dropped_video_frames_ = static_cast<int>(dropped);
          }
        }
        GST_CAT_LEVEL_LOG (
          GST_CAT_DEFAULT, log_level, pipeline_,
          "QOS written = %d, processed = %" G_GUINT64_FORMAT ", dropped = %" G_GUINT64_FORMAT,
          total_video_frames_, processed, dropped);
      }
    } break;

    default:
      break;
  }

  return TRUE;
}

// static
void* PlayerImpl::ThreadEntryPoint(void* context) {
  SbThreadSetPriority(kSbThreadPriorityRealTime);
  SB_DCHECK(context);
  GST_TRACE("%d", SbThreadGetId());

  PlayerImpl* self = reinterpret_cast<PlayerImpl*>(context);
  self->state_ = State::kInitial;

  g_main_context_push_thread_default(self->main_loop_context_);

  self->hang_monitor_.Reset();

  self->DispatchOnWorkerThread(new PlayerStatusTask(
      self->player_status_func_, self->player_, self->ticket_, self->context_,
      kSbPlayerStateInitialized));
  g_main_loop_run(self->main_loop_);

  return nullptr;
}

guint PlayerImpl::DispatchOnWorkerThread(Task* task) const {
  GSource* src = g_source_new(&SourceFunctions, sizeof(GSource));
  g_source_set_ready_time(src, 0);
  g_source_set_callback(src,
    [](gpointer userData) -> gboolean {
      auto* task = static_cast<Task*>(userData);
      GST_TRACE("%d", SbThreadGetId());
      task->PrintInfo();
      task->Do();
      return G_SOURCE_REMOVE;
    },
    task,
    [](gpointer userData) {
      delete static_cast<Task*>(userData);
    }
  );
  guint id = g_source_attach(src, main_loop_context_);
  g_source_unref(src);
  return id;
}

// static
gboolean PlayerImpl::FinishSourceSetup(gpointer user_data) {
  PlayerImpl* self = static_cast<PlayerImpl*>(user_data);
  std::lock_guard lock(self->source_setup_mutex_);
  SB_DCHECK(self->source_);
  bool has_drm_system = !!self->drm_system_;
  GstElement* source = self->source_;
  GstAppSrcCallbacks callbacks = {&PlayerImpl::AppSrcNeedData,
                                  &PlayerImpl::AppSrcEnoughData,
                                  &PlayerImpl::AppSrcSeekData, nullptr};
  if (self->audio_codec_ != kSbMediaAudioCodecNone) {
    gst_cobalt_src_setup_and_add_app_src(kSbMediaTypeAudio,
        source, self->audio_appsrc_,
        &callbacks, self, has_drm_system);
  }
  if (self->video_codec_ != kSbMediaVideoCodecNone) {
    gst_cobalt_src_setup_and_add_app_src(kSbMediaTypeVideo,
        source, self->video_appsrc_,
        &callbacks, self, has_drm_system);
  }
  gst_cobalt_src_all_app_srcs_added(self->source_);
  self->source_setup_id_ = 0;
  return G_SOURCE_REMOVE;
}

// static
void PlayerImpl::AppSrcNeedData(GstAppSrc* src,
                                guint length,
                                gpointer user_data) {
  PlayerImpl* self = static_cast<PlayerImpl*>(user_data);

  GST_LOG_OBJECT(src, "===> Gimme more data");

  std::lock_guard lock(self->mutex_);
  int need_data = static_cast<int>(MediaType::kNone);
  SB_DCHECK(src == GST_APP_SRC(self->video_appsrc_) ||
         src == GST_APP_SRC(self->audio_appsrc_));
  if (src == GST_APP_SRC(self->video_appsrc_)) {
    self->has_enough_data_ &= ~static_cast<int>(MediaType::kVideo);
    need_data |= static_cast<int>(MediaType::kVideo);
  } else if (src == GST_APP_SRC(self->audio_appsrc_)) {
    self->has_enough_data_ &= ~static_cast<int>(MediaType::kAudio);
    need_data |= static_cast<int>(MediaType::kAudio);
  }

  if (self->state_ == State::kPrerollAfterSeek) {
    if (self->has_enough_data_ != static_cast<int>(MediaType::kNone)) {
      GST_LOG_OBJECT(src, "Seeking. Waiting for other appsrcs.");
      return;
    }

    need_data =
        static_cast<int>(self->GetBothMediaTypeTakingCodecsIntoAccount());
  }

  GST_LOG_OBJECT(src, "===> Really. Gimme more data need:%d", need_data);
  self->DecoderNeedsData(static_cast<MediaType>(need_data));
}

// static
void PlayerImpl::AppSrcEnoughData(GstAppSrc* src, gpointer user_data) {
  PlayerImpl* self = static_cast<PlayerImpl*>(user_data);

  std::lock_guard lock(self->mutex_);

  if (src == GST_APP_SRC(self->video_appsrc_))
    self->has_enough_data_ |= static_cast<int>(MediaType::kVideo);
  else if (src == GST_APP_SRC(self->audio_appsrc_))
    self->has_enough_data_ |= static_cast<int>(MediaType::kAudio);

  GST_DEBUG_OBJECT(src, "===> Enough is enough (enough:%d)",
                   self->has_enough_data_);
}

// static
gboolean PlayerImpl::AppSrcSeekData(GstAppSrc* src,
                                    guint64 offset,
                                    gpointer user_data) {
  PlayerImpl* self = static_cast<PlayerImpl*>(user_data);
  GST_DEBUG_OBJECT(src, "===> Seek on appsrc %" PRId64, offset);

  {
    std::lock_guard lock(self->mutex_);
    if (self->state_ != State::kPrerollAfterSeek) {
      GST_DEBUG_OBJECT(src, "Not seeking");
      return TRUE;
    }
  }

  PlayerImpl::AppSrcEnoughData(src, user_data);
  return TRUE;
}

// static
void PlayerImpl::SetupSource(GstElement* pipeline,
                             GstElement* source,
                             PlayerImpl* self) {
  std::lock_guard lock(self->source_setup_mutex_);
  if (self->source_)
    return;
  self->source_ = source;
  static constexpr int kAsyncSourceFinishTimeMs = 50;
  GSource* src = g_timeout_source_new(kAsyncSourceFinishTimeMs);
  g_source_set_callback(src, &PlayerImpl::FinishSourceSetup, self, nullptr);
  self->source_setup_id_ = g_source_attach(src, self->main_loop_context_);
  g_source_unref(src);
}

// static
void PlayerImpl::OnVideoBufferUnderflow(PlayerImpl* self)
{
  GST_WARNING_OBJECT(self->pipeline_,
    "Decoder need data state = 0x%x,"
    " video appsrc level = %" G_GUINT64_FORMAT " kb,"
    " audio appsrc level = %" G_GUINT64_FORMAT " kb",
    self->decoder_state_data_,
    gst_app_src_get_current_level_bytes(GST_APP_SRC(self->video_appsrc_)) / 1024,
    gst_app_src_get_current_level_bytes(GST_APP_SRC(self->audio_appsrc_)) / 1024);
}

// static
void PlayerImpl::SetupElement(GstElement* pipeline,
                              GstElement* element,
                              PlayerImpl* self) {
  if (GST_IS_BASE_SINK(element)) {
    static bool disable_wait_video = !!getenv("COBALT_AML_DISABLE_WAIT_VIDEO");
    bool has_video = (self->video_codec_ != kSbMediaVideoCodecNone);
    if (has_video && g_str_has_prefix(GST_ELEMENT_NAME(element), "amlhalasink") && !disable_wait_video) {
      g_object_set(element, "wait-video", TRUE, "a-wait-timeout", 4000, "disable-xrun", TRUE, nullptr);
    }
    else
    if (has_video && g_str_has_prefix(GST_ELEMENT_NAME(element), "westerossink")) {
      if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "zoom-mode")) {
        GST_INFO_OBJECT(pipeline, "Setting westerossink zoom-mode to 0");
        g_object_set(element, "zoom-mode", 0, nullptr);
      }
      g_signal_connect_swapped(
        G_OBJECT(element), "buffer-underflow-callback",
        G_CALLBACK(OnVideoBufferUnderflow), self);
    }
    else
    if (g_str_has_prefix(GST_ELEMENT_NAME(element), "brcmaudiosink")) {
      g_object_set(G_OBJECT(element), "async", TRUE, nullptr);
    }
  }

  if (GST_IS_BASE_PARSE(element)) {
    gst_base_parse_set_pts_interpolation(GST_BASE_PARSE(element), FALSE);
  }

  const gchar *klass_str = gst_element_class_get_metadata(GST_ELEMENT_GET_CLASS(element), "klass");
  if (strstr(klass_str, "Sink")) {
    GObjectClass *oclass = G_OBJECT_GET_CLASS(element);

    if (strstr(klass_str, "Video")) {
      if (!self->max_video_capabilities_.empty() &&
          g_object_class_find_property(oclass, "maxVideoWidth") &&
          g_object_class_find_property(oclass, "maxVideoHeight")) {
            uint32_t width = 0, height = 0;
            ParseMaxVideoCapabilities(self->max_video_capabilities_.c_str(), &width, &height, nullptr);
            g_object_set(G_OBJECT(element), "maxVideoWidth", width, "maxVideoHeight", height, nullptr);
        }
    }

    if (g_object_class_find_property(oclass, "has-drm")) {
      g_object_set(G_OBJECT(element), "has-drm", self->drm_system_ != nullptr, nullptr);
    }
  }
}

void PlayerImpl::MarkEOS(SbMediaType stream_type) {
  GstElement* src = nullptr;
  if (stream_type == kSbMediaTypeVideo) {
    src = video_appsrc_;
  } else {
    src = audio_appsrc_;
  }

  GST_INFO_OBJECT(src, "===> %d", SbThreadGetId());
  std::lock_guard lock(mutex_);
  if (state_ == State::kPrerollAfterSeek)
    GST_DEBUG_OBJECT(src, "===> Mark EOS with State::kPrerollAfterSeek");

  if (stream_type == kSbMediaTypeVideo)
      eos_data_ |= static_cast<int>(MediaType::kVideo);
  else
      eos_data_ |= static_cast<int>(MediaType::kAudio);

  if (eos_data_ == static_cast<int>(GetBothMediaTypeTakingCodecsIntoAccount())) {
    if (audio_codec_ != kSbMediaAudioCodecNone)
      gst_app_src_end_of_stream(GST_APP_SRC(audio_appsrc_));
    if (video_codec_ != kSbMediaVideoCodecNone)
      gst_app_src_end_of_stream(GST_APP_SRC(video_appsrc_));

    SchedulePlayingStateUpdate();
  }
}

bool PlayerImpl::WriteSample(SbMediaType sample_type, GstBuffer* buffer, uint64_t serial_id) {
  GstElement* src = nullptr;
  if (sample_type == kSbMediaTypeVideo) {
    src = video_appsrc_;
  } else {
    src = audio_appsrc_;
  }

  GstDebugLevel log_level = GST_LEVEL_TRACE;
  {
    std::lock_guard lock(mutex_);
    if (sample_type == kSbMediaTypeVideo)
      decoder_state_data_ &= ~static_cast<int>(MediaType::kVideo);
    else
      decoder_state_data_ &= ~static_cast<int>(MediaType::kAudio);

    if (state_ < State::kPresenting)
      log_level = GST_LEVEL_DEBUG;
  }

  GST_CAT_LEVEL_LOG (
    GST_CAT_DEFAULT, log_level, src,
    "SampleType:%d %" GST_TIME_FORMAT " id:%" PRIu64 " b:%p",
    sample_type, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)), serial_id, buffer);

  gst_app_src_push_buffer(GST_APP_SRC(src), buffer);

  std::lock_guard lock(mutex_);
  // Wait for need-data to trigger instead.
  if (state_ == State::kInitial || state_ == State::kInitialPreroll)
    return true;

  MediaType media = sample_type == kSbMediaTypeVideo
    ? MediaType::kVideo
    : MediaType::kAudio;

  bool has_enough =
      (sample_type == kSbMediaTypeVideo &&
       (has_enough_data_ & static_cast<int>(MediaType::kVideo)) != 0) ||
      (sample_type == kSbMediaTypeAudio &&
       (has_enough_data_ & static_cast<int>(MediaType::kAudio)) != 0);

  bool force_buf = has_enough &&
      (GST_CLOCK_TIME_IS_VALID(buf_target_min_ts_) &&
       min_sample_timestamp_origin_ == media);

  if (!has_enough || force_buf) {
    GST_LOG_OBJECT(src, "Asking for more (forced buffering? %s)", force_buf ? "yes" : "no");
    DecoderNeedsData(media);
  } else {
    GST_LOG_OBJECT(src, "Has enough data");
  }

  return true;
}

void PlayerImpl::WriteSample(SbMediaType sample_type,
                             const SbPlayerSampleInfo* sample_infos,
                             int number_of_sample_infos) {
  static_assert(kMaxNumberOfSamplesPerWrite == 1,
                "Adjust impl. to handle more samples after changing samples"
                "count");
  SB_DCHECK(number_of_sample_infos == kMaxNumberOfSamplesPerWrite);
  g_return_if_fail(sample_infos[0].buffer_size > 0);
  // For debuggin purposes it could be usefull to disable audio or video
  // in this case just drop the sample
  if (audio_codec_ == kSbMediaAudioCodecNone && sample_type == kSbMediaTypeAudio) {
    sample_deallocate_func_(player_, context_, sample_infos[0].buffer);
    return;
  }
  if (video_codec_ == kSbMediaVideoCodecNone && sample_type == kSbMediaTypeVideo) {
    sample_deallocate_func_(player_, context_, sample_infos[0].buffer);
    return;
  }

  gsize buffer_size = static_cast<gsize>(sample_infos[0].buffer_size);
#if defined(SB_RDK_ZERO_COPY_SAMPLE_WRITE) && SB_RDK_ZERO_COPY_SAMPLE_WRITE
  using BufferInfo = std::tuple<SbPlayerDeallocateSampleFunc, SbPlayer, void*, const void*>;
  GstBuffer* buffer =
    gst_buffer_new_wrapped_full(
      static_cast<GstMemoryFlags>(0),
      const_cast<gpointer> (sample_infos[0].buffer),
      buffer_size,
      0,
      buffer_size,
      new BufferInfo(sample_deallocate_func_, player_, context_, sample_infos[0].buffer),
      [](gpointer data) {
        BufferInfo &info = *reinterpret_cast<BufferInfo*>(data);
        auto deallocate_func = std::get<0>(info);
        auto player = std::get<1>(info);
        auto* context = std::get<2>(info);
        const auto* buffer = std::get<3>(info);
        deallocate_func(player, context, buffer);
        delete &info;
      });
#else
  G_GNUC_UNUSED gsize sz;
  GstBuffer* buffer =
    gst_buffer_new_allocate(nullptr, buffer_size, nullptr);
  sz = gst_buffer_fill(buffer, 0, sample_infos[0].buffer, buffer_size);
  SB_DCHECK(sz == buffer_size);
  sample_deallocate_func_(player_, context_, sample_infos[0].buffer);
#endif

  int64_t sample_timestamp = sample_infos[0].timestamp;
  if (sample_timestamp < 0) {
    // FIXME: figure out how to handle negative timestamps properly
    GST_WARNING_OBJECT(pipeline_, "Negative timestamp %" G_GINT64_FORMAT ", reseting to 0", sample_timestamp);
    sample_timestamp = 0;
  }

  GstClockTime timestamp = static_cast<GstClockTime>(sample_timestamp * GST_USECOND);
  GST_BUFFER_TIMESTAMP(buffer) = timestamp;

  if (sample_infos[0].type == kSbMediaTypeVideo) {
    const auto& info = sample_infos[0].video_sample_info.stream_info;
    if (frame_width_ != info.frame_width ||
        frame_height_ != info.frame_height ||
        CompareColorMetadata(color_metadata_, info.color_metadata) != 0) {
      frame_width_ = info.frame_width;
      frame_height_ = info.frame_height;
      color_metadata_ = info.color_metadata;
      GstCaps* gst_caps = CodecToGstCaps(video_codec_);
      if (gst_caps) {
        AddVideoInfoToGstCaps(info, gst_caps);
        PrintGstCaps(gst_caps);
        gst_app_src_set_caps(GST_APP_SRC(video_appsrc_), gst_caps);
        gst_caps_replace(&video_caps_, gst_caps);
        gst_caps_unref(gst_caps);
      }
    }
    if (!sample_infos[0].video_sample_info.is_key_frame) {
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    }
  }
  else if (sample_infos[0].type == kSbMediaTypeAudio) {
    SB_DCHECK (sample_infos[0].type == kSbMediaTypeAudio);
    SB_DCHECK (audio_codec_ != kSbMediaAudioCodecNone);

    if ( audio_caps_ == nullptr ) {
      const auto& audio_info = sample_infos[0].audio_sample_info.stream_info;
      GstCaps* gst_caps = CodecToGstCaps(audio_codec_, &audio_info);
      if (gst_caps) {
        PrintGstCaps(gst_caps);
        gst_app_src_set_caps(GST_APP_SRC(audio_appsrc_), gst_caps);
        if (audio_codec_ == kSbMediaAudioCodecVorbis || audio_codec_ == kSbMediaAudioCodecFlac)
          PushStreamHeaders(GST_APP_SRC(audio_appsrc_), gst_caps);
        gst_caps_replace(&audio_caps_, gst_caps);
        gst_caps_unref(gst_caps);
      }
    }

    const guint64 kMaxGstClockTime = G_MAXUINT64 / G_GUINT64_CONSTANT (2);
    const auto& info = sample_infos[0].audio_sample_info;
    guint64 start_clip = 0, end_clip = 0;

    if (info.discarded_duration_from_front > 0)
      start_clip = (info.discarded_duration_from_front == std::numeric_limits<int64_t>::max())
        ? kMaxGstClockTime : static_cast<guint64>(info.discarded_duration_from_front * GST_USECOND);

    if (info.discarded_duration_from_back > 0)
      end_clip = (info.discarded_duration_from_back == std::numeric_limits<int64_t>::max())
        ? kMaxGstClockTime : static_cast<guint64>(info.discarded_duration_from_back * GST_USECOND);

    if (start_clip || end_clip) {
      gst_buffer_add_audio_clipping_meta(buffer, GST_FORMAT_TIME, start_clip, end_clip);
      GST_DEBUG_OBJECT(pipeline_, "Add audio clipping, start: %" PRIu64 ", end %" PRIu64, start_clip, end_clip);
    }
  }

  RecordTimestamp(sample_type, timestamp);

  if (sample_infos[0].drm_info) {
    GST_LOG_OBJECT(pipeline_, "Encounterd encrypted %s sample",
            sample_type == kSbMediaTypeVideo ? "video" : "audio");
    SB_DCHECK(drm_system_);

    const auto& drm_info = sample_infos[0].drm_info;

    GstBuffer* subsamples = nullptr;
    GstBuffer* iv = nullptr;
    GstBuffer* key = nullptr;
    uint32_t subsamples_count = 0u;
    uint32_t iv_size = 0u;

    const int8_t kEmptyArray[kMaxIvSize / 2] = {0};
    const auto encryption_scheme = drm_info->encryption_scheme;
    const char* cipher_mode =
      (encryption_scheme == kSbDrmEncryptionSchemeAesCtr) ? "cenc" :
      (encryption_scheme == kSbDrmEncryptionSchemeAesCbc  ? "cbcs" : "unknown");
    uint32_t identifier_size = drm_info->identifier_size > 0
        ? static_cast<uint32_t>(drm_info->identifier_size)
        : 0;

    GST_LOG_OBJECT(pipeline_, "Encryption cipher-mode: %s", cipher_mode);

    key = gst_buffer_new_allocate(nullptr, identifier_size, nullptr);
    gst_buffer_fill(key, 0, drm_info->identifier, identifier_size);

    iv_size = drm_info->initialization_vector_size > 0
        ? static_cast<uint32_t>(drm_info->initialization_vector_size)
        : 0;
    if (iv_size == kMaxIvSize &&
        memcmp(drm_info->initialization_vector + kMaxIvSize / 2,
               kEmptyArray, kMaxIvSize / 2) == 0) {
      iv_size /= 2;
    }
    iv = gst_buffer_new_allocate(nullptr, iv_size, nullptr);
    gst_buffer_fill(iv, 0, drm_info->initialization_vector, iv_size);

    subsamples_count = drm_info->subsample_count > 0
        ? static_cast<uint32_t> (drm_info->subsample_count)
        : 0;
    if (subsamples_count) {
      auto subsamples_raw_size =
        subsamples_count * (sizeof(guint16) + sizeof(guint32));
      guint8* subsamples_raw =
        static_cast<guint8*>(g_malloc(subsamples_raw_size));
      GstByteWriter writer;
      gst_byte_writer_init_with_data(&writer, subsamples_raw, subsamples_raw_size, FALSE);
      for (uint32_t i = 0; i < subsamples_count; ++i) {
        uint16_t clear_byte_count = drm_info->subsample_mapping[i].clear_byte_count > 0
            ? static_cast<uint16_t>(drm_info->subsample_mapping[i].clear_byte_count)
            : 0;
        uint32_t encrypted_byte_count = drm_info->subsample_mapping[i].encrypted_byte_count > 0
            ? static_cast<uint32_t> (drm_info->subsample_mapping[i].encrypted_byte_count)
            : 0;
        if (!gst_byte_writer_put_uint16_be(&writer, clear_byte_count))
          GST_ERROR_OBJECT(pipeline_, "Failed writing clear subsample info at %d", i);
        if (!gst_byte_writer_put_uint32_be(&writer, encrypted_byte_count))
          GST_ERROR_OBJECT(pipeline_, "Failed writing encrypted subsample info at %d", i);
      }
      subsamples = gst_buffer_new_wrapped(subsamples_raw, subsamples_raw_size);
    }

    GstStructure* info = gst_structure_new (
      "application/x-cenc",
      "encrypted", G_TYPE_BOOLEAN, TRUE,
      "kid", GST_TYPE_BUFFER, key,
      "iv_size", G_TYPE_UINT, iv_size,
      "iv", GST_TYPE_BUFFER, iv,
      "subsample_count", G_TYPE_UINT, subsamples_count,
      "subsamples", GST_TYPE_BUFFER, subsamples,
      "cipher-mode", G_TYPE_STRING, cipher_mode,
      NULL);

    if (encryption_scheme == kSbDrmEncryptionSchemeAesCbc) {
      const auto& encryption_pattern = drm_info->encryption_pattern;
      gst_structure_set(info,
        "crypt_byte_block", G_TYPE_UINT, encryption_pattern.crypt_byte_block,
        "skip_byte_block", G_TYPE_UINT, encryption_pattern.skip_byte_block,
        NULL);
    }

    gst_buffer_add_protection_meta(buffer, info);

    gst_buffer_unref(iv);
    gst_buffer_unref(key);
    gst_buffer_unref(subsamples);
  } else {
    GST_LOG_OBJECT(pipeline_, "Encounterd clear %s sample",
            sample_type == kSbMediaTypeVideo ? "video" : "audio");
  }

  GstClockTime seek_pos_ns = GST_CLOCK_TIME_NONE;
  uint64_t serial = 0;
  bool keep_samples = false;
  {
    std::lock_guard lock(mutex_);
    keep_samples = is_seek_pending_;
    serial = samples_serial_[ (sample_type == kSbMediaTypeVideo ? kVideoIndex : kAudioIndex) ]++;
    if (sample_type == kSbMediaTypeVideo)
      ++total_video_frames_;
    if (GST_CLOCK_TIME_IS_VALID(seek_position_))
        seek_pos_ns =  seek_position_;
  }

  if (GST_CLOCK_TIME_IS_VALID(seek_pos_ns) && seek_pos_ns > GST_BUFFER_TIMESTAMP(buffer)) {
    // Set dummy duration to let sink drop out-of-segment samples
    GST_BUFFER_DURATION (buffer) = GST_SECOND / 60;
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DECODE_ONLY);
  }

  if (keep_samples) {
    GST_INFO_OBJECT(pipeline_, "Pending flushing operation. Storing sample");
    GST_INFO_OBJECT(pipeline_, "SampleType:%d %" GST_TIME_FORMAT " id:%" PRIu64 " b:%" GST_PTR_FORMAT,
             sample_type, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)), serial, buffer);
    PendingSample sample(sample_type, buffer, serial);
    buffer= nullptr;
    std::lock_guard lock(mutex_);
    pending_samples_.emplace_back(std::move(sample));
  }

  {
    // Let other thread finish writing
    std::unique_lock lock(mutex_);
    while(has_oob_write_pending_) {
      constexpr auto kWaitTime = 10'000'000;  // 10 seconds
      if (pending_oob_write_condition_.wait_for(lock, std::chrono::microseconds(kWaitTime)) == std::cv_status::timeout) {
        GST_ERROR_OBJECT(pipeline_, "Pending write took too long, give up");
        has_oob_write_pending_ = false;
        break;
      }
    }
  }

  if (keep_samples) {
    PendingSamples local_samples;
    {
      std::lock_guard lock(mutex_);
      local_samples.swap(pending_samples_);
    }

    if(local_samples.empty()) {
      GST_WARNING_OBJECT(pipeline_, "No pending samples");
      return;
    }

    auto& sample = local_samples.back();

    SB_CHECK(sample.Type() == sample_type);

    if (serial != sample.SerialID()) {
      GST_WARNING_OBJECT(pipeline_, "Detected out-of-order sample. Expected serial: %" PRIu64 ", sample serial: %" PRIu64 "",
                  serial, sample.SerialID());
    }

    GstBuffer* buffer_copy = sample.CopyBuffer();
    if (!WriteSample(sample.Type(), buffer_copy, sample.SerialID())) {
      gst_buffer_unref(buffer_copy);
    }

    {
      std::lock_guard lock(mutex_);
      std::move(local_samples.begin(), local_samples.end(),
                std::back_inserter(pending_samples_));
    }
  } else {
    if (!WriteSample(sample_type, buffer, serial)) {
      gst_buffer_unref(buffer);
    }
  }

  GST_LOG_OBJECT(pipeline_,"Wrote sample.");
}

void PlayerImpl::SetVolume(double volume) {
  GST_DEBUG_OBJECT(pipeline_, "volume %lf, TID %d", volume, SbThreadGetId());
  gst_stream_volume_set_volume(GST_STREAM_VOLUME(pipeline_),
                               GST_STREAM_VOLUME_FORMAT_LINEAR, volume);
}

void PlayerImpl::HandleInititialSeek() {
  SB_DCHECK(state_ == State::kInitial || GST_STATE(pipeline_) < GST_STATE_PAUSED);

  if (state_ == State::kInitial) {
    // This is the initial seek to 0 which will trigger data pumping.
    SB_DCHECK(seek_position_ == .0);
    AddBufferingProbe(0, ticket_);
    state_ = State::kInitialPreroll;
    DispatchOnWorkerThread(
      new PlayerStatusTask(player_status_func_, player_,
                           ticket_, context_,
                           kSbPlayerStatePrerolling));
    seek_position_ = GST_CLOCK_TIME_NONE;
    if (GST_STATE(pipeline_) < GST_STATE_PAUSED &&
        GST_STATE_PENDING(pipeline_) < GST_STATE_PAUSED) {
      mutex_.unlock();
      ChangePipelineState(GST_STATE_PAUSED);
      mutex_.lock();
    }
    return;
  }

  // Ask for data.
  if (state_ == State::kInitialPreroll) {
    MediaType need_data = GetBothMediaTypeTakingCodecsIntoAccount();
    DecoderNeedsData(need_data);
  }

#if GST_CHECK_VERSION(1, 18, 0)
  // It is safe to replace the segment before app writes any data.
  bool can_change_segment = !audio_caps_ && !video_caps_ && !isRialtoEnabled();
  if (can_change_segment) {
    GST_INFO_OBJECT(pipeline_, "Changing initial segment.");
    GstClockTime position = seek_position_;
    GstSegment segment;
    gst_segment_init (&segment, GST_FORMAT_TIME);
    if ( !gst_segment_do_seek(&segment, !rate_ ? 1.0 : rate_,
                              GST_FORMAT_TIME, GST_SEEK_FLAG_ACCURATE,
                              GST_SEEK_TYPE_SET, position,
                              GST_SEEK_TYPE_NONE, 0, nullptr) ) {
      GST_WARNING_OBJECT(pipeline_, "Segment seek failed.");
    }
    else if ( !gst_base_src_new_segment(GST_BASE_SRC(audio_appsrc_), &segment) ) {
      GST_WARNING_OBJECT(pipeline_, "Could not change audio source segment.");
    }
    else if ( !gst_base_src_new_segment(GST_BASE_SRC(video_appsrc_), &segment) ) {
      GST_WARNING_OBJECT(pipeline_, "Could not change video source segment.");
    }
    else {
      DispatchOnWorkerThread(new PlayerStatusTask(player_status_func_, player_, ticket_,
                                                  context_, kSbPlayerStatePrerolling));
      AddBufferingProbe(position, ticket_);
      GST_INFO_OBJECT(pipeline_, "Successfully changed initial segment, position: %" GST_TIME_FORMAT ", ticket: %d", GST_TIME_ARGS(position), ticket_);
      return;
    }
  }
#endif

  // Else send seek after pre-roll.
  GST_INFO_OBJECT(pipeline_, "Delaying seek.");
  is_seek_pending_ = true;
}

void PlayerImpl::Seek(int64_t seek_to_timestamp, int ticket) {
  g_return_if_fail(seek_to_timestamp >= 0);

  std::lock_guard lock(seek_mutex_);
  GstClockTime current_pos_ns = GetPosition();
  GstClockTime seek_to_time_ns = static_cast<GstClockTime>(seek_to_timestamp * GST_USECOND);
  double rate = 1.;
  GST_INFO_OBJECT(pipeline_, "===> time %" PRId64 " (target=%" GST_TIME_FORMAT ", curr=%" GST_TIME_FORMAT ") TID: %d state %d, ticket = %d",
                  seek_to_timestamp,
                  GST_TIME_ARGS(seek_to_time_ns),
                  GST_TIME_ARGS(current_pos_ns),
                  SbThreadGetId(),
                  static_cast<int>(state_),
                  ticket);
  {
    std::lock_guard lock(mutex_);
    if (ticket_ > ticket) {
      GST_INFO_OBJECT(pipeline_, "Ignore seek with ticket: %d (stored ticket: %d)", ticket, ticket_);
      return;
    }

    if (ticket_ != ticket) {
      pending_samples_.clear();
      max_sample_timestamps_[kVideoIndex] = 0;
      max_sample_timestamps_[kAudioIndex] = 0;
      min_sample_timestamp_ = GST_CLOCK_TIME_NONE;
      samples_serial_[kVideoIndex] = 0;
      samples_serial_[kAudioIndex] = 0;
      buf_target_min_ts_ = GST_CLOCK_TIME_NONE;
      dropped_video_frames_ = 0;
      total_video_frames_ = 0;
    }

    ticket_ = ticket;
    seek_position_ = seek_to_time_ns;
    decoder_state_data_ = 0;
    eos_data_ = 0;
    is_seek_pending_ = false;

    rate = rate_;

    if (state_ == State::kInitial || GST_STATE(pipeline_) < GST_STATE_PAUSED) {
      HandleInititialSeek();
      return;
    }
  }

  GST_DEBUG_OBJECT(pipeline_, "Calling seek");
  DispatchOnWorkerThread(new PlayerStatusTask(player_status_func_, player_,
                                              ticket_, context_,
                                              kSbPlayerStatePrerolling));
  if (!gst_element_seek(pipeline_, !rate ? 1.0 : rate, GST_FORMAT_TIME,
                        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                                  GST_SEEK_FLAG_ACCURATE),
                        GST_SEEK_TYPE_SET,
                        seek_to_timestamp * GST_USECOND,
                        GST_SEEK_TYPE_NONE, 0)) {
    GST_ERROR_OBJECT(pipeline_, "Seek failed");
    buffering_state_ = 0;
    DispatchOnWorkerThread(new PlayerStatusTask(player_status_func_, player_,
                                                ticket_, context_,
                                                kSbPlayerStatePresenting));
    DispatchOnWorkerThread(new FunctionTask([this]() {
      state_ = State::kPresenting;
    }, "Presenting after seek failure"));
  } else {
    GST_DEBUG_OBJECT(pipeline_, "Seek called with success");
    DispatchOnWorkerThread(new FunctionTask([this]() {
      state_ = State::kPrerollAfterSeek;
    }, "Preroll after seek"));
    AddBufferingProbe(seek_to_time_ns, ticket);
  }
}

bool PlayerImpl::SetRate(double rate) {
  GST_INFO_OBJECT(pipeline_, "===> rate %lf (rate_ %lf), TID: %d", rate, rate_,
                   SbThreadGetId());

  GstState state;
  double old_rate;
  bool success = true;

  mutex_.lock();
  old_rate = rate_;
  rate_ = rate;
  pending_rate_ = .0;

  if (rate == .0) {
    mutex_.unlock();
    ChangePipelineState(GST_STATE_PAUSED);
    return true;
  }

  gst_element_get_state(pipeline_, &state, nullptr, 0);

  if (state < GST_STATE_PLAYING)
    SchedulePlayingStateUpdate();

  if (rate != 1. || need_instant_rate_change_) {
    if (is_seek_pending_) {
      mutex_.unlock();
      GST_DEBUG_OBJECT(pipeline_, "Rate will be set when doing seek");
      return true;
    }
    if (state < GST_STATE_PLAYING || need_first_segment_ack_) {
      rate_ = old_rate;
      pending_rate_ = rate;
      mutex_.unlock();
      GST_DEBUG_OBJECT(pipeline_, "===> Set rate postponed");
      return true;
    }
    need_instant_rate_change_ = ( rate != 1. );
    mutex_.unlock();

    GstStructure* s = gst_structure_new(
        kCustomInstantRateChangeEventName, "rate", G_TYPE_DOUBLE, rate, NULL);
    success = gst_element_send_event(
        pipeline_, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, s));

    mutex_.lock();
  }

  if (!success) {
    rate_ = old_rate;
    GST_ERROR_OBJECT(pipeline_, "Set rate failed");
  }
  mutex_.unlock();

  return success;
}

void PlayerImpl::GetInfo(SbPlayerInfo* out_player_info) {

  GstClockTime position = GetPosition();

  CheckBuffering(position);

  GST_LOG_OBJECT(
    pipeline_,"Current position: %" GST_TIME_FORMAT " (Seek position: %" GST_TIME_FORMAT ")",
    GST_TIME_ARGS(position),
    GST_TIME_ARGS(seek_position_));

  out_player_info->duration = SB_PLAYER_NO_DURATION;
  out_player_info->current_media_timestamp =
      GST_CLOCK_TIME_IS_VALID(position)
          ? static_cast<int64_t>(position / GST_USECOND)
          : 0;

  out_player_info->frame_width = frame_width_;
  out_player_info->frame_height = frame_height_;
  out_player_info->is_paused = GST_STATE(pipeline_) != GST_STATE_PLAYING;
  out_player_info->volume = gst_stream_volume_get_volume(
      GST_STREAM_VOLUME(pipeline_), GST_STREAM_VOLUME_FORMAT_LINEAR);
  out_player_info->total_video_frames = total_video_frames_;
  out_player_info->corrupted_video_frames = 0;

  {
    std::lock_guard lock(mutex_);
    out_player_info->dropped_video_frames = dropped_video_frames_;
  }

  GST_LOG_OBJECT(
    pipeline_,
    "Frames dropped: %d, Frames corrupted: %d",
    out_player_info->dropped_video_frames,
    out_player_info->corrupted_video_frames);

  out_player_info->playback_rate = rate_;
}

void PlayerImpl::SetBounds(int zindex, int x, int y, int w, int h) {
  GST_TRACE_OBJECT(pipeline_, "Set Bounds: %d %d %d %d %d", zindex, x, y, w, h);
  GstElement* vid_sink = nullptr;
  g_object_get(pipeline_, "video-sink", &vid_sink, nullptr);
  if (vid_sink && g_object_class_find_property(G_OBJECT_GET_CLASS(vid_sink),
                                               "rectangle")) {
    gchar* rect = g_strdup_printf("%d,%d,%d,%d", x, y, w, h);
    g_object_set(vid_sink, "rectangle", rect, nullptr);
    free(rect);
  } else {
    pending_bounds_ = PendingBounds{x, y, w, h};
  }
  if (vid_sink)
    gst_object_unref(GST_OBJECT(vid_sink));
}

bool PlayerImpl::ChangePipelineState(GstState state) {
  if (force_stop_ && state > GST_STATE_READY) {
    GST_INFO_OBJECT(pipeline_, "Ignore state change due to forced stop");
    return false;
  }

  GstState current, pending;
  current = pending = GST_STATE_VOID_PENDING;
  gst_element_get_state(pipeline_, &current, &pending, 0);
  if ((current == state && pending == GST_STATE_VOID_PENDING) || pending == state) {
    GST_DEBUG_OBJECT(
      pipeline_, "Rejected state change to %s from %s with %s pending",
      gst_element_state_get_name(state),
      gst_element_state_get_name(current),
      gst_element_state_get_name(pending));
    return true;
  }

  if (state == GST_STATE_PLAYING) {
    GstClockTime seek_pos_ns = GST_CLOCK_TIME_NONE;
    GstClockTime min_ts = GST_CLOCK_TIME_NONE;
    {
      std::unique_lock lock(mutex_);
      if (GST_CLOCK_TIME_IS_VALID(seek_position_)) {
        seek_pos_ns =  seek_position_;
        min_ts = MinTimestamp(nullptr);
      }
    }

    if (GST_CLOCK_TIME_IS_VALID(seek_pos_ns)) {
      if (!GST_CLOCK_TIME_IS_VALID(min_ts)) {
        GST_INFO_OBJECT(pipeline_, "Ignore state change to playing: invalid min sample ts");
        return false;
      }
      else if (seek_pos_ns > min_ts) {
        GST_INFO_OBJECT(
          pipeline_,
          "Ignore state change to playing: no samples for seek time yet"
          "(seek time: %" GST_TIME_FORMAT ", min sample time: %" GST_TIME_FORMAT ")",
          GST_TIME_ARGS(seek_pos_ns), GST_TIME_ARGS(min_ts));
        return false;
      }
    }
  }
  else {
    std::lock_guard lock(mutex_);
    guint src_id = std::exchange(playing_state_update_source_id_, 0u);
    if (src_id != 0u) {
      GSource* src = g_main_context_find_source_by_id(main_loop_context_, src_id);
      g_source_destroy(src);
    }
  }
  GST_INFO_OBJECT(pipeline_, "Changing state to %s",
                   gst_element_state_get_name(state));
  bool result = gst_element_set_state(pipeline_, state) != GST_STATE_CHANGE_FAILURE;
  if (!result) {
    GST_ERROR_OBJECT(
      pipeline_, "Failed to change pipeline state to %s from %s with %s pending",
      gst_element_state_get_name(state),
      gst_element_state_get_name(current),
      gst_element_state_get_name(pending));
  }
  return result;
}

void PlayerImpl::CheckBuffering(GstClockTime position) {
  if (!GST_CLOCK_TIME_IS_VALID(position))
    return;

  constexpr GstClockTimeDiff kMarginNs =
      50 * GST_MSECOND;

  MediaType origin = MediaType::kNone;
  GstClockTime min_ts = MinTimestamp(&origin);

  if (!GST_CLOCK_TIME_IS_VALID(min_ts))
    return;

  if (min_ts < position &&
      GST_STATE(pipeline_) == GST_STATE_PLAYING &&
      GST_STATE_PENDING(pipeline_) != GST_STATE_PAUSED &&
      eos_data_ == 0) {
    {
      std::lock_guard lock(mutex_);
      DecoderNeedsData(origin);
      buf_target_min_ts_ = position + kMarginNs;
    }
    PrintPositionPerSink(pipeline_, GST_LEVEL_INFO);
    GST_INFO_OBJECT(pipeline_, "Pause for buffering. Pos: %" GST_TIME_FORMAT
                ", min ts:%" GST_TIME_FORMAT, GST_TIME_ARGS(position), GST_TIME_ARGS(min_ts));
    ChangePipelineState(GST_STATE_PAUSED);
  } else if (GST_CLOCK_TIME_IS_VALID(buf_target_min_ts_) && (min_ts > buf_target_min_ts_ || eos_data_ == static_cast<int>(GetBothMediaTypeTakingCodecsIntoAccount()))) {
    double rate;
    GstClockTime buf_target_min_ts = buf_target_min_ts_;
    {
      std::lock_guard lock(mutex_);
      buf_target_min_ts_ = GST_CLOCK_TIME_NONE;
      rate = rate_;
    }
    GstState state, pending;
    gst_element_get_state(pipeline_, &state, &pending, 0);
    if (rate > .0 && state != GST_STATE_PLAYING && pending != GST_STATE_PLAYING) {
      GST_INFO_OBJECT(
        pipeline_, "Resuming playback. min_ts: %" GST_TIME_FORMAT ", buff traget: %" GST_TIME_FORMAT,
        GST_TIME_ARGS(min_ts), GST_TIME_ARGS(buf_target_min_ts));
      ChangePipelineState(GST_STATE_PLAYING);
    }
  }
}

GstClockTime PlayerImpl::GetPosition() {
  gint64 position = -1;

  GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);
  if (gst_element_query(pipeline_, query))
    gst_query_parse_position(query, 0, &position);
  gst_query_unref(query);

  {
    std::lock_guard lock(mutex_);
    double rate = rate_;

    if (!GST_CLOCK_TIME_IS_VALID(position)) {
      if (GST_CLOCK_TIME_IS_VALID(seek_position_))
        return seek_position_;
      if (GST_CLOCK_TIME_IS_VALID(cached_position_ns_))
        return cached_position_ns_;
      return 0;
    }

    if (GST_CLOCK_TIME_IS_VALID(seek_position_)) {
      if (GST_STATE(pipeline_) != GST_STATE_PLAYING)
        return seek_position_;

      if ((rate >= 0. && position <= seek_position_) ||
          (rate < 0. && position >= seek_position_)) {
        return seek_position_;
      }

      cached_position_ns_ = seek_position_;
      seek_position_ = GST_CLOCK_TIME_NONE;
    }
  }

  if (GST_CLOCK_TIME_IS_VALID(cached_position_ns_) &&
      std::abs(GST_CLOCK_DIFF(GstClockTime(position), cached_position_ns_)) > GST_SECOND) {
    PrintPositionPerSink(pipeline_, GST_LEVEL_WARNING);
    GST_WARNING_OBJECT(pipeline_, "Unexpected position! More than 1 second jump detected: "
                "%" GST_TIME_FORMAT " --> %" GST_TIME_FORMAT "",
                GST_TIME_ARGS(cached_position_ns_),
                GST_TIME_ARGS(position));
  }

  PrintPositionPerSink(pipeline_, GST_LEVEL_LOG);
  cached_position_ns_ = GstClockTime(position);
  return cached_position_ns_;
}

void PlayerImpl::WritePendingSamples() {
  PendingSamples local_samples;
  bool keep_samples = false;
  int ticket = -1;
  {
    std::lock_guard lock(mutex_);
    keep_samples = is_seek_pending_;
    ticket = ticket_;
    local_samples.swap(pending_samples_);
  }

  if (!local_samples.empty()) {
    std::sort(
      local_samples.begin(), local_samples.end(),
      [](const PendingSample& lhs, const PendingSample& rhs) -> bool {
        return lhs.SerialID() < rhs.SerialID();
      });
    GstClockTime prev_timestamps[kMediaNumber] = {GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE};
    for (auto& sample : local_samples) {
      auto &prev_ts = prev_timestamps[sample.Type() == kSbMediaTypeVideo ? kVideoIndex : kAudioIndex];

      if (prev_ts == sample.Timestamp()) {
        GST_WARNING_OBJECT(pipeline_, "Skipping %" GST_TIME_FORMAT ". Already written.",
                    GST_TIME_ARGS(prev_ts));
        continue;
      }

      GstBuffer* buffer = keep_samples ? sample.CopyBuffer() : sample.TakeBuffer();
      GST_INFO_OBJECT(pipeline_, "Writing pending: SampleType:%d id:%" PRIu64 " b:%" GST_PTR_FORMAT, sample.Type(), sample.SerialID(), buffer);
      prev_ts = GST_BUFFER_TIMESTAMP(buffer);
      if (WriteSample(sample.Type(), buffer, sample.SerialID())) {
        GST_INFO_OBJECT(pipeline_, "Pending sample was written.");
      } else {
        gst_buffer_unref(buffer);
      }
    }

    if (keep_samples) {
      {
        std::lock_guard lock(mutex_);
        if (ticket_ == ticket) {
          std::move(local_samples.begin(), local_samples.end(),
                    std::back_inserter(pending_samples_));
        } else {
          keep_samples = false;
        }
      }
      if (keep_samples) {
        GST_INFO_OBJECT(pipeline_, "Stored samples again.");
      } else {
        GST_INFO_OBJECT(pipeline_, "Seek ticket changed (%d -> %d), dropped local samples.", ticket, ticket_);
      }
    }
  }
}

MediaType PlayerImpl::GetBothMediaTypeTakingCodecsIntoAccount() const {
  SB_DCHECK(audio_codec_ != kSbMediaAudioCodecNone ||
            video_codec_ != kSbMediaVideoCodecNone);
  MediaType both_need_data = MediaType::kBoth;

  if (audio_codec_ == kSbMediaAudioCodecNone)
    both_need_data = MediaType::kVideo;

  if (video_codec_ == kSbMediaVideoCodecNone)
    both_need_data = MediaType::kAudio;

  return both_need_data;
}

void PlayerImpl::RecordTimestamp(SbMediaType type, GstClockTime timestamp) {
  if (type == kSbMediaTypeVideo) {
    max_sample_timestamps_[kVideoIndex] =
        std::max(max_sample_timestamps_[kVideoIndex], timestamp);
  } else if (type == kSbMediaTypeAudio) {
    max_sample_timestamps_[kAudioIndex] =
        std::max(max_sample_timestamps_[kAudioIndex], timestamp);
  }

  if (audio_codec_ == kSbMediaAudioCodecNone) {
    min_sample_timestamp_origin_ = MediaType::kVideo;
    min_sample_timestamp_ = max_sample_timestamps_[kVideoIndex];
  } else if (video_codec_ == kSbMediaVideoCodecNone) {
    min_sample_timestamp_origin_ = MediaType::kAudio;
    min_sample_timestamp_ = max_sample_timestamps_[kAudioIndex];
  } else {
    min_sample_timestamp_ = std::min(max_sample_timestamps_[kVideoIndex],
                                     max_sample_timestamps_[kAudioIndex]);
    if (min_sample_timestamp_ == max_sample_timestamps_[kVideoIndex])
      min_sample_timestamp_origin_ = MediaType::kVideo;
    else
      min_sample_timestamp_origin_ = MediaType::kAudio;
  }
}

GstClockTime PlayerImpl::MinTimestamp(MediaType* origin) const {
  if (origin)
    *origin = min_sample_timestamp_origin_;
  return min_sample_timestamp_;
}

void PlayerImpl::HandleApplicationMessage(GstBus* bus, GstMessage* message) {
  const GstStructure* structure = gst_message_get_structure(message);
  if (gst_structure_has_name(structure, "force-stop") && !force_stop_) {
    GST_INFO_OBJECT(pipeline_, "Received force STOP, pipeline = %p!!!", pipeline_);
    force_stop_ = true;
    ChangePipelineState(GST_STATE_READY);
    g_signal_handlers_disconnect_by_func(pipeline_, reinterpret_cast<gpointer>(&PlayerImpl::SetupSource), this);
    g_signal_handlers_disconnect_by_func(pipeline_, reinterpret_cast<gpointer>(&PlayerImpl::SetupElement), this);
    std::lock_guard lock(source_setup_mutex_);
    if (source_setup_id_ > 0) {
      GSource* src = g_main_context_find_source_by_id(main_loop_context_, source_setup_id_);
      g_source_destroy(src);
      source_setup_id_ = 0;
    }
  }
  else if (gst_structure_has_name(structure, kDidReceiveFirstSegmentMsgName)) {
    if (GST_MESSAGE_SRC(message) == GST_OBJECT(audio_appsrc_) || GST_MESSAGE_SRC(message) == GST_OBJECT(video_appsrc_)) {
      GST_INFO_OBJECT(pipeline_, "Received '%s' message from %" GST_PTR_FORMAT, kDidReceiveFirstSegmentMsgName, GST_MESSAGE_SRC(message));
      bool should_set_rate = false;
      double rate = 0.;
      auto type = GST_MESSAGE_SRC(message) == GST_OBJECT(audio_appsrc_) ? MediaType::kAudio : MediaType::kVideo;

      mutex_.lock();
      need_first_segment_ack_ &= ~ static_cast<int>(type);
      should_set_rate = (need_first_segment_ack_ == 0 && pending_rate_ != .0 && !is_seek_pending_);
      rate = pending_rate_;
      mutex_.unlock();

      if (should_set_rate) {
        GST_INFO_OBJECT(pipeline_, "Sending pending SetRate(rate=%lf)", rate);
        SetRate(rate);
      }
    }
  }
  else if (gst_structure_has_name(structure, kDidReachBufferingTargetMsgName)) {
    if (GST_MESSAGE_SRC(message) == GST_OBJECT(audio_appsrc_) || GST_MESSAGE_SRC(message) == GST_OBJECT(video_appsrc_)) {
      int ticket;
      if (gst_structure_get_int(structure, "ticket", &ticket)) {
        GST_INFO_OBJECT(pipeline_, "Received '%s' message from %" GST_PTR_FORMAT " with ticket %d", kDidReachBufferingTargetMsgName, GST_MESSAGE_SRC(message), ticket);
        auto type = GST_MESSAGE_SRC(message) == GST_OBJECT(audio_appsrc_) ? MediaType::kAudio : MediaType::kVideo;
        bool should_update_playing_state = false;
        std::lock_guard lock(mutex_);
        if (ticket == ticket_ && buffering_state_ != 0) {
          buffering_state_ &= ~(static_cast<int>(type));
          should_update_playing_state = (buffering_state_ == 0);
        }
        if (should_update_playing_state) {
          SchedulePlayingStateUpdate();
          UpdatePresentingState();
        }
      }
    }
  }
}

void PlayerImpl::UpdatePresentingState() {
  // Already reached Presenting/Ended
  if (state_ >= State::kPresenting)
    return;

  // Awaiting for buffering probes to report they got some data
  if (buffering_state_ != 0)
    return;

  // Awaiting for video sink preroll
  GstState state, pending;
  GstStateChangeReturn ret;
  ret = gst_element_get_state(pipeline_, &state, &pending, 0);
  if ((state < GST_STATE_PAUSED) || (pending == GST_STATE_PAUSED && ret == GST_STATE_CHANGE_ASYNC))
    return;

  GST_INFO_OBJECT(pipeline_, "Commit to State::kPresenting");

  DispatchOnWorkerThread(
    new PlayerStatusTask(
      player_status_func_, player_, ticket_,
      context_, kSbPlayerStatePresenting));

  state_ = State::kPresenting;
}

void PlayerImpl::ConfigureLimitedVideo() {
  if (!isRialtoEnabled()) {
    GstElementFactory* factory = gst_element_factory_find("westerossink");
    if (factory) {
      GstElement* video_sink = gst_element_factory_create(factory, nullptr);
      if (video_sink) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(video_sink), "res-usage")) {
          g_object_set(video_sink, "res-usage", 0x0u, nullptr);
        }
        else {
          GST_WARNING_OBJECT(pipeline_, "'westerossink' has no 'res-usage' property, secondary video may steal decoder");
        }
        g_object_set(pipeline_, "video-sink", video_sink, nullptr);
      }
      else {
        GST_DEBUG_OBJECT(pipeline_, "Failed to create 'westerossink'");
      }
      gst_object_unref(GST_OBJECT(factory));
    }

    GstContext* context = gst_context_new("erm", FALSE);
    GstStructure* context_structure = gst_context_writable_structure(context);
    gst_structure_set(context_structure, "res-usage", G_TYPE_UINT, 0x0u, nullptr);
    gst_element_set_context(GST_ELEMENT(pipeline_), context);
    gst_context_unref(context);

  }
  // enforce no audio
  audio_codec_ = kSbMediaAudioCodecNone;
}

void PlayerImpl::AddBufferingProbe(GstClockTime target, int ticket) {
  struct BufferingProbeData {
    GstClockTime target_time;
    int ticket;
  };

  auto add_probe = [](GstElement* element, GstClockTime target, int ticket, GstPadProbeCallback callback) -> gulong {
    BufferingProbeData* data = reinterpret_cast<BufferingProbeData*>(g_malloc0(sizeof (BufferingProbeData)));
    data->target_time = target;
    data->ticket = ticket;

    GstPad* pad = gst_element_get_static_pad(element, "src");
    GstPadProbeType probe_type = static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_FLUSH);
    gulong ret = gst_pad_add_probe (pad, probe_type, callback, data, g_free);
    GST_DEBUG_OBJECT(element,
      "Buffering probe added to %" GST_PTR_FORMAT ", target time: %" GST_TIME_FORMAT ", ticket: %d",
      pad, GST_TIME_ARGS(target), ticket);
    gst_object_unref(pad);

    return ret;
  };

  auto buffering_probe_callback = [](GstPad * pad, GstPadProbeInfo * info, gpointer user_data) -> GstPadProbeReturn {
    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_FLUSH)
      return GST_PAD_PROBE_REMOVE;

    SB_CHECK(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER);

    GstBuffer* buffer = gst_pad_probe_info_get_buffer(info);
    BufferingProbeData* data = reinterpret_cast<BufferingProbeData*>(user_data);

    GST_TRACE_OBJECT(pad, "Testing buffer: %" GST_PTR_FORMAT " for target time: %" GST_TIME_FORMAT " with ticket: %d", buffer, GST_TIME_ARGS(data->target_time), data->ticket);

    GstClockTime pts = GST_BUFFER_TIMESTAMP(buffer);
    GstClockTime duration = GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buffer)) ? GST_BUFFER_DURATION(buffer) : 0;

    if ( (pts + duration) >= data->target_time ) {
      GstObject* parent = gst_pad_get_parent(pad);
      if (!parent) {
        GST_WARNING_OBJECT(pad, "Pad(%" GST_PTR_FORMAT ") has no parent", pad);
        SB_DCHECK(parent);
        return GST_PAD_PROBE_REMOVE;
      }

      GST_DEBUG_OBJECT(parent, "Did reach target buffering time:%" GST_TIME_FORMAT ", ticket: %d, on pad: %" GST_PTR_FORMAT, GST_TIME_ARGS(data->target_time), data->ticket, pad);
      GstStructure* structure = gst_structure_new(kDidReachBufferingTargetMsgName, "ticket", G_TYPE_INT, data->ticket, nullptr);
      gst_element_post_message(GST_ELEMENT(parent), gst_message_new_application(parent, structure));
      gst_object_unref(parent);

      return GST_PAD_PROBE_REMOVE;
    }

    return GST_PAD_PROBE_OK;
  };

  buffering_state_ = 0;

  const GstClockTime kAudioBufferTarget = 100 * GST_MSECOND;
  const GstClockTime kVideoBufferTarget = 160 * GST_MSECOND;

  if (audio_appsrc_ && audio_codec_ != kSbMediaAudioCodecNone) {
    if (add_probe(audio_appsrc_, target + kAudioBufferTarget, ticket, buffering_probe_callback) != 0u)
      buffering_state_ |= static_cast<int>(MediaType::kAudio);
    else
      GST_WARNING_OBJECT(audio_appsrc_, "Could not add buffering probe!");
  }

  if (video_appsrc_ && video_codec_ != kSbMediaVideoCodecNone) {
    if (add_probe(video_appsrc_, target + kVideoBufferTarget, ticket, buffering_probe_callback) != 0u)
      buffering_state_ |= static_cast<int>(MediaType::kVideo);
    else
      GST_WARNING_OBJECT(video_appsrc_, "Could not add buffering probe!");
  }
}

void PlayerImpl::SchedulePlayingStateUpdate() {
  if (playing_state_update_source_id_ != 0u)  // already scheduled
    return;

  auto update_callback = [this]() {
    bool should_be_playing = false, can_play = false;
    std::string reason;

    {
      GstState state, pending;
      GstStateChangeReturn ret;
      bool need_preroll;
      guint src_id;

      ret = gst_element_get_state(pipeline_, &state, &pending, 0);
      need_preroll = (state <= GST_STATE_PAUSED && pending == GST_STATE_PAUSED && ret == GST_STATE_CHANGE_ASYNC);

      std::lock_guard lock(mutex_);
      src_id = std::exchange(playing_state_update_source_id_, 0u);
      should_be_playing = (rate_ || pending_rate_);
      // the update was canceled when src_id == 0
      can_play = (!need_preroll && src_id != 0u) &&
          ((buffering_state_ == 0 && !GST_CLOCK_TIME_IS_VALID(buf_target_min_ts_))
           || eos_data_ == static_cast<int>(GetBothMediaTypeTakingCodecsIntoAccount()));
      if (!can_play) {
        reason  = ", reason: ";
        reason += ([&]{
          if(!should_be_playing)
             return "paused";
          if (buffering_state_ != 0)
            return "buffering";
          if (need_preroll)
            return "awaiting preroll";
          if (src_id == 0)
            return "canceled";
          if (GST_CLOCK_TIME_IS_VALID(buf_target_min_ts_))
            return "min buffer ts is set";
          return "unknown";
        })();
      }
    }

    GST_INFO_OBJECT(pipeline_, "Update pipeline state, should be playing: %s, can_play: %s%s", should_be_playing ? "yes" : "no", can_play ? "yes" : "no", reason.c_str());

    if (should_be_playing && can_play)
      ChangePipelineState(GST_STATE_PLAYING);
  };

  playing_state_update_source_id_ =
    DispatchOnWorkerThread(new FunctionTask(std::move(update_callback), "Playing state update"));
}

void PlayerImpl::DidEnd() {
  if (state_ < State::kPresenting) {
    DispatchOnWorkerThread(
      new PlayerStatusTask(
        player_status_func_, player_, ticket_,
        context_, kSbPlayerStatePresenting));
    state_ = State::kPresenting;
  }

  if (state_ == State::kPresenting) {
    DispatchOnWorkerThread(
      new PlayerStatusTask(
        player_status_func_, player_, ticket_,
        context_, kSbPlayerStateEndOfStream));
    state_ = State::kEnded;
  }
}

void PlayerImpl::AudioConfigurationChanged() {
  GST_WARNING_OBJECT(pipeline_, "Emitting an error on audio configuration change.");
  DispatchOnWorkerThread(
    new PlayerErrorTask(
      player_error_func_, player_, context_,
      kSbPlayerErrorCapabilityChanged, "Audio device capability changed"));

}

}  // namespace

void ForceStop() {
  using third_party::starboard::rdk::shared::player::GetPlayerRegistry;
  GetPlayerRegistry()->ForceStop();
}

void AudioConfigurationChanged() {
  using third_party::starboard::rdk::shared::player::GetPlayerRegistry;
  GetPlayerRegistry()->AudioConfigurationChanged();
}

}  // namespace player
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

using third_party::starboard::rdk::shared::player::PlayerImpl;

SbPlayerPrivate::SbPlayerPrivate(
    SbWindow window,
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbDrmSystem drm_system,
    const SbMediaAudioStreamInfo& audio_info,
    const char* max_video_capabilities,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* provider) {
  if ( third_party::starboard::rdk::shared::player::GetPlayerRegistry()->CanCreate(max_video_capabilities) ) {
    player_.reset(
      new PlayerImpl(this,
                     window,
                     video_codec,
                     audio_codec,
                     drm_system,
                     audio_info,
                     max_video_capabilities,
                     sample_deallocate_func,
                     decoder_status_func,
                     player_status_func,
                     player_error_func,
                     context,
                     output_mode,
                     provider));
    if (  !static_cast<PlayerImpl&>(*player_).IsValid() )
      player_.reset(nullptr);
  }
}
