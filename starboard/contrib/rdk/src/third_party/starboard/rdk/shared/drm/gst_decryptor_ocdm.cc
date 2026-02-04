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
#include "third_party/starboard/rdk/shared/drm/gst_decryptor_ocdm.h"

#if defined(HAS_OCDM)
#include "third_party/starboard/rdk/shared/drm/drm_system_ocdm.h"

#include <condition_variable>
#include <mutex>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbytereader.h>

#include <opencdm/open_cdm.h>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace drm {

namespace {

G_BEGIN_DECLS

#define COBALT_OCDM_DECRYPTOR_TYPE          (cobalt_ocdm_decryptor_get_type())
#define COBALT_OCDM_DECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), COBALT_OCDM_DECRYPTOR_TYPE, CobaltOcdmDecryptor))
#define COBALT_OCDM_DECRYPTOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), COBALT_OCDM_DECRYPTOR_TYPE, CobaltOcdmDecryptorClass))

typedef struct _CobaltOcdmDecryptor           CobaltOcdmDecryptor;
typedef struct _CobaltOcdmDecryptorClass      CobaltOcdmDecryptorClass;
typedef struct _CobaltOcdmDecryptorPrivate    CobaltOcdmDecryptorPrivate;

GType cobalt_ocdm_decryptor_get_type(void);

struct _CobaltOcdmDecryptor {
  GstBaseTransform parent;
  CobaltOcdmDecryptorPrivate *priv;
};

struct _CobaltOcdmDecryptorClass {
  GstBaseTransformClass parentClass;
};

G_END_DECLS

static GstStaticPadTemplate sinkTemplate =
  GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srcTemplate =
  GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY(cobalt_ocdm_decryptor_debug_category);
#define GST_CAT_DEFAULT cobalt_ocdm_decryptor_debug_category

struct _CobaltOcdmDecryptorPrivate : public DrmSystemOcdm::Observer {
  _CobaltOcdmDecryptorPrivate() {
#ifndef GST_DISABLE_GST_DEBUG
    if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
      decrypt_dur_.reserve( 300 );
    }
#endif
  }

  ~_CobaltOcdmDecryptorPrivate() {
    if (drm_system_)
      drm_system_->RemoveObserver(this);
    if (current_key_id_) {
      gst_buffer_unref(current_key_id_);
      current_key_id_ = nullptr;
    }
    if (cached_caps_) {
      gst_caps_unref(cached_caps_);
      cached_caps_ = nullptr;
    }
  }

