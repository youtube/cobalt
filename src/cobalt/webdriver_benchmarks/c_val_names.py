"""Provides cval names needed by the webdriver benchmarks."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


def count_dom_active_dispatch_events():
  return "Count.DOM.ActiveDispatchEvents"


def count_image_cache_loading_resources():
  return "Count.MainWebModule.ImageCache.LoadingResources"


def event_duration_dom_video_start_delay():
  return "Event.Duration.MainWebModule.DOM.VideoStartDelay"


def event_value_dictionary(event_type):
  return "Event.MainWebModule.{}.ValueDictionary".format(event_type)


def layout_is_dirty():
  return "MainWebModule.Layout.IsDirty"


def rasterize_animations_entry_list():
  return "Renderer.Rasterize.Animations.EntryList"


def renderer_has_active_animations():
  return "Renderer.HasActiveAnimations"
