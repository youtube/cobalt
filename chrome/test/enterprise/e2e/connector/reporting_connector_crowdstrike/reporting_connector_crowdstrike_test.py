# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime

from chrome_ent_test.infra.core import before_all
from chrome_ent_test.infra.core import category
from chrome_ent_test.infra.core import environment
from chrome_ent_test.infra.core import test

from .. import ChromeReportingConnectorTestCase, VerifyContent
from .crowdstrike_humio_api_service import CrowdStrikeHumioApiService

@category("chrome_only")
@environment(file="../connector_test.asset.textpb")
class ReportingConnectorwithCrowdStrikeTest(ChromeReportingConnectorTestCase):

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
    apiService = CrowdStrikeHumioApiService(
        self.GetFileFromGCSBucket('secrets/humio_user_token'))
    self.TryVerifyUntilTimeout(
        verifyClass=apiService,
        content=VerifyContent(deviceId=deviceId, timestamp=testStartTime))
