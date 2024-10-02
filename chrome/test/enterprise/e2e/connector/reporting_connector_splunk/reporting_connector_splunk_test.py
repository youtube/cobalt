# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test
from .. import ChromeReportingConnectorTestCase, VerifyContent
from .splunk_server import SplunkApiService

@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorwithSplunkTest(ChromeReportingConnectorTestCase):
  """Test the Realtime Reporting pipeline events"""

  @before_all
  def setup(self):
    self.InstallBrowserAndEnableUITest()

  @test
  def test_browser_enrolled_prod(self):
    token = self.GetCELabDefaultToken()
    self.EnrollBrowserToDomain(token)
    self.EnableSafeBrowsing()
    self.UpdatePoliciesOnClient()
    testStartTime = datetime.utcnow()

    # trigger malware event & get device id from browser
    deviceId = self.TriggerUnsafeBrowsingEvent()

    # read service account private key from gs-bucket & write into local
    apiService = SplunkApiService(
        self.GetFileFromGCSBucket('secrets/splunkInstances.json'))
    self.TryVerifyUntilTimeout(
        verifyClass=apiService,
        content=VerifyContent(deviceId=deviceId, timestamp=testStartTime))
