"""Provides cval names needed by the webdriver benchmarks."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


def count_dom_active_java_script_events():
  return "Count.DOM.ActiveJavaScriptEvents"


def count_dom_html_elements_document():
  return "Count.MainWebModule.DOM.HtmlElement.Document"


def count_dom_html_elements_total():
  return "Count.MainWebModule.DOM.HtmlElement.Total"


def count_dom_html_script_element_execute():
  return "Count.MainWebModule.DOM.HtmlScriptElement.Execute"


def count_layout_boxes():
  return "Count.MainWebModule.Layout.Box"


def count_image_cache_loading_resources():
  return "Count.MainWebModule.ImageCache.LoadingResources"


def count_image_cache_requested_resources():
  return "Count.MainWebModule.ImageCache.RequestedResources"


def count_rasterize_new_render_tree():
  return "Count.Renderer.Rasterize.NewRenderTree"


def event_duration_dom_video_start_delay():
  return "Event.Duration.MainWebModule.DOM.VideoStartDelay"


def event_is_processing():
  return "Event.MainWebModule.IsProcessing"


def event_value_dictionary(event_type):
  return "Event.MainWebModule.{}.ValueDictionary".format(event_type)


def layout_is_dirty():
  return "MainWebModule.Layout.IsDirty"


def rasterize_animations_entry_list():
  return "Renderer.Rasterize.Animations.EntryList"


def renderer_has_active_animations():
  return "Renderer.HasActiveAnimations"


def time_browser_navigate():
  return "Time.Browser.Navigate"


def time_browser_on_load_event():
  return "Time.Browser.OnLoadEvent"


def time_cobalt_start():
  return "Time.Cobalt.Start"


def time_dom_html_script_element_execute():
  return "Time.MainWebModule.DOM.HtmlScriptElement.Execute"


def time_rasterize_animations_start():
  return "Time.Renderer.Rasterize.Animations.Start"


def time_rasterize_animations_end():
  return "Time.Renderer.Rasterize.Animations.End"


def time_rasterize_new_render_tree():
  return "Time.Renderer.Rasterize.NewRenderTree"