  // DrmSystemOcdm::Observer
  void OnKeyReady(const uint8_t* key, size_t key_len) override {
#ifndef GST_DISABLE_GST_DEBUG
    if (gst_debug_category_get_threshold(GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
      gchar *md5sum = g_compute_checksum_for_data(G_CHECKSUM_MD5, key, key_len);
      GST_DEBUG("key ready: %s", md5sum);
      g_free(md5sum);
    }
#endif

    std::lock_guard lock(mutex_);
    if (awaiting_key_info_)
      condition_.notify_one();
  }

  GstFlowReturn Decrypt(
    CobaltOcdmDecryptor* self, GstBuffer* buffer,
    GstBuffer* subsamples, uint32_t subsample_count,
    GstBuffer* iv, GstBuffer* key) {

    gint64 start = 0;

#ifndef GST_DISABLE_GST_DEBUG
    const GstDebugLevel debug_level = gst_debug_category_get_threshold(GST_CAT_DEFAULT);
    if (debug_level >= GST_LEVEL_TRACE) {
      gchar *md5sum = 0;

      GstMapInfo map_info;
      if (gst_buffer_map(key, &map_info, GST_MAP_READ)) {
        md5sum = g_compute_checksum_for_data(G_CHECKSUM_MD5, map_info.data, map_info.size);
        gst_buffer_unmap(key, &map_info);
      }

      uint32_t total_clear = 0;
      uint32_t total_encrypted = 0;
      GstMapInfo sample_map;
      if (gst_buffer_map(subsamples, &sample_map, GST_MAP_READ)) {
        GstByteReader* reader = gst_byte_reader_new(sample_map.data, sample_map.size);
        for (uint32_t i = 0; i < subsample_count; ++i) {
          uint16_t clear = 0;
          uint32_t encrypted = 0;
          gst_byte_reader_get_uint16_be(reader, &clear);
          gst_byte_reader_get_uint32_be(reader, &encrypted);
          total_clear += clear;
          total_encrypted += encrypted;
        }
        gst_byte_reader_free(reader);
        gst_buffer_unmap(subsamples, &sample_map);
      }

      GST_TRACE_OBJECT(self, "buf=(%" GST_PTR_FORMAT "), "
                       "subsample_count=%u, subsamples=(%p), clear=%u, encrypted=%u, iv=(%p), key=(%p : %s)",
                       buffer, subsample_count, subsamples, total_clear, total_encrypted, iv, key, md5sum);

      g_free(md5sum);
    }
#endif

    if ( !drm_system_ ) {
      GstContext* context = gst_element_get_context(GST_ELEMENT(self), "cobalt-drm-system");
      if (context) {
        const GValue* value = gst_structure_get_value(gst_context_get_structure(context), "drm-system-instance");
        DrmSystemOcdm* drm_system = reinterpret_cast<DrmSystemOcdm*>(value ? g_value_get_pointer(value) : nullptr);
        SetDrmSystem( drm_system );
      }
      if (!drm_system_) {
        GST_ELEMENT_ERROR (self, STREAM, DECRYPT, ("No DRM System instance"), (NULL));
        return GST_FLOW_ERROR;
      }
    }

    GstMapInfo map_info;
    if (FALSE == gst_buffer_map(key, &map_info, GST_MAP_READ)) {
      GST_ELEMENT_ERROR (self, STREAM, DECRYPT, ("Failed to map kid buffer"), (NULL));
      return GST_FLOW_NOT_SUPPORTED;
    } else {
      if (!current_key_id_ || gst_buffer_memcmp(current_key_id_, 0, map_info.data, map_info.size) != 0) {
        if (debug_level >= GST_LEVEL_DEBUG) {
          gchar *md5sum = g_compute_checksum_for_data(G_CHECKSUM_MD5, map_info.data, map_info.size);
          GST_DEBUG_OBJECT(self, "Got buffer protected with key %s", md5sum);
          g_free(md5sum);
        }
        std::unique_lock lock(mutex_);
        current_session_id_.clear();
        if (current_key_id_) {
          gst_buffer_unref(current_key_id_);
          current_key_id_ = nullptr;
        }
        while(true) {
          if (is_flushing_ || is_active_ == false)
            break;
          current_session_id_ = drm_system_->SessionIdByKeyId(map_info.data, map_info.size);
          if (!current_session_id_.empty()) {
            current_key_id_ = gst_buffer_copy(key);
            break;
          }
          GST_DEBUG_OBJECT(self, "Session id is empty, waiting");
          awaiting_key_info_ = &map_info;
          condition_.wait(lock);
          awaiting_key_info_ = nullptr;
        }
        if (debug_level >= GST_LEVEL_DEBUG) {
          gchar *md5sum = g_compute_checksum_for_data(G_CHECKSUM_MD5, (const guchar*)current_session_id_.c_str(), current_session_id_.size());
          GST_DEBUG_OBJECT(self, "Using session with id '%s'", md5sum);
          g_free(md5sum);
        }
      }
      gst_buffer_unmap(key, &map_info);
    }

    if ( current_session_id_.empty() ) {
      if ( is_flushing_ ) {
        GST_DEBUG_OBJECT(self, "flushing");
        return GST_FLOW_FLUSHING;
      }
      if ( !is_active_ ) {
        GST_DEBUG_OBJECT(self, "inactive");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
      }
      GST_ELEMENT_ERROR (self, STREAM, DECRYPT_NOKEY, ("No session found"), (NULL));
      return GST_FLOW_NOT_SUPPORTED;
    }

    GstCaps *caps = nullptr;
    gst_caps_replace(&caps, cached_caps_);
    if ( !caps ) {
      GstPad* sink_pad = gst_element_get_static_pad(GST_ELEMENT(self), "sink");
      caps = gst_pad_get_current_caps(sink_pad);
      gst_object_unref(sink_pad);
      GST_DEBUG_OBJECT(self, "using new caps for decrypt = %" GST_PTR_FORMAT, caps);
      SetCachedCaps( caps );
    }

#ifndef GST_DISABLE_GST_DEBUG
    if ( debug_level >= GST_LEVEL_DEBUG ) {
      start = g_get_monotonic_time();
    }
#endif

    int rc = drm_system_->Decrypt(
      current_session_id_, buffer,
      subsamples, subsample_count,
      iv, key, caps);

    if ( caps ) {
      gst_caps_unref(caps);
      caps = nullptr;
    }

    if ( rc != 0 ) {
      if ( rc == (int)ERROR_INVALID_SESSION ) {
        GST_DEBUG_OBJECT(self, "Invalid session. Probably due to player shutdown.");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
      }

      GST_ELEMENT_ERROR (self, STREAM, DECRYPT, ("Decryption failed"), ("error code = %d", rc));
      return GST_FLOW_ERROR;
    }

    if ( start ) {
      gint64 dur_ms = (g_get_monotonic_time() - start) / 1000;

      decrypt_dur_.push_back(dur_ms);
      total_time_ += dur_ms;
      total_size_ += gst_buffer_get_size(buffer);

      ++buf_count_;

      if ( buf_count_ == 300 ) {
        std::sort(decrypt_dur_.begin(), decrypt_dur_.end());

        uint64_t median     = decrypt_dur_[ decrypt_dur_.size() / 2 ];
        uint64_t max_time   = *decrypt_dur_.rbegin();
        uint64_t total_time = total_time_; total_time_ = 0;
        uint64_t total_size = total_size_; total_size_ = 0;
        uint32_t buf_count  = buf_count_; buf_count_ = 0;

        decrypt_dur_.resize(0);

        GST_DEBUG_OBJECT(self,
          "%s decrypt time: "
          "avg = %" G_GUINT64_FORMAT " ms, "
          "median = %" G_GUINT64_FORMAT " ms, "
          "max = %" G_GUINT64_FORMAT " ms, "
          "size = %" G_GUINT64_FORMAT " kb, "
          "buf count = %u",
          IsVideo() ? "video" : "audio",
          (total_time / buf_count),
          median,
          max_time,
          (total_size / 1024),
          buf_count);
      }
    }

    return GST_FLOW_OK;
  }

  void SetDrmSystem(DrmSystemOcdm* system) {
    if (drm_system_)
      drm_system_->RemoveObserver(this);
    drm_system_ = system;
    if (drm_system_)
      drm_system_->AddObserver(this);
  }

  void SetIsFlushing(bool is_flushing) {
    std::lock_guard lock(mutex_);
    is_flushing_ = is_flushing;
    condition_.notify_one();
  }

  void SetActive(bool is_active) {
    std::lock_guard lock(mutex_);
    is_active_ = is_active;
    condition_.notify_one();
  }

  void SetCachedCaps(GstCaps* caps) {
    gst_caps_replace(&cached_caps_, caps);

    if ( caps ) {
      const GstStructure *s;
      const gchar *media_type;
      s = gst_caps_get_structure (caps, 0);
      media_type = gst_structure_get_name (s);

      is_video_ = g_str_has_prefix(media_type, "video");
    }
  }

  bool IsVideo() const { return is_video_; }

private:
  std::mutex mutex_;
  std::condition_variable condition_;

  GstCaps*    cached_caps_ { nullptr };
  GstMapInfo* awaiting_key_info_ { nullptr };
  GstBuffer*  current_key_id_ { nullptr };
  std::string current_session_id_;

  DrmSystemOcdm* drm_system_ { nullptr };
  bool is_flushing_ { false };
  bool is_active_ { true };
  bool is_video_ { false };

  std::vector<uint64_t> decrypt_dur_;
  uint64_t total_time_ { 0 };
  uint64_t total_size_ { 0 };
  uint32_t buf_count_ { 0 };
};

#define cobalt_ocdm_decryptor_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE(CobaltOcdmDecryptor, cobalt_ocdm_decryptor, GST_TYPE_BASE_TRANSFORM);

static void cobalt_ocdm_decryptor_finalize(GObject*);
static GstCaps* cobalt_ocdm_decryptor_transform_caps(GstBaseTransform*, GstPadDirection, GstCaps*, GstCaps*);
static GstFlowReturn cobalt_ocdm_decryptor_transform_ip(GstBaseTransform* base, GstBuffer* buffer);
static gboolean cobalt_ocdm_decryptor_sink_event(GstBaseTransform* base, GstEvent* event);
static gboolean cobalt_ocdm_decryptor_stop(GstBaseTransform *base);
static gboolean cobalt_ocdm_decryptor_start(GstBaseTransform *base);
static void cobalt_ocdm_decryptor_set_context(GstElement* element, GstContext* context);
static GstStateChangeReturn cobalt_ocdm_decryptor_change_state(GstElement* element, GstStateChange transition);

static void cobalt_ocdm_decryptor_class_init(CobaltOcdmDecryptorClass* klass) {
  GST_DEBUG_CATEGORY_INIT(
    cobalt_ocdm_decryptor_debug_category,
    "cobaltocdm", 0, "OCDM Decryptor for Cobalt");

  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->finalize = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_finalize);

  GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
  element_class->set_context = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_set_context);
  element_class->change_state = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_change_state);

  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sinkTemplate));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&srcTemplate));
  gst_element_class_set_static_metadata(
    element_class, "OCDM Decryptor.",
    GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
    "Decryptor element for Cobalt.", "Comcast");

  GstBaseTransformClass* base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_transform_caps);
  base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_transform_ip);
  base_transform_class->transform_ip_on_passthrough = FALSE;
  base_transform_class->sink_event = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_sink_event);
  base_transform_class->start = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR(cobalt_ocdm_decryptor_stop);
}

static void cobalt_ocdm_decryptor_init(CobaltOcdmDecryptor* self) {
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));
  self->priv = new (priv) CobaltOcdmDecryptorPrivate();

  GstBaseTransform* base = GST_BASE_TRANSFORM(self);
  gst_base_transform_set_in_place(base, TRUE);
  gst_base_transform_set_passthrough(base, FALSE);
  gst_base_transform_set_gap_aware(base, FALSE);
}

static void cobalt_ocdm_decryptor_finalize(GObject* object) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(object);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));

  priv->~CobaltOcdmDecryptorPrivate();

  GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static GstCaps* cobalt_ocdm_decryptor_transform_caps(GstBaseTransform* base, GstPadDirection direction, GstCaps* caps, GstCaps* filter) {
  if (direction == GST_PAD_UNKNOWN)
    return nullptr;

  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(base);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));

  GST_DEBUG_OBJECT(self, "Transform in direction: %s, caps %" GST_PTR_FORMAT ", filter %" GST_PTR_FORMAT,
                   direction == GST_PAD_SINK ? "GST_PAD_SINK" : "GST_PAD_SRC", caps, filter);

  priv->SetCachedCaps( nullptr );

  return GST_BASE_TRANSFORM_CLASS(parent_class)->transform_caps(base, direction, caps, filter);
}

