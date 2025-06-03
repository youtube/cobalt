# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is more akin to a .pyl/JSON file, so it's expected to be long.
# pylint: disable=too-many-lines

from __future__ import print_function

from datetime import date
import json
import logging
import os
import posixpath
import time
from typing import Callable, Dict, List, Optional

from enum import Enum

from gpu_tests import common_browser_args as cba
from gpu_tests import skia_gold_heartbeat_integration_test_base as sghitb
from gpu_tests import skia_gold_matching_algorithms as algo
from gpu_tests.util import websocket_server as wss

import gpu_path_util

from telemetry.internal.browser import browser as browser_module

CRASH_TYPE_BROWSER = 'browser'
CRASH_TYPE_GPU = 'gpu-process'
CRASH_TYPE_RENDERER = 'renderer'

SHORT_GLOBAL_TIMEOUT = 30

# Meant to be used when we know a test is going to be noisy, and we want any
# images it generates to be auto-triaged until we have enough data to calculate
# more suitable/less permissive parameters.
VERY_PERMISSIVE_SOBEL_ALGO = algo.SobelMatchingAlgorithm(
    max_different_pixels=100000000,
    pixel_delta_threshold=255,
    edge_threshold=0,
    ignored_border_thickness=1)

# The optimizer script spat out pretty similar values for most MP4 tests, so
# combine into a single set of parameters.
GENERAL_MP4_ALGO = algo.SobelMatchingAlgorithm(max_different_pixels=56300,
                                               pixel_delta_threshold=35,
                                               edge_threshold=80,
                                               ignored_border_thickness=1)

BrowserArgType = List[str]


class PixelTestPage(sghitb.SkiaGoldHeartbeatTestCase):
  """A wrapper class mimicking the functionality of the PixelTestsStorySet
  from the old-style GPU tests.
  """

  def __init__(  # pylint: disable=too-many-arguments
      self,
      url: str,
      name: str,
      test_rect: List[int],
      *args,
      browser_args: Optional[BrowserArgType] = None,
      restart_browser_after_test: bool = False,
      other_args: Optional[dict] = None,
      expected_per_process_crashes: Optional[Dict[str, int]] = None,
      timeout: int = 300,
      should_capture_full_screenshot_func: Optional[Callable[
          [browser_module.Browser], bool]] = None,
      **kwargs):
    # Video tests can result in non-hermetic test behavior due to overlays, so
    # do a full refresh after each one. See crbug.com/1484212.
    is_video_test = 'video' in name.lower()
    super().__init__(name, refresh_after_finish=is_video_test, *args, **kwargs)
    self.url = url
    self.test_rect = test_rect
    self.browser_args = browser_args
    # Whether the browser should be forcibly restarted after the test
    # runs. The browser is always restarted after running tests with
    # optional_actions.
    self.restart_browser_after_test = restart_browser_after_test
    # These are used to pass additional arguments to the test harness.
    # VideoPathTraceTest and OverlayModeTest support the following boolean
    # arguments: pixel_format, zero_copy, no_overlay, video_is_rotated and
    # full_size.
    self.other_args = other_args
    # This lets the test runner know that one or more crashes are expected as
    # part of the test. Should be a map of process type (str) to expected number
    # of crashes (int).
    self.expected_per_process_crashes = expected_per_process_crashes or {}
    # Test timeout
    self.timeout = timeout
    # Most tests will use the standard capture code path which captures an image
    # that is more representative of what is shown to a user, but some tests
    # require capturing the entire web contents for some reason.
    if should_capture_full_screenshot_func is None:
      should_capture_full_screenshot_func = lambda _: False
    self.ShouldCaptureFullScreenshot = should_capture_full_screenshot_func


class TestActionCrashGpuProcess(sghitb.TestAction):
  """Runs JavaScript to crash the GPU process once."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    sghitb.EvalInTestIframe(tab_data.tab,
                            'chrome.gpuBenchmarking.crashGpuProcess()')


class TestActionSwitchTabs(sghitb.TestAction):
  """Opens and briefly switches to a new tab before switching back."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    tab = tab_data.tab
    if not tab.browser.supports_tab_control:
      test_instance.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that the new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    tab.Activate()


class TestActionSwitchTabsAndCopyImage(sghitb.TestAction):
  """Opens and closes a new tab before running test-specific JavaScript."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    tab = tab_data.tab
    if not tab.browser.supports_tab_control:
      test_instance.fail('Browser must support tab control')
    dummy_tab = tab.browser.tabs.New()
    dummy_tab.Activate()
    # Wait for 2 seconds so that the new tab becomes visible.
    dummy_tab.action_runner.Wait(2)
    dummy_tab.Close()
    sghitb.EvalInTestIframe(tab, 'copyImage()')


class TestActionRunOffscreenCanvasIBRCWebGLLowPerfTest(sghitb.TestAction):
  """Runs steps for an offscreen canvas IBRC WebGL test on the low power GPU."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    tab = tab_data.tab
    test_instance.AssertLowPowerGPU()
    sghitb.EvalInTestIframe(tab, 'setup()')
    # Wait a few seconds for any (incorrect) GPU switched notifications to
    # propagate throughout the system.
    time.sleep(5)
    test_instance.AssertLowPowerGPU()
    sghitb.EvalInTestIframe(tab, 'render()')


class TestActionRunOffscreenCanvasIBRCWebGLHighPerfTest(sghitb.TestAction):
  """Runs steps for an offscreen canvas IBRC WebGL test on the high perf GPU."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    tab = tab_data.tab
    test_instance.AssertLowPowerGPU()
    sghitb.EvalInTestIframe(tab, 'setup(true)')
    # Wait a few seconds for any (incorrect) GPU switched notifications to
    # propagate throughout the system.
    time.sleep(5)
    test_instance.AssertHighPerformanceGPU()
    sghitb.EvalInTestIframe(tab, 'render()')


class TestActionRunTestWithHighPerformanceTab(sghitb.TestAction):
  """Runs steps for a specific test with a high perf second tab present."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    tab = tab_data.tab
    if not test_instance.IsDualGPUMacLaptop():
      # Short-circuit this test.
      logging.info('Short-circuiting test because not running on dual-GPU Mac '
                   'laptop')
      sghitb.EvalInTestIframe(tab, 'initialize(false)')
      return

    high_performance_tab = tab.browser.tabs.New()
    high_performance_file = posixpath.join(
        gpu_path_util.GPU_DATA_RELATIVE_PATH,
        'functional_webgl_high_performance.html')
    high_performance_websocket_server = wss.WebsocketServer()
    high_performance_websocket_server.StartServer()
    high_performance_tab_data = sghitb.TabData(
        high_performance_tab, high_performance_websocket_server)
    high_performance_loop_state = sghitb.LoopState()
    try:
      test_instance.NavigateTo(high_performance_file,
                               tab_data=high_performance_tab_data)
      test_instance.HandleMessageLoop(SHORT_GLOBAL_TIMEOUT,
                                      loop_state=high_performance_loop_state,
                                      tab_data=high_performance_tab_data)
      assert high_performance_loop_state.test_finished

      # Wait a few seconds for the GPU switched notification to propagate
      # throughout the system.
      time.sleep(5)
      # Switch back to the main tab and quickly start its rendering, while the
      # high-power GPU is still active.
      tab.Activate()
      sghitb.EvalInTestIframe(tab, 'initialize(true)')
      test_instance.HandleMessageLoop(SHORT_GLOBAL_TIMEOUT,
                                      loop_state=loop_state,
                                      tab_data=tab_data)
      high_performance_tab.Close()
    finally:
      high_performance_websocket_server.StopServer()
    # Wait for ~15 seconds for the system to switch back to the
    # integrated GPU.
    time.sleep(15)
    # Run the page to completion.
    sghitb.EvalInTestIframe(tab, 'setTimeout(runToCompletion, 0)')


