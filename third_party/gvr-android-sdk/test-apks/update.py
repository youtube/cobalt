import os
import subprocess
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
DAYDREAM_DIR = os.path.abspath(os.path.join(THIS_DIR, 'daydream_home'))
VR_SERVICES_DIR = os.path.abspath(os.path.join(THIS_DIR, 'vr_services'))
VR_KEYBOARD_DIR = os.path.abspath(os.path.join(THIS_DIR, 'vr_keyboard'))

def main():
  if 'DOWNLOAD_VR_TEST_APKS' not in os.environ:
    return 0
  subprocess.check_call(['download_from_google_storage',
                                '--bucket', 'chrome-vr-test-apks/daydream_home',
                                '-d', DAYDREAM_DIR])
  subprocess.check_call(['download_from_google_storage',
                                 '--bucket', 'chrome-vr-test-apks/vr_services',
                                 '-d', VR_SERVICES_DIR])
  subprocess.check_call(['download_from_google_storage',
                                 '--bucket', 'chrome-vr-test-apks/vr_keyboard',
                                 '-d', VR_KEYBOARD_DIR])
  return 0

if __name__ == '__main__':
  sys.exit(main())
