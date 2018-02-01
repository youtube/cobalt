"""Provides names for Cobalt CVals that are used by webdriver-related tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


def count_dom_active_java_script_events():
  return 'Count.DOM.ActiveJavaScriptEvents'


def count_dom_html_elements_document():
  return 'Count.MainWebModule.DOM.HtmlElement.Document'


def count_dom_html_elements_total():
  return 'Count.MainWebModule.DOM.HtmlElement.Total'


def count_dom_html_script_element_execute():
  return 'Count.MainWebModule.DOM.HtmlScriptElement.Execute'


def count_layout_boxes():
  return 'Count.MainWebModule.Layout.Box'


def count_font_cache_resource_loaded():
  return 'Count.MainWebModule.RemoteTypefaceCache.Resource.Loaded'


def count_font_cache_resource_requested():
  return 'Count.MainWebModule.RemoteTypefaceCache.Resource.Requested'


def count_image_cache_resource_loading():
  return 'Count.MainWebModule.ImageCache.Resource.Loading'


def count_image_cache_resource_requested():
  return 'Count.MainWebModule.ImageCache.Resource.Requested'


def count_image_cache_resource_loaded():
  return 'Count.MainWebModule.ImageCache.Resource.Loaded'


def count_rasterize_new_render_tree():
  return 'Count.Renderer.Rasterize.NewRenderTree'


def count_version_compatibility_violation():
  return 'Count.VersionCompatibilityViolation'


def event_duration_dom_video_start_delay():
  return 'Event.Duration.MainWebModule.DOM.VideoStartDelay'


def event_duration_renderer_rasterize_render_tree_delay(event_type):
  c_val_prefix = 'Event.Duration.MainWebModule'
  c_val_suffix = 'Renderer.Rasterize.RenderTreeDelay'
  return '{}.{}.{}'.format(c_val_prefix, event_type, c_val_suffix)


def event_is_processing():
  return 'Event.MainWebModule.IsProcessing'


def event_value_dictionary(event_type):
  return 'Event.MainWebModule.{}.ValueDictionary'.format(event_type)


def is_render_tree_generation_pending():
  return 'MainWebModule.Layout.IsRenderTreePending'


def is_render_tree_rasterization_pending():
  return 'MainWebModule.IsRenderTreeRasterizationPending'


def memory_font_cache_resource_loaded():
  return 'Memory.MainWebModule.RemoteTypefaceCache.Resource.Loaded'


def memory_image_cache_resource_loaded():
  return 'Memory.MainWebModule.ImageCache.Resource.Loaded'


def rasterize_animations_entry_list():
  return 'Renderer.Rasterize.Animations.EntryList'


def rasterize_animations_interval_entry_list():
  return 'Renderer.Rasterize.AnimationsInterval.EntryList'


def renderer_has_active_animations():
  return 'Renderer.HasActiveAnimations'


def time_browser_navigate():
  return 'Time.Browser.Navigate'


def time_browser_on_load_event():
  return 'Time.Browser.OnLoadEvent'


def time_cobalt_start():
  return 'Time.Cobalt.Start'


def time_dom_html_script_element_execute():
  return 'Time.MainWebModule.DOM.HtmlScriptElement.Execute'


def time_rasterize_animations_start():
  return 'Time.Renderer.Rasterize.Animations.Start'


def time_rasterize_animations_end():
  return 'Time.Renderer.Rasterize.Animations.End'


def time_rasterize_new_render_tree():
  return 'Time.Renderer.Rasterize.NewRenderTree'