static GstFlowReturn cobalt_ocdm_decryptor_transform_ip(GstBaseTransform* base, GstBuffer* buffer) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(base);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));

  GST_TRACE_OBJECT(self, "Transform in place buf=(%" GST_PTR_FORMAT ")", buffer);

  GstProtectionMeta* protection_meta = reinterpret_cast<GstProtectionMeta*>(gst_buffer_get_protection_meta(buffer));
  if (!protection_meta) {
    GST_TRACE_OBJECT(self, "Clear sample");
    return GST_FLOW_OK;
  }

  GstFlowReturn ret = GST_FLOW_NOT_SUPPORTED;
  GstStructure* info = protection_meta->info;
  GstBuffer* subsamples = nullptr;
  GstBuffer* iv = nullptr;
  GstBuffer* key = nullptr;
  uint32_t subsample_count = 0u;

  const GValue* value = nullptr;

  value = gst_structure_get_value(info, "kid");
  if (!value) {
    GST_ELEMENT_ERROR (self, STREAM, DECRYPT_NOKEY, ("No key ID available for encrypted sample"), (NULL));
    goto exit;
  }
  key = gst_value_get_buffer(value);

  value = gst_structure_get_value(info, "iv");
  if (!value) {
    GST_ELEMENT_ERROR (self, STREAM, DECRYPT_NOKEY, ("Failed to get IV buffer"), (NULL));
    goto exit;
  }
  iv = gst_value_get_buffer(value);

  if (!gst_structure_get_uint(info, "subsample_count", &subsample_count)) {
    GST_ELEMENT_ERROR (self, STREAM, DECRYPT, ("Failed to get subsamples_count"), (NULL));
    goto exit;
  }

  if (subsample_count) {
    value = gst_structure_get_value(info, "subsamples");
    if (!value) {
      GST_ELEMENT_ERROR (self, STREAM, DECRYPT, ("Failed to get subsamples buffer"), (NULL));
      goto exit;
    }
    subsamples = gst_value_get_buffer(value);
  }

  if (!gst_structure_has_field(info, "initWithLast15")) {
    gst_structure_set (info, "initWithLast15", G_TYPE_UINT, 0, NULL);
  }

  ret = priv->Decrypt(self, buffer, subsamples, subsample_count, iv, key);

  GST_TRACE_OBJECT(self, "ret=%s", gst_flow_get_name(ret));

