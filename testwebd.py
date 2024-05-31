from cobalt.tools.automated_testing import webdriver_utils
import time

webdriver_url = f'http://127.0.0.1:4444/'

COBALT_WEBDRIVER_CAPABILITIES = {
    'browserName': 'cobalt',
    'javascriptEnabled': True,
    'platform': 'LINUX'
}


def test_conn(delay):
  selenium_webdriver_module = webdriver_utils.import_selenium_module(
      'webdriver')
  rc = selenium_webdriver_module.remote.remote_connection
  executor = rc.RemoteConnection(webdriver_url)
  executor.set_timeout(2)
  webdriver = selenium_webdriver_module.Remote(executor,
                                               COBALT_WEBDRIVER_CAPABILITIES)
  time.sleep(delay)
  print("{!r}".format(webdriver.window_handles))
  time.sleep(delay)
  # webdriver.switch_to.window("window-0")
  shot = webdriver.get_screenshot_as_base64()
  webdriver.quit()


if __name__ == "__main__":
  test_conn(0.5)
