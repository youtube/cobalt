# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import time

from .verifyable import Verifyable
from .verifyContent import VerifyContent
from infra import ChromeEnterpriseTestCase


class ChromeReportingConnectorTestCase(ChromeEnterpriseTestCase):
  """Test the Realtime Reporting pipeline events"""

  def GetFileFromGCSBucket(self, path):
    """Get file from GCS bucket"""
    path = "gs://%s/%s" % (self.gsbucket, path)
    cmd = r'gsutil cat ' + path
    return self.RunCommand(self.win_config['dc'], cmd).rstrip().decode()

  def InstallBrowserAndEnableUITest(self):
    """Install chrome on machine and enable ui test.
      This is for the before all step"""
    self.EnableUITest(self.win_config['client'])
    self.InstallChrome(self.win_config['client'])

  def GetCELabDefaultToken(self):
    """Get default celab org enrollment token from GCS bucket"""
    return self.GetFileFromGCSBucket('secrets/CELabOrg-enrollToken')

  def EnrollBrowserToDomain(self, token):
    """Enroll browser to test domain"""
    self.SetPolicy(self.win_config['dc'], r'CloudManagementEnrollmentToken',
                   token, 'String')

  def EnableSafeBrowsing(self):
    """Enable safe browsing policy"""
    self.SetPolicy(self.win_config['dc'], r'SafeBrowsingEnabled', 1, 'DWORD')

  def UpdatePoliciesOnClient(self):
    """Refetch policies values on client"""
    self.RunCommand(self.win_config['client'], 'gpupdate /force')

  def TriggerUnsafeBrowsingEvent(self):
    """Run UI script to trigger safe browsing event and return deviceId"""
    localDir = os.path.dirname(os.path.abspath(__file__))
    deviceId = self.RunUITest(
        self.win_config['client'],
        os.path.join(localDir, 'common', 'realtime_reporting_ui_test.py'),
        timeout=600)
    deviceId = re.search(r'DeviceId:.*$',
                         deviceId.strip()).group(0).replace('DeviceId:',
                                                            '').rstrip("\\rn'")
    return deviceId

  def TryVerifyUntilTimeout(self,
                            verifyClass: Verifyable,
                            content: VerifyContent,
                            timeout: int = 600,
                            interval: int = 10):
    """Calls verifyClass.tryVerify every interval until it returns true or
    Until timeout is reached"""
    # wait until events are logged in the connector
    max_wait_time_secs = timeout
    delta_secs = interval
    total_wait_time_secs = 0

    while total_wait_time_secs < max_wait_time_secs:
      if verifyClass.TryVerify(content):
        print('TryVerify succeeded after %d seconds' % total_wait_time_secs)
        return
      time.sleep(delta_secs)
      total_wait_time_secs += delta_secs

    raise Exception('TryVerify reached timeout without being true')
