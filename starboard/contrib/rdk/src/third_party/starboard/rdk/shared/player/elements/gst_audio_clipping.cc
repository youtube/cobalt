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
#include "third_party/starboard/rdk/shared/player/elements/gst_audio_clipping.h"

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace player {
namespace elements {

namespace {

G_BEGIN_DECLS

#define COBALT_AUDIO_CLIPPING_TYPE          (cobalt_audio_clipping_get_type())
#define COBALT_AUDIO_CLIPPING(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), COBALT_AUDIO_CLIPPING_TYPE, CobaltAudioClipping))
#define COBALT_AUDIO_CLIPPING_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), COBALT_AUDIO_CLIPPING_TYPE, CobaltAudioClippingClass))

typedef struct _CobaltAudioClipping           CobaltAudioClipping;
typedef struct _CobaltAudioClippingClass      CobaltAudioClippingClass;

GType cobalt_audio_clipping_get_type(void);

struct _CobaltAudioClipping {
  GstAudioFilter parent;
};

struct _CobaltAudioClippingClass {
  GstAudioFilterClass parentClass;
};

G_END_DECLS

#define ALLOWED_CAPS                                 \
    GST_AUDIO_CAPS_MAKE(GST_AUDIO_FORMATS_ALL) ", "  \
    " layout=(string)interleaved"

#define cobalt_audio_clipping_parent_class parent_class
G_DEFINE_TYPE(CobaltAudioClipping, cobalt_audio_clipping, GST_TYPE_AUDIO_FILTER);

GST_DEBUG_CATEGORY(cobalt_audio_clipping_debug_category);
#define GST_CAT_DEFAULT cobalt_audio_clipping_debug_category

static GstFlowReturn cobalt_audio_clipping_transform_ip(GstBaseTransform* base, GstBuffer* buffer);

static void cobalt_audio_clipping_class_init(CobaltAudioClippingClass* klass) {
  GstElementClass* element_class = GST_ELEMENT_CLASS(klass);
  gst_element_class_set_static_metadata(
    element_class, "Audio clipping.", "Filter/Audio",
    "Audio filter element for Cobalt.", "Comcast");

  GstBaseTransformClass* base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
  base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(cobalt_audio_clipping_transform_ip);
  base_transform_class->transform_ip_on_passthrough = FALSE;

  GstAudioFilterClass *filter_class = (GstAudioFilterClass *) (klass);
  GstCaps *caps = gst_caps_from_string (ALLOWED_CAPS);
  gst_audio_filter_class_add_pad_templates (filter_class, caps);
  gst_caps_unref (caps);
}

static void cobalt_audio_clipping_init(CobaltAudioClipping* self) {
  GST_DEBUG_CATEGORY_INIT(cobalt_audio_clipping_debug_category,
    "cobaltaudioclipping", 0, "Audio clipping");

  GstBaseTransform* base = GST_BASE_TRANSFORM(self);
  gst_base_transform_set_in_place(base, TRUE);
  gst_base_transform_set_passthrough(base, FALSE);
  gst_base_transform_set_gap_aware(base, FALSE);
}

static GstFlowReturn cobalt_audio_clipping_transform_ip(GstBaseTransform* base, GstBuffer* buffer) {
  GstAudioFilter *filter = GST_AUDIO_FILTER_CAST (base);

  GstAudioMeta *meta = gst_buffer_get_audio_meta (buffer);
  GstAudioClippingMeta *cmeta = gst_buffer_get_audio_clipping_meta (buffer);

  if (!cmeta)
    return GST_FLOW_OK;

  GST_LOG_OBJECT (filter, "%" GST_PTR_FORMAT, buffer);

  guint64 discard_front = 0, discard_back = 0;
  gint bpf = filter->info.bpf, rate = filter->info.rate;
  gsize samples = meta ? meta->samples : (gst_buffer_get_size (buffer) / bpf);

  if (cmeta->format == GST_FORMAT_TIME) {
    GST_LOG_OBJECT (filter, "clipping start %" GST_TIME_FORMAT ", end %" GST_TIME_FORMAT,
                    GST_TIME_ARGS (cmeta->start), GST_TIME_ARGS(cmeta->end));
    discard_front = gst_util_uint64_scale(cmeta->start, rate, GST_SECOND);
    discard_back  = gst_util_uint64_scale(cmeta->end, rate, GST_SECOND);
  } else if (cmeta->format == GST_FORMAT_DEFAULT) {
    GST_LOG_OBJECT (filter, "clipping start %llu end %llu", cmeta->start, cmeta->end);
    discard_front = cmeta->start;
    discard_back = cmeta->end;
  } else {
    GST_LOG_OBJECT (filter, "Not supported clipping format");
    return GST_FLOW_OK;
  }

  if ((discard_front + discard_back) < samples) {
    gsize rest = samples - discard_front - discard_back;
    gst_buffer_resize (buffer, bpf * discard_front, bpf * rest);
    gst_buffer_remove_meta (buffer, (GstMeta *)cmeta);
    if (meta)
      meta->samples = rest;
    return GST_FLOW_OK;
  }

  // drop the entire frame
  GST_LOG_OBJECT (filter, "drop entire frame");
  return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

}  // namespace

GstElement *CreateAudioClippingElement(const gchar* name) {
  return GST_ELEMENT ( g_object_new (COBALT_AUDIO_CLIPPING_TYPE, name) );
}

}  // namespace elements
}  // namespace player
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
