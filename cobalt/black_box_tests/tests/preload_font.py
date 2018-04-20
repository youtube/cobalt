"""Open Cobalt in preload mode and find a basic element."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import time

import _env  # pylint: disable=unused-import

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.threaded_web_server import ThreadedWebServer

_MAX_RESUME_WAIT_SECONDS = 30


class PreloadFontTest(black_box_tests.BlackBoxTestCase):

  def test_simple(self):

    with ThreadedWebServer() as server:
      url = server.GetURL(file_name='testdata/preload_font.html')

      with self.CreateCobaltRunner(
          url=url, target_params=['--preload']) as runner:
        runner.WaitForJSTestsSetup()
        runner.SendResume()
        start_time = time.time()
        while runner.IsInPreload():
          if time.time() - start_time > _MAX_RESUME_WAIT_SECONDS:
            raise Exception('Cobalt can not exit preload mode after receiving'
                            'resume signal')
          time.sleep(.1)
        # At this point, Cobalt is in started mode.
        self.assertTrue(runner.JSTestsSucceeded())