class TestActionRunLowToHighPowerTest(sghitb.TestAction):
  """Runs steps for the low to high power GPU transition test."""
  def Run(self, test_case: PixelTestPage, tab_data: sghitb.TabData,
          loop_state: sghitb.LoopState,
          test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase) -> None:
    is_dual_gpu = test_instance.IsDualGPUMacLaptop()
    sghitb.EvalInTestIframe(tab_data.tab,
                            'initialize(%s)' % json.dumps(is_dual_gpu))


def GetMediaStreamTestBrowserArgs(media_stream_source_relpath: str
                                  ) -> List[str]:
  return [
      '--use-fake-device-for-media-stream', '--use-fake-ui-for-media-stream',
      '--use-file-for-fake-video-capture=' +
      os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, media_stream_source_relpath)
  ]


def CaptureFullScreenshotOnFuchsia(browser: browser_module.Browser) -> bool:
  return browser.platform.GetOSName() == 'fuchsia'


class PixelTestPages():
  @staticmethod
  def DefaultPages(base_name: str) -> List[PixelTestPage]:
    sw_compositing_args = [cba.DISABLE_GPU_COMPOSITING]
    experimental_hdr_args = [cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES]

    switch_tab_test_actions = [
        sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
        TestActionSwitchTabs(),
        sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
    ]

    low_power_test_actions = [
        sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
        TestActionRunOffscreenCanvasIBRCWebGLLowPerfTest(),
        sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
    ]

    return [
        PixelTestPage(
            'pixel_background_image.html',
            base_name + '_BackgroundImage',
            test_rect=[20, 20, 370, 370],
            # Small Fuchsia screens result in an incomplete capture
            # without this.
            should_capture_full_screenshot_func=CaptureFullScreenshotOnFuchsia),
        PixelTestPage('pixel_reflected_div.html',
                      base_name + '_ReflectedDiv',
                      test_rect=[0, 0, 100, 300]),
        PixelTestPage('pixel_canvas2d.html',
                      base_name + '_Canvas2DRedBox',
                      test_rect=[0, 0, 300, 300],
                      matching_algorithm=algo.FuzzyMatchingAlgorithm(
                          max_different_pixels=130, pixel_delta_threshold=3)),
        PixelTestPage('pixel_canvas2d_untagged.html',
                      base_name + '_Canvas2DUntagged',
                      test_rect=[0, 0, 257, 257]),
        PixelTestPage('pixel_css3d.html',
                      base_name + '_CSS3DBlueBox',
                      test_rect=[0, 0, 300, 300],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=0,
                          pixel_delta_threshold=0,
                          edge_threshold=90)),
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name + '_WebGLGreenTriangle_AA_Alpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noaa_alpha.html',
                      base_name + '_WebGLGreenTriangle_NoAA_Alpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_aa_noalpha.html',
                      base_name + '_WebGLGreenTriangle_AA_NoAlpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noaa_noalpha.html',
                      base_name + '_WebGLGreenTriangle_NoAA_NoAlpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noalpha_implicit_clear.html',
                      base_name +
                      '_WebGLTransparentGreenTriangle_NoAlpha_ImplicitClear',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage(
            'pixel_webgl_context_restored.html',
            base_name + '_WebGLContextRestored',
            test_rect=[0, 0, 300, 300],
            test_actions=[
                sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
                TestActionCrashGpuProcess(),
                sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
            ]),
        PixelTestPage(
            'pixel_webgl_sad_canvas.html',
            base_name + '_WebGLSadCanvas',
            test_rect=[0, 0, 300, 300],
            test_actions=[
                sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
                TestActionCrashGpuProcess(),
                sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
                TestActionCrashGpuProcess(),
                sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
            ]),
        PixelTestPage('pixel_scissor.html',
                      base_name + '_ScissorTestWithPreserveDrawingBuffer',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_canvas2d_webgl.html',
                      base_name + '_2DCanvasWebGL',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage(
            'pixel_background.html',
            base_name + '_SolidColorBackground',
            test_rect=[500, 500, 600, 600],
            # Small Fuchsia screens result in an incomplete capture
            # without this.
            should_capture_full_screenshot_func=CaptureFullScreenshotOnFuchsia),
        PixelTestPage(
            'pixel_video_mp4.html?width=240&height=135&use_timer=1',
            base_name + '_Video_MP4',
            test_rect=[0, 0, 240, 135],
            # Most images are actually very similar, but Pixel 2
            # tends to produce images with all colors shifted by a
            # small amount.
            matching_algorithm=GENERAL_MP4_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_aspect_4x3.html'
            '?width=240&height=135&use_timer=1',
            base_name + '_Video_MP4_FourColors_Aspect_4x3',
            test_rect=[0, 0, 240, 135],
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=41700,
                pixel_delta_threshold=15,
                edge_threshold=40,
                ignored_border_thickness=1)),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_90.html'
            '?width=270&height=240&use_timer=1',
            base_name + '_Video_MP4_FourColors_Rot_90',
            test_rect=[0, 0, 270, 240],
            matching_algorithm=GENERAL_MP4_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_180.html'
            '?width=240&height=135&use_timer=1',
            base_name + '_Video_MP4_FourColors_Rot_180',
            test_rect=[0, 0, 240, 135],
            matching_algorithm=GENERAL_MP4_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_270.htm'
            'l?width=270&height=240&use_timer=1',
            base_name + '_Video_MP4_FourColors_Rot_270',
            test_rect=[0, 0, 270, 240],
            matching_algorithm=GENERAL_MP4_ALGO),
        PixelTestPage(
            'pixel_video_mp4_rounded_corner.html'
            '?width=240&height=135&use_timer=1',
            base_name + '_Video_MP4_Rounded_Corner',
            test_rect=[0, 0, 240, 135],
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=30500,
                pixel_delta_threshold=15,
                edge_threshold=70,
                ignored_border_thickness=1)),
        PixelTestPage('pixel_video_vp9.html?width=240&height=135&use_timer=1',
                      base_name + '_Video_VP9',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=114000,
                          pixel_delta_threshold=30,
                          edge_threshold=20,
                          ignored_border_thickness=1)),
        PixelTestPage('pixel_video_av1.html?width=240&height=135&use_timer=1',
                      base_name + '_Video_AV1',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=114000,
                          pixel_delta_threshold=30,
                          edge_threshold=20,
                          ignored_border_thickness=1)),
        PixelTestPage('pixel_video_hevc.html?width=240&height=135&use_timer=1',
                      base_name + '_Video_HEVC',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=114000,
                          pixel_delta_threshold=30,
                          edge_threshold=20,
                          ignored_border_thickness=1)),
        PixelTestPage(
            'pixel_video_media_stream_incompatible_stride.html',
            base_name + '_Video_Media_Stream_Incompatible_Stride',
            browser_args=GetMediaStreamTestBrowserArgs(
                'media/test/data/four-colors-incompatible-stride.y4m'),
            test_rect=[0, 0, 240, 135],
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),

        # The MP4 contains H.264 which is primarily hardware decoded on bots.
        PixelTestPage(
            'pixel_video_context_loss.html?src='
            '/media/test/data/four-colors.mp4',
            base_name + '_Video_Context_Loss_MP4',
            test_rect=[0, 0, 240, 135],
            # Optimizer script spat out a value of 255 for the Sobel edge
            # threshold, so use fuzzy for now since it's slightly more
            # efficient.
            matching_algorithm=algo.FuzzyMatchingAlgorithm(
                max_different_pixels=31700, pixel_delta_threshold=20),
            expected_per_process_crashes={
                CRASH_TYPE_GPU: 1,
            }),

        # The VP9 test clip is primarily software decoded on bots.
        PixelTestPage(('pixel_video_context_loss.html'
                       '?src=/media/test/data/four-colors-vp9.webm'),
                      base_name + '_Video_Context_Loss_VP9',
                      test_rect=[0, 0, 240, 135],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=54400,
                          pixel_delta_threshold=30,
                          edge_threshold=250,
                          ignored_border_thickness=1),
                      expected_per_process_crashes={
                          CRASH_TYPE_GPU: 1,
                      }),
        PixelTestPage(
            'pixel_video_backdrop_filter.html?width=240&height=135&use_timer=1',
            base_name + '_Video_BackdropFilter',
            test_rect=[0, 0, 240, 135],
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=1000,
                pixel_delta_threshold=20,
                edge_threshold=40,
                ignored_border_thickness=1)),
        PixelTestPage('pixel_webgl_premultiplied_alpha_false.html',
                      base_name + '_WebGL_PremultipliedAlpha_False',
                      test_rect=[0, 0, 150, 150]),
        PixelTestPage('pixel_webgl2_blitframebuffer_result_displayed.html',
                      base_name + '_WebGL2_BlitFramebuffer_Result_Displayed',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_webgl2_clearbufferfv_result_displayed.html',
                      base_name + '_WebGL2_ClearBufferfv_Result_Displayed',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D',
                      test_rect=[0, 0, 256, 256]),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D_SoftwareCompositing',
                      test_rect=[0, 0, 256, 256],
                      browser_args=sw_compositing_args),
        PixelTestPage('pixel_canvas2d_tab_switch.html',
                      base_name + '_Canvas2DTabSwitch',
                      test_rect=[0, 0, 100, 100],
                      test_actions=switch_tab_test_actions),
        PixelTestPage('pixel_canvas2d_tab_switch.html',
                      base_name + '_Canvas2DTabSwitch_SoftwareCompositing',
                      test_rect=[0, 0, 100, 100],
                      browser_args=sw_compositing_args,
                      test_actions=switch_tab_test_actions),
        PixelTestPage('pixel_webgl_copy_image.html',
                      base_name + '_WebGLCopyImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_webgl_read_pixels_tab_switch.html',
                      base_name + '_WebGLReadPixelsTabSwitch',
                      test_rect=[0, 0, 100, 100],
                      test_actions=switch_tab_test_actions),
        PixelTestPage('pixel_webgl_read_pixels_tab_switch.html',
                      base_name +
                      '_WebGLReadPixelsTabSwitch_SoftwareCompositing',
                      test_rect=[0, 0, 100, 100],
                      browser_args=sw_compositing_args,
                      test_actions=switch_tab_test_actions),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_main.html',
                      base_name + '_OffscreenCanvasIBRCWebGLMain',
                      test_rect=[0, 0, 300, 300],
                      test_actions=low_power_test_actions),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_worker.html',
                      base_name + '_OffscreenCanvasIBRCWebGLWorker',
                      test_rect=[0, 0, 300, 300],
                      test_actions=low_power_test_actions),
        PixelTestPage(
            'pixel_webgl_preserved_after_tab_switch.html',
            base_name + '_WebGLPreservedAfterTabSwitch',
            test_rect=[0, 0, 300, 300],
            test_actions=[
                sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
                TestActionSwitchTabsAndCopyImage(),
                sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
            ]),
        PixelTestPage('pixel_svg_huge.html',
                      base_name + '_SVGHuge',
                      test_rect=[0, 0, 400, 400]),
        PixelTestPage('pixel_webgl_display_p3.html',
                      base_name + '_WebGLDisplayP3',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_float.html',
                      base_name + '_WebGLFloat',
                      test_rect=[0, 0, 200, 100],
                      browser_args=experimental_hdr_args),
        PixelTestPage('pixel_offscreenCanvas_ibrc_worker.html',
                      base_name + '_OffscreenCanvasIBRCWorker',
                      test_rect=[0, 0, 100, 100],
                      grace_period_end=date(2023, 8, 5)),
    ]

  @staticmethod
  def WebGPUPages(base_name):
    class Mode(Enum):
      WEBGPU_DEFAULT = 0
      WEBGPU_SWIFTSHADER = 1
      VULKAN_SWIFTSHADER = 2

    def webgpu_pages_helper(base_name, mode):
      webgpu_args = cba.ENABLE_WEBGPU_FOR_TESTING + [
          cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES
      ]
      video_frame_query_params = '?sourceType=hw_decoder'
      if mode == Mode.WEBGPU_SWIFTSHADER:
        base_name += '_WebGPUSwiftShader'
        webgpu_args += [
            '--enable-features=Vulkan', '--use-webgpu-adapter=swiftshader'
        ]
        video_frame_query_params = '?sourceType=sw_decoder'
      elif mode == Mode.VULKAN_SWIFTSHADER:
        base_name += '_VulkanSwiftShader'
        webgpu_args += [
            '--enable-features=Vulkan', '--use-angle=swiftshader',
            '--use-vulkan=swiftshader', '--use-webgpu-adapter=swiftshader',
            '--disable-vulkan-surface'
        ]
        video_frame_query_params = '?sourceType=sw_decoder'

      return [
          PixelTestPage('pixel_webgpu_import_video_frame.html' +
                        video_frame_query_params,
                        base_name + '_WebGPUImportVideoFrame',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage(
              'pixel_webgpu_import_video_frame.html' + video_frame_query_params,
              base_name + '_WebGPUImportVideoFrameUnaccelerated',
              test_rect=[0, 0, 400, 200],
              browser_args=webgpu_args + [cba.DISABLE_ACCELERATED_2D_CANVAS]),
          PixelTestPage(
              'pixel_webgpu_import_video_frame_offscreen_canvas.html' +
              video_frame_query_params,
              base_name + '_WebGPUImportVideoFrameOffscreenCanvas',
              test_rect=[0, 0, 400, 200],
              browser_args=webgpu_args),
          PixelTestPage(
              'pixel_webgpu_import_video_frame_offscreen_canvas.html' +
              video_frame_query_params,
              base_name + '_WebGPUImportVideoFrameUnacceleratedOffscreenCanvas',
              test_rect=[0, 0, 400, 200],
              browser_args=webgpu_args + [cba.DISABLE_ACCELERATED_2D_CANVAS]),
          PixelTestPage('pixel_webgpu_webgl_teximage2d.html',
                        base_name + '_WebGPUWebGLTexImage2D',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_canvas2d_drawimage.html',
                        base_name + '_WebGPUCanvas2DDrawImage',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_image.html',
                        base_name + '_WebGPUToDataURL',
                        test_rect=[0, 0, 400, 300],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_cached_swap_buffer_invalidated.html',
                        base_name +
                        '_WebGPUCachedSwapBufferInvalidatedShouldBeBlank',
                        test_rect=[0, 0, 300, 300],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_externalImage_2d_canvas.html',
                        base_name + '_WebGPUCopyExternalImage2DCanvas',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_externalImage_imageData.html',
                        base_name + '_WebGPUCopyExternalImageImageData',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_externalImage_imageBitmap.html',
                        base_name + '_WebGPUCopyExternalImageImageBitmap',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_externalImage_offscreenCanvas.html',
                        base_name + '_WebGPUCopyExternalImageOffscreenCanvas',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_externalImage_webgl_canvas.html',
                        base_name + '_WebGPUCopyExternalImageWebGLCanvas',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_copy_externalImage_webgpu_canvas.html',
                        base_name + '_WebGPUCopyExternalImageWebGPUCanvas',
                        test_rect=[0, 0, 400, 200],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_display_p3.html',
                        base_name + '_WebGPUDisplayP3',
                        test_rect=[0, 0, 300, 300],
                        browser_args=webgpu_args),
          PixelTestPage('pixel_webgpu_canvas_format_reinterpretation.html',
                        base_name + '_WebGPUCanvasFormatReinterpretation',
                        test_rect=[0, 0, 300, 300],
                        browser_args=webgpu_args),
      ]

    return (webgpu_pages_helper(base_name, mode=Mode.WEBGPU_DEFAULT) +
            webgpu_pages_helper(base_name, mode=Mode.WEBGPU_SWIFTSHADER) +
            webgpu_pages_helper(base_name, mode=Mode.VULKAN_SWIFTSHADER))

  @staticmethod
  def WebGPUCanvasCapturePages(base_name):
    webgpu_args = cba.ENABLE_WEBGPU_FOR_TESTING + [
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES
    ]

    browser_args_canvas_one_copy_capture = webgpu_args + [
        '--enable-features=OneCopyCanvasCapture'
    ]
    other_args_canvas_one_copy_capture = {'one_copy': True}

    browser_args_canvas_disable_one_copy_capture = webgpu_args + [
        '--disable-features=OneCopyCanvasCapture'
    ]
    other_args_canvas_accelerated_two_copy = {
        'one_copy': False,
        'accelerated_two_copy': True
    }

    # Setting grace_period_end to monitor the affects on bots for 2 weeks
    # without making the bots red unexpectedly.
    return [
        # Enabled OneCopyCapture
        PixelTestPage('pixel_webgpu_canvas_capture_to_video.html',
                      base_name + '_WebGPUCanvasOneCopyCapture',
                      test_rect=[0, 0, 400, 200],
                      matching_algorithm=GENERAL_MP4_ALGO,
                      browser_args=browser_args_canvas_one_copy_capture,
                      other_args=other_args_canvas_one_copy_capture),
        PixelTestPage('pixel_webgpu_canvas_capture_to_video.html?hidden=true',
                      base_name + '_WebGPUCanvasOneCopyCapture_Hidden',
                      test_rect=[0, 0, 200, 200],
                      matching_algorithm=GENERAL_MP4_ALGO,
                      browser_args=browser_args_canvas_one_copy_capture,
                      other_args=other_args_canvas_one_copy_capture),
        # Disabled OneCopyCapture + canvas is opaque
        PixelTestPage(
            'pixel_webgpu_canvas_capture_to_video.html?has_alpha=false',
            base_name + '_WebGPUCanvasDisableOneCopyCapture_Accelerated',
            test_rect=[0, 0, 400, 200],
            matching_algorithm=GENERAL_MP4_ALGO,
            browser_args=browser_args_canvas_disable_one_copy_capture,
            other_args=other_args_canvas_accelerated_two_copy,
            grace_period_end=date(2022, 8, 30)),
    ]


  # Pages that should be run with GPU rasterization enabled.
  @staticmethod
  def GpuRasterizationPages(base_name: str) -> List[PixelTestPage]:
    browser_args = [
        cba.ENABLE_GPU_RASTERIZATION,
        cba.DISABLE_SOFTWARE_COMPOSITING_FALLBACK,
    ]
    return [
        PixelTestPage('pixel_background.html',
                      base_name + '_GpuRasterization_BlueBox',
                      test_rect=[0, 0, 220, 220],
                      browser_args=browser_args),
        PixelTestPage('concave_paths.html',
                      base_name + '_GpuRasterization_ConcavePaths',
                      test_rect=[0, 0, 100, 100],
                      browser_args=browser_args),
        PixelTestPage(
            'pixel_precision_rounded_corner.html',
            base_name + '_PrecisionRoundedCorner',
            test_rect=[0, 0, 400, 400],
            browser_args=browser_args,
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=10,
                pixel_delta_threshold=30,
                edge_threshold=100),
            # Small Fuchsia screens result in an incomplete capture
            # without this.
            should_capture_full_screenshot_func=CaptureFullScreenshotOnFuchsia),
    ]

  # Pages that should be run with off-thread paint worklet flags.
  @staticmethod
  def PaintWorkletPages(base_name: str) -> List[PixelTestPage]:
    browser_args = [
        '--enable-blink-features=OffMainThreadCSSPaint',
        '--enable-gpu-rasterization'
    ]

    return [
        PixelTestPage(
            'pixel_paintWorklet_transform.html',
            base_name + '_PaintWorkletTransform',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args),
    ]

  # Pages that should be run with experimental canvas features.
  @staticmethod
  def ExperimentalCanvasFeaturesPages(base_name: str) -> List[PixelTestPage]:
    browser_args = [
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    ]
    accelerated_args = [
        cba.DISABLE_SOFTWARE_COMPOSITING_FALLBACK,
    ]
    unaccelerated_args = [
        cba.DISABLE_ACCELERATED_2D_CANVAS,
        cba.DISABLE_GPU_COMPOSITING,
    ]
    unaccelerated_canvas_accelerated_compositing_args = [
        cba.DISABLE_ACCELERATED_2D_CANVAS,
        cba.DISABLE_SOFTWARE_COMPOSITING_FALLBACK,
    ]

    # The sRGB tests have been observed to create a large number
    # (~15,000) of pixels with difference ~3.
    srgb_fuzzy_algo = algo.FuzzyMatchingAlgorithm(max_different_pixels=20000,
                                                  pixel_delta_threshold=3)

    return [
        PixelTestPage('pixel_offscreenCanvas_transfer_after_style_resize.html',
                      base_name + '_OffscreenCanvasTransferAfterStyleResize',
                      test_rect=[0, 0, 350, 350],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_transfer_before_style_resize.html',
                      base_name + '_OffscreenCanvasTransferBeforeStyleResize',
                      test_rect=[0, 0, 350, 350],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_paint_after_resize.html',
                      base_name + '_OffscreenCanvasWebGLPaintAfterResize',
                      test_rect=[0, 0, 200, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_transferToImageBitmap_main.html',
                      base_name + '_OffscreenCanvasTransferToImageBitmap',
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_transferToImageBitmap_main.html',
            base_name +
            '_OffscreenCanvasTransferToImageBitmapSoftwareCompositing',
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args + unaccelerated_args),
        PixelTestPage('pixel_offscreenCanvas_transferToImageBitmap_worker.html',
                      base_name + '_OffscreenCanvasTransferToImageBitmapWorker',
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_commit_main.html',
                      base_name + '_OffscreenCanvasWebGLDefault',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_commit_worker.html',
                      base_name + '_OffscreenCanvasWebGLDefaultWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_commit_main.html',
                      base_name + '_OffscreenCanvasWebGLSoftwareCompositing',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args +
                      [cba.DISABLE_GPU_COMPOSITING]),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_commit_worker.html',
            base_name + '_OffscreenCanvasWebGLSoftwareCompositingWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + [cba.DISABLE_GPU_COMPOSITING]),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_main.html',
                      base_name + '_OffscreenCanvasAccelerated2D',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + accelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_worker.html',
                      base_name + '_OffscreenCanvasAccelerated2DWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + accelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_main.html',
                      base_name + '_OffscreenCanvasUnaccelerated2D',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + unaccelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_worker.html',
                      base_name + '_OffscreenCanvasUnaccelerated2DWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args + unaccelerated_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_main.html',
                      base_name +
                      '_OffscreenCanvasUnaccelerated2DGPUCompositing',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args +
                      unaccelerated_canvas_accelerated_compositing_args),
        PixelTestPage('pixel_offscreenCanvas_2d_commit_worker.html',
                      base_name +
                      '_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
                      test_rect=[0, 0, 360, 200],
                      browser_args=browser_args +
                      unaccelerated_canvas_accelerated_compositing_args),
        PixelTestPage('pixel_offscreenCanvas_2d_resize_on_worker.html',
                      base_name + '_OffscreenCanvas2DResizeOnWorker',
                      test_rect=[0, 0, 200, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_offscreenCanvas_webgl_resize_on_worker.html',
                      base_name + '_OffscreenCanvasWebglResizeOnWorker',
                      test_rect=[0, 0, 200, 200],
                      browser_args=browser_args),
        PixelTestPage('pixel_canvas_display_srgb.html',
                      base_name + '_CanvasDisplaySRGBAccelerated2D',
                      test_rect=[0, 0, 140, 140],
                      browser_args=browser_args + accelerated_args,
                      matching_algorithm=srgb_fuzzy_algo),
        PixelTestPage('pixel_canvas_display_srgb.html',
                      base_name + '_CanvasDisplaySRGBUnaccelerated2D',
                      test_rect=[0, 0, 140, 140],
                      browser_args=browser_args + unaccelerated_args,
                      matching_algorithm=srgb_fuzzy_algo),
        PixelTestPage(
            'pixel_canvas_display_srgb.html',
            base_name + '_CanvasDisplaySRGBUnaccelerated2DGPUCompositing',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + [cba.DISABLE_ACCELERATED_2D_CANVAS],
            matching_algorithm=srgb_fuzzy_algo),
        PixelTestPage('pixel_webgl_webcodecs_breakoutbox_displays_frame.html',
                      base_name + '_WebGLWebCodecsBreakoutBoxDisplaysFrame',
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args)
    ]

  @staticmethod
  def LowLatencyPages(base_name: str) -> List[PixelTestPage]:
    unaccelerated_args = [
        cba.DISABLE_ACCELERATED_2D_CANVAS,
        cba.DISABLE_GPU_COMPOSITING,
    ]
    return [
        PixelTestPage('pixel_canvas_low_latency_2d.html',
                      base_name + '_CanvasLowLatency2D',
                      test_rect=[0, 0, 100, 100]),
        PixelTestPage('pixel_canvas_low_latency_2d.html',
                      base_name + '_CanvasUnacceleratedLowLatency2D',
                      test_rect=[0, 0, 100, 100],
                      browser_args=unaccelerated_args),
        PixelTestPage('pixel_canvas_low_latency_webgl.html',
                      base_name + '_CanvasLowLatencyWebGL',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_canvas_low_latency_webgl_alpha_false.html',
                      base_name + '_CanvasLowLatencyWebGLAlphaFalse',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_canvas_low_latency_2d_draw_image.html',
                      base_name + '_CanvasLowLatency2DDrawImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_canvas_low_latency_webgl_draw_image.html',
                      base_name + '_CanvasLowLatencyWebGLDrawImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_canvas_low_latency_2d_image_data.html',
                      base_name + '_CanvasLowLatency2DImageData',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_canvas_low_latency_webgl_rounded_corners.html',
                      base_name + '_CanvasLowLatencyWebGLRoundedCorners',
                      test_rect=[0, 0, 100, 100]),
        PixelTestPage('pixel_canvas_low_latency_webgl_occluded.html',
                      base_name + '_CanvasLowLatencyWebGLOccluded',
                      test_rect=[0, 0, 100, 100],
                      other_args={'no_overlay': True}),
    ]

  # Only add these tests on platforms where SwiftShader is enabled.
  # Currently this is Windows and Linux.
  @staticmethod
  def SwiftShaderPages(base_name: str) -> List[PixelTestPage]:
    browser_args = [cba.DISABLE_GPU]
    suffix = '_SwiftShader'
    return [
        PixelTestPage('pixel_canvas2d.html',
                      base_name + '_Canvas2DRedBox' + suffix,
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_css3d.html',
                      base_name + '_CSS3DBlueBox' + suffix,
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name + '_WebGLGreenTriangle_AA_Alpha' + suffix,
                      test_rect=[0, 0, 300, 300],
                      browser_args=browser_args),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D' + suffix,
                      test_rect=[0, 0, 256, 256],
                      browser_args=browser_args),
    ]

  # Test rendering where GPU process is blocked.
  @staticmethod
  def NoGpuProcessPages(base_name: str) -> List[PixelTestPage]:
    browser_args = [cba.DISABLE_GPU, cba.DISABLE_SOFTWARE_RASTERIZER]
    suffix = '_NoGpuProcess'
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args,
            gpu_process_disabled=True),
        PixelTestPage(
            'pixel_css3d.html',
            base_name + '_CSS3DBlueBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args,
            gpu_process_disabled=True),
    ]

  # Pages that should be run with various macOS specific command line
  # arguments.
  @staticmethod
  def MacSpecificPages(base_name: str) -> List[PixelTestPage]:
    unaccelerated_2d_canvas_args = [cba.DISABLE_ACCELERATED_2D_CANVAS]

    non_chromium_image_args = ['--disable-webgl-image-chromium']

    # This disables the Core Animation compositor, falling back to the
    # old GLRenderer path, but continuing to allocate IOSurfaces for
    # WebGL's back buffer.
    no_overlays_args = ['--disable-features=CoreAnimationRenderer']

    angle_gl = ['--use-angle=gl']

    # The filter effect tests produce images with lots of gradients and blurs
    # which don't play nicely with Sobel filters, so a fuzzy algorithm instead
    # of Sobel. The images are also relatively large (360k pixels), and large
    # portions of the image are prone to noise, hence the large max different
    # pixels value.
    filter_effect_fuzzy_algo = algo.FuzzyMatchingAlgorithm(
        max_different_pixels=57500, pixel_delta_threshold=15)

    return [
        PixelTestPage('pixel_canvas2d_webgl.html',
                      base_name + '_IOSurface2DCanvasWebGL',
                      test_rect=[0, 0, 300, 300]),

        # On macOS, test WebGL non-Chromium Image compositing path.
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_AA_Alpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),
        PixelTestPage('pixel_webgl_noaa_alpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_NoAA_Alpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),
        PixelTestPage('pixel_webgl_aa_noalpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_AA_NoAlpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),
        PixelTestPage('pixel_webgl_noaa_noalpha.html',
                      base_name +
                      '_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
                      test_rect=[0, 0, 300, 300],
                      browser_args=non_chromium_image_args),

        # On macOS, test CSS filter effects with and without the CA compositor.
        PixelTestPage('filter_effects.html',
                      base_name + '_CSSFilterEffects',
                      test_rect=[0, 0, 300, 300],
                      matching_algorithm=filter_effect_fuzzy_algo),
        PixelTestPage('filter_effects.html',
                      base_name + '_CSSFilterEffects_NoOverlays',
                      test_rect=[0, 0, 300, 300],
                      browser_args=no_overlays_args,
                      matching_algorithm=filter_effect_fuzzy_algo),

        # Test WebGL's premultipliedAlpha:false without the CA compositor.
        PixelTestPage('pixel_webgl_premultiplied_alpha_false.html',
                      base_name + '_WebGL_PremultipliedAlpha_False_NoOverlays',
                      test_rect=[0, 0, 150, 150],
                      browser_args=no_overlays_args),

        # Test GpuBenchmarking::AddCoreAnimationStatusEventListener.
        # Error code is 0 (gfx::kCALayerSuccess) when it succeeds.
        # --enable-gpu-benchmarking is added by default and it's required to run
        # this test.
        PixelTestPage('core_animation_status_api.html?error=0',
                      base_name + '_CoreAnimationStatusApiNoError',
                      test_rect=[0, 0, 300, 300]),
        # Test GpuBenchmarking::AddCoreAnimationStatusEventListener.
        # Error code is 32 (gfx::kCALayerFailedOverlayDisabled) when
        # CoreAnimationRenderer is disabled.
        # --enable-gpu-benchmarking is added by default and it's required to run
        # this test.
        PixelTestPage('core_animation_status_api.html?error=32',
                      base_name + '_CoreAnimationStatusApiWithError',
                      test_rect=[0, 0, 300, 300],
                      browser_args=no_overlays_args),

        # --enable-gpu-benchmarking is required to run this test. it's added to
        # the pixel tests by default.
        PixelTestPage('canvas_uses_overlay.html',
                      base_name + '_CanvasUsesOverlay',
                      test_rect=[0, 0, 100, 100]),

        # --enable-gpu-benchmarking is required to run this test. it's added to
        # the pixel tests by default.
        PixelTestPage('canvas_uses_overlay.html',
                      base_name + '_UnacceleratedCanvasUsesOverlay',
                      test_rect=[0, 0, 100, 100],
                      browser_args=unaccelerated_2d_canvas_args),

        # --enable-gpu-benchmarking is required to run this test. it's added to
        # the pixel tests by default.
        PixelTestPage(
            'offscreencanvas_imagebitmap_from_worker_uses_overlay.html',
            base_name + '_OffscreenCanvasImageBitmapWorkerUsesOverlay',
            test_rect=[0, 0, 100, 100]),

        # --enable-gpu-benchmarking is required to run this test. it's added to
        # the pixel tests by default.
        PixelTestPage(
            'offscreencanvas_imagebitmap_from_worker_uses_overlay.html',
            base_name +
            '_UnacceleratedOffscreenCanvasImageBitmapWorkerUsesOverlay',
            test_rect=[0, 0, 100, 100],
            browser_args=unaccelerated_2d_canvas_args),

        # --enable-gpu-benchmarking is required to run this test. it's added to
        # the pixel tests by default.
        PixelTestPage('offscreencanvas_imagebitmap_uses_overlay.html',
                      base_name + '_OffscreenCanvasImageBitmapUsesOverlay',
                      test_rect=[0, 0, 100, 100]),

        # --enable-gpu-benchmarking is required to run this test. it's added to
        # the pixel tests by default.
        PixelTestPage('offscreencanvas_imagebitmap_uses_overlay.html',
                      base_name +
                      '_UnacceleratedOffscreenCanvasImageBitmapUsesOverlay',
                      test_rect=[0, 0, 100, 100],
                      browser_args=unaccelerated_2d_canvas_args),

        # Regression test for crbug.com/1410696
        PixelTestPage('pixel_offscreenCanvas_ibrc_worker.html',
                      base_name + '_OffscreenCanvasIBRCWorkerAngleGL',
                      test_rect=[0, 0, 100, 100],
                      browser_args=angle_gl,
                      grace_period_end=date(2023, 8, 5)),
    ]

  # Pages that should be run only on dual-GPU MacBook Pros (at the
  # present time, anyway).
  @staticmethod
  def DualGPUMacSpecificPages(base_name: str) -> List[PixelTestPage]:

    low_to_high_power_test_actions = [
        sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
        TestActionRunLowToHighPowerTest(),
        sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
    ]

    high_perf_test_actions = [
        sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
        TestActionRunOffscreenCanvasIBRCWebGLHighPerfTest(),
        sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
    ]

    return [
        PixelTestPage(
            'pixel_webgl_high_to_low_power.html',
            base_name + '_WebGLHighToLowPower',
            test_rect=[0, 0, 300, 300],
            test_actions=[
                sghitb.TestActionWaitForContinue(SHORT_GLOBAL_TIMEOUT),
                TestActionRunTestWithHighPerformanceTab(),
                sghitb.TestActionWaitForFinish(SHORT_GLOBAL_TIMEOUT),
            ]),
        PixelTestPage('pixel_webgl_low_to_high_power.html',
                      base_name + '_WebGLLowToHighPower',
                      test_rect=[0, 0, 300, 300],
                      test_actions=low_to_high_power_test_actions),
        PixelTestPage('pixel_webgl_low_to_high_power_alpha_false.html',
                      base_name + '_WebGLLowToHighPowerAlphaFalse',
                      test_rect=[0, 0, 300, 300],
                      test_actions=low_to_high_power_test_actions),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_main.html',
                      base_name + '_OffscreenCanvasIBRCWebGLHighPerfMain',
                      test_rect=[0, 0, 300, 300],
                      test_actions=high_perf_test_actions),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_worker.html',
                      base_name + '_OffscreenCanvasIBRCWebGLHighPerfWorker',
                      test_rect=[0, 0, 300, 300],
                      test_actions=high_perf_test_actions),
    ]

  @staticmethod
  def DirectCompositionPages(base_name: str) -> List[PixelTestPage]:
    browser_args = [
        cba.ENABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS,
        # All bots are connected with a power source, however, we want to to
        # test with the code path that's enabled with battery power.
        cba.DISABLE_DIRECT_COMPOSITION_VP_SCALING,
        # This feature ensures that addSwapCompletionEventListener in
        # gpu_benchmarking only sends completion event on a succdessful commit.
        '--enable-features=ReportFCPOnlyOnSuccessfulCommit',
    ]
    browser_args_NV12 = browser_args + [
        '--direct-composition-video-swap-chain-format=nv12'
    ]
    browser_args_YUY2 = browser_args + [
        '--direct-composition-video-swap-chain-format=yuy2'
    ]
    browser_args_BGRA = browser_args + [
        '--direct-composition-video-swap-chain-format=bgra'
    ]
    browser_args_vp_scaling = [
        cba.ENABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS,
        cba.ENABLE_DIRECT_COMPOSITION_VP_SCALING,
    ]
    browser_args_sw_decode = browser_args + [
        cba.DISABLE_ACCELERATED_VIDEO_DECODE
    ]

    # Most tests fall roughly into 3 tiers of noisiness.
    # Parameter values were determined using the automated optimization script,
    # and similar values combined into a single set using the most permissive
    # value for each parameter in that tier.
    strict_dc_sobel_algorithm = algo.SobelMatchingAlgorithm(
        max_different_pixels=1000,
        pixel_delta_threshold=5,
        edge_threshold=250,
        ignored_border_thickness=1)
    permissive_dc_sobel_algorithm = algo.SobelMatchingAlgorithm(
        max_different_pixels=16800,
        pixel_delta_threshold=20,
        edge_threshold=30,
        ignored_border_thickness=1)
    very_permissive_dc_sobel_algorithm = algo.SobelMatchingAlgorithm(
        max_different_pixels=30400,
        pixel_delta_threshold=45,
        edge_threshold=10,
        ignored_border_thickness=1,
    )

    return [
        PixelTestPage('pixel_video_mp4.html?width=240&height=135&swaps=12',
                      base_name + '_DirectComposition_Video_MP4',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html?width=960&height=540',
                      base_name + '_DirectComposition_Video_MP4_Fullsize',
                      browser_args=browser_args,
                      other_args={'full_size': True},
                      test_rect=[0, 0, 960, 540],
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_MP4_NV12',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_NV12,
                      other_args={'pixel_format': 'NV12'},
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_MP4_YUY2',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_YUY2,
                      other_args={'pixel_format': 'YUY2'},
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html?width=960&height=540',
                      base_name + '_DirectComposition_Video_MP4_BGRA',
                      test_rect=[0, 0, 960, 540],
                      browser_args=browser_args_BGRA,
                      other_args={'pixel_format': 'BGRA'},
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_MP4_VP_SCALING',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_vp_scaling,
                      other_args={'zero_copy': False},
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_mp4_four_colors_aspect_4x3.html?width=240&height=135',
            base_name + '_DirectComposition_Video_MP4_FourColors_Aspect_4x3',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_90.html?width=270&height=240',
            base_name + '_DirectComposition_Video_MP4_FourColors_Rot_90',
            test_rect=[0, 0, 270, 240],
            browser_args=browser_args,
            other_args={'video_is_rotated': True},
            matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_180.html?'
            'width=240&height=135&swaps=12',
            base_name + '_DirectComposition_Video_MP4_FourColors_Rot_180',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            other_args={'video_is_rotated': True},
            matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_270.html?'
            'width=270&height=240',
            base_name + '_DirectComposition_Video_MP4_FourColors_Rot_270',
            test_rect=[0, 0, 270, 240],
            browser_args=browser_args,
            other_args={'video_is_rotated': True},
            matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_VP9',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_vp9.html?width=960&height=540',
            base_name + '_DirectComposition_Video_VP9_Fullsize',
            test_rect=[0, 0, 960, 540],
            browser_args=browser_args,
            other_args={'full_size': True},
            # Much larger image than other VP9 tests.
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=504000,
                pixel_delta_threshold=10,
                edge_threshold=10,
                ignored_border_thickness=1,
            )),
        PixelTestPage('pixel_video_vp9.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_VP9_NV12',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_NV12,
                      other_args={
                          'pixel_format': 'NV12',
                      },
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_VP9_YUY2',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_YUY2,
                      other_args={'pixel_format': 'YUY2'},
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9.html?width=960&height=540&swaps=12',
                      base_name + '_DirectComposition_Video_VP9_BGRA',
                      test_rect=[0, 0, 960, 540],
                      browser_args=browser_args_BGRA,
                      other_args={'pixel_format': 'BGRA'},
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9_i420a.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_VP9_I420A',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage('pixel_video_vp9.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_VP9_VP_SCALING',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_vp_scaling,
                      other_args={'zero_copy': False},
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_underlay.html?width=240&height=136&swaps=16',
                      base_name + '_DirectComposition_Underlay',
                      test_rect=[0, 0, 240, 136],
                      browser_args=browser_args,
                      matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_underlay.html?width=960&height=540&swaps=12',
                      base_name + '_DirectComposition_Underlay_Fullsize',
                      test_rect=[0, 0, 960, 540],
                      browser_args=browser_args,
                      other_args={'full_size': True},
                      matching_algorithm=strict_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_video_mp4_rounded_corner.html?width=240&height=135',
            base_name + '_DirectComposition_Video_MP4_Rounded_Corner',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            matching_algorithm=permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_backdrop_filter.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_BackdropFilter',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True}),
        PixelTestPage(
            'pixel_video_mp4.html?width=240&height=135',
            base_name + '_DirectComposition_Video_Disable_Overlays',
            test_rect=[0, 0, 240, 135],
            browser_args=[cba.DISABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS],
            other_args={'no_overlay': True},
            matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage('pixel_video_mp4.html?width=240&height=135',
                      base_name + '_DirectComposition_Video_SW_Decode',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args_sw_decode,
                      matching_algorithm=very_permissive_dc_sobel_algorithm),
        PixelTestPage(
            'pixel_media_foundation_clear_dcomp.html?src='
            '/media/test/data/four-colors.mp4',
            base_name + '_MediaFoundationClearDirectComposition',
            test_rect=[0, 0, 256, 256],
            browser_args=[
                '--enable-features=MediaFoundationClearPlayback, \
                MediaFoundationClearRendering:strategy/direct-composition'
            ],
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO,
            grace_period_end=date(2022, 10, 24)),
    ]

  @staticmethod
  def VideoFromCanvasPages(base_name: str) -> List[PixelTestPage]:
    # Tests for <video> element rendering results of <canvas> capture.
    # It's important for video conference software.

    match_algo = VERY_PERMISSIVE_SOBEL_ALGO
    # Use shorter timeout since the tests are not supposed to be long.
    timeout = 150
    test_rect = [0, 0, 200, 200]
    grace_period_end = date(2022, 10, 20)

    return [
        PixelTestPage('pixel_video_from_canvas_2d.html',
                      base_name + '_VideoStreamFrom2DCanvas',
                      test_rect=test_rect,
                      browser_args=[],
                      matching_algorithm=match_algo,
                      grace_period_end=grace_period_end,
                      timeout=timeout),
        PixelTestPage('pixel_video_from_canvas_2d_alpha.html',
                      base_name + '_VideoStreamFrom2DAlphaCanvas',
                      test_rect=test_rect,
                      browser_args=[],
                      matching_algorithm=match_algo,
                      grace_period_end=grace_period_end,
                      timeout=timeout),
        PixelTestPage('pixel_video_from_canvas_webgl2_alpha.html',
                      base_name + '_VideoStreamFromWebGLAlphaCanvas',
                      test_rect=test_rect,
                      browser_args=[],
                      matching_algorithm=match_algo,
                      grace_period_end=grace_period_end,
                      timeout=timeout),
        PixelTestPage('pixel_video_from_canvas_webgl2.html',
                      base_name + '_VideoStreamFromWebGLCanvas',
                      test_rect=test_rect,
                      browser_args=[],
                      matching_algorithm=match_algo,
                      grace_period_end=grace_period_end,
                      timeout=timeout),

        # Safeguard against repeating crbug.com/1337101
        PixelTestPage(
            'pixel_video_from_canvas_2d_alpha.html',
            base_name + '_VideoStreamFrom2DAlphaCanvas_DisableOOPRaster',
            test_rect=test_rect,
            browser_args=['--disable-features=CanvasOopRasterization'],
            matching_algorithm=match_algo,
            grace_period_end=grace_period_end,
            timeout=timeout),

        # Safeguard against repeating crbug.com/1371308
        PixelTestPage(
            'pixel_video_from_canvas_2d.html',
            base_name +
            '_VideoStreamFrom2DAlphaCanvas_DisableReadbackFromTexture',
            test_rect=test_rect,
            browser_args=[
                '--disable-features=GpuMemoryBufferReadbackFromTexture'
            ],
            matching_algorithm=match_algo,
            grace_period_end=grace_period_end,
            timeout=timeout),

        # Test OneCopyCanvasCapture
        PixelTestPage('pixel_video_from_canvas_webgl2.html',
                      base_name + '_VideoStreamFromWebGLCanvas_OneCopy',
                      test_rect=test_rect,
                      browser_args=['--enable-features=OneCopyCanvasCapture'],
                      other_args={'one_copy': True},
                      matching_algorithm=match_algo,
                      grace_period_end=grace_period_end,
                      timeout=timeout),
        # TwoCopyCanvasCapture
        PixelTestPage('pixel_video_from_canvas_webgl2.html',
                      base_name +
                      '_VideoStreamFromWebGLCanvas_TwoCopy_Accelerated',
                      test_rect=test_rect,
                      browser_args=['--disable-features=OneCopyCanvasCapture'],
                      other_args={
                          'one_copy': False,
                          'accelerated_two_copy': True
                      },
                      matching_algorithm=match_algo,
                      grace_period_end=grace_period_end,
                      timeout=timeout),
    ]

  @staticmethod
  def HdrTestPages(base_name: str) -> List[PixelTestPage]:
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBoxScrgbLinear',
            test_rect=[0, 0, 300, 300],
            browser_args=['--force-color-profile=scrgb-linear']),
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBoxHdr10',
            test_rect=[0, 0, 300, 300],
            browser_args=['--force-color-profile=hdr10']),
    ]

  # Check that the root swap chain claims to be opaque. A root swap chain with a
  # premultiplied alpha mode has a large negative battery impact (even if all
  # the pixels are opaque).
  @staticmethod
  def RootSwapChainPages(base_name: str) -> List[PixelTestPage]:
    return [
        PixelTestPage('wait_for_compositing.html',
                      base_name + '_IsOpaque',
                      test_rect=[0, 0, 0, 0],
                      other_args={
                          'has_alpha': False,
                      }),
    ]

  # This should only be used with the cast_streaming suite.
  @staticmethod
  def CastStreamingReceiverPages(base_name):
    return [
        PixelTestPage('receiver.html',
                      base_name + '_VP8_1Frame',
                      test_rect=[0, 0, 0, 0]),
    ]
