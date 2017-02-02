"""Provides cval names needed by the webdriver benchmarks."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

EVENT_COUNT_STRING = "Event.Count.MainWebModule"
EVENT_DURATION_STRING = "Event.Duration.MainWebModule"


def count_dom_active_dispatch_events():
  return "Count.DOM.ActiveDispatchEvents"


def count_dom_event_listeners():
  return "Count.DOM.EventListeners"


def count_dom_nodes():
  return "Count.DOM.Nodes"


def count_dom_html_elements():
  return "Count.MainWebModule.DOM.HtmlElement"


def count_image_cache_loading_resources():
  return "Count.MainWebModule.ImageCache.LoadingResources"


def count_image_cache_pending_callbacks():
  return "Count.MainWebModule.ImageCache.PendingCallbacks"


def count_layout_boxes():
  return "Count.MainWebModule.Layout.Box"


def count_font_files_loaded():
  return "Count.Font.FilesLoaded"


def count_remote_typeface_cache_loading_resources():
  return "Count.MainWebModule.RemoteTypefaceCache.LoadingResources"


def count_remote_typeface_cache_pending_callbacks():
  return "Count.MainWebModule.RemoteTypefaceCache.PendingCallbacks"


def event_produced_render_tree(event_type):
  return "Event.MainWebModule.{}.ProducedRenderTree".format(event_type)


def event_count_dom_html_elements_created(event_type):
  return "{}.{}.DOM.HtmlElement.Created".format(EVENT_COUNT_STRING, event_type)


def event_count_dom_html_elements_destroyed(event_type):
  return "{}.{}.DOM.HtmlElement.Destroyed".format(EVENT_COUNT_STRING,
                                                  event_type)


def event_count_dom_update_matching_rules(event_type):
  return "{}.{}.DOM.HtmlElement.UpdateMatchingRules".format(EVENT_COUNT_STRING,
                                                            event_type)


def event_count_dom_update_computed_style(event_type):
  return "{}.{}.DOM.HtmlElement.UpdateComputedStyle".format(EVENT_COUNT_STRING,
                                                            event_type)


def event_count_layout_boxes_created(event_type):
  return "{}.{}.Layout.Box.Created".format(EVENT_COUNT_STRING, event_type)


def event_count_layout_boxes_destroyed(event_type):
  return "{}.{}.Layout.Box.Destroyed".format(EVENT_COUNT_STRING, event_type)


def event_duration_total(event_type):
  return "{}.{}".format(EVENT_DURATION_STRING, event_type)


def event_duration_dom_inject_event(event_type):
  return "{}.{}.DOM.InjectEvent".format(EVENT_DURATION_STRING, event_type)


def event_duration_dom_update_computed_style(event_type):
  return "{}.{}.DOM.UpdateComputedStyle".format(EVENT_DURATION_STRING,
                                                event_type)


def event_duration_dom_video_start_delay():
  return "{}.DOM.VideoStartDelay".format(EVENT_DURATION_STRING)


def event_duration_layout_box_tree(event_type):
  return "{}.{}.Layout.BoxTree".format(EVENT_DURATION_STRING, event_type)


def event_duration_layout_box_tree_box_generation(event_type):
  return "{}.{}.Layout.BoxTree.BoxGeneration".format(EVENT_DURATION_STRING,
                                                     event_type)


def event_duration_layout_box_tree_update_used_sizes(event_type):
  return "{}.{}.Layout.BoxTree.UpdateUsedSizes".format(EVENT_DURATION_STRING,
                                                       event_type)


def event_duration_layout_render_and_animate(event_type):
  return "{}.{}.Layout.RenderAndAnimate".format(EVENT_DURATION_STRING,
                                                event_type)


def layout_is_dirty():
  return "MainWebModule.Layout.IsDirty"


def renderer_has_active_animations():
  return "Renderer.HasActiveAnimations"


def rasterize_animations_average():
  return "Renderer.Rasterize.Animations.Avg"


def rasterize_animations_percentile_25():
  return "Renderer.Rasterize.Animations.Pct.25th"


def rasterize_animations_percentile_50():
  return "Renderer.Rasterize.Animations.Pct.50th"


def rasterize_animations_percentile_75():
  return "Renderer.Rasterize.Animations.Pct.75th"


def rasterize_animations_percentile_95():
  return "Renderer.Rasterize.Animations.Pct.95th"
