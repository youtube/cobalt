import os
import subprocess
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
ARCORE_DIR = os.path.abspath(os.path.join(THIS_DIR, 'arcore'))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
PLAYBACK_DATASET_DIR = os.path.abspath(os.path.join(
    CHROMIUM_SRC_DIR, 'chrome', 'test', 'data', 'xr', 'ar_playback_datasets'))

def main():
  # TODO(https://crbug.com/853898): Rename this to DOWNLOAD_XR_TEST_APKS
  if 'DOWNLOAD_VR_TEST_APKS' not in os.environ:
    return 0
  subprocess.check_call([
      'download_from_google_storage',
      '--bucket', 'chromium-ar-test-apks/arcore',
      '-d', ARCORE_DIR])
  subprocess.check_call([
      'download_from_google_storage',
      '--bucket', 'chromium-ar-test-apks/playback_datasets',
      '-d', PLAYBACK_DATASET_DIR])
  return 0

if __name__ == '__main__':
  sys.exit(main())