exit:
  gst_buffer_remove_meta(buffer, reinterpret_cast<GstMeta*>(protection_meta));
  return ret;
}

static void cobalt_ocdm_decryptor_set_context(GstElement* element, GstContext* context) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(element);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));

  if (gst_context_has_context_type(context, "cobalt-drm-system")) {
    const GValue* value = gst_structure_get_value(gst_context_get_structure(context), "drm-system-instance");
    DrmSystemOcdm* drm_system = reinterpret_cast<DrmSystemOcdm*>(value ? g_value_get_pointer(value) : nullptr);
    priv->SetDrmSystem( drm_system );
    GST_DEBUG_OBJECT(self, "got drm system %p", drm_system);
    return;
  }

  GST_ELEMENT_CLASS(parent_class)->set_context(element, context);
}

static GstStateChangeReturn cobalt_ocdm_decryptor_change_state(GstElement* element, GstStateChange transition) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(element);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      priv->SetActive(true);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      priv->SetActive(false);
      break;
    default:
      break;
  }

  GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

  return result;
}

static gboolean cobalt_ocdm_decryptor_sink_event(GstBaseTransform* base, GstEvent* event) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(base);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));

  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_FLUSH_START: {
      GST_DEBUG_OBJECT(self, "flushing");
      priv->SetIsFlushing(true);
      break;
    }
    case GST_EVENT_FLUSH_STOP: {
      GST_DEBUG_OBJECT(self, "flushing done");
      priv->SetIsFlushing(false);
      break;
    default:
        break;
    }
  }

  return GST_BASE_TRANSFORM_CLASS(parent_class)->sink_event(base, event);
}

static gboolean cobalt_ocdm_decryptor_stop(GstBaseTransform *base) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(base);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));
  priv->SetActive(false);
  return TRUE;
}

static gboolean cobalt_ocdm_decryptor_start(GstBaseTransform *base) {
  CobaltOcdmDecryptor* self = COBALT_OCDM_DECRYPTOR(base);
  CobaltOcdmDecryptorPrivate* priv = reinterpret_cast<CobaltOcdmDecryptorPrivate*>(
    cobalt_ocdm_decryptor_get_instance_private(self));
  priv->SetActive(true);
  return TRUE;
}

}  // namespace

GstElement *CreateDecryptorElement(const gchar* name) {
  return GST_ELEMENT ( g_object_new (COBALT_OCDM_DECRYPTOR_TYPE, name) );
}

}  // namespace drm
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#else  //defined(HAS_OCDM)

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace drm {

GstElement *CreateDecryptorElement(const gchar* name) {
  return nullptr;
}

}  // namespace drm
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

#endif  //defined(HAS_OCDM)
