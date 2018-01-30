# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Starboard Android Cobalt shared configuration."""

import os

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltAndroidConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard Android Cobalt shared configuration."""

  def __init__(self, platform_configuration, application_name,
               application_directory):
    super(CobaltAndroidConfiguration, self).__init__(
        platform_configuration, application_name, application_directory)

  def GetPostIncludes(self):
    # If there isn't a configuration.gypi found in the usual place, we'll
    # supplement with our shared implementation.
    includes = super(CobaltAndroidConfiguration, self).GetPostIncludes()
    for include in includes:
      if os.path.basename(include) == 'configuration.gypi':
        return includes

    shared_gypi_path = os.path.join(os.path.dirname(__file__),
                                    'configuration.gypi')
    if os.path.isfile(shared_gypi_path):
      includes.append(shared_gypi_path)
    return includes

  def GetTestFilters(self):
    filters = super(CobaltAndroidConfiguration, self).GetTestFilters()
    for target, tests in self._FAILING_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'},
        'crypto_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'},
        'net_unittests': {'ASAN_OPTIONS': 'detect_leaks=0'}
    }

  # A map of failing or crashing tests per target.
  _FAILING_TESTS = {
      'base_unittests': [
          'JSONReaderTest.ReadFromFile',
          'PathServiceTest.Get',
      ],
      'browser_test': [
          'StorageUpgradeHandlerTest.UpgradeFullData',
      ],
      'css_parser_test': [
          'ScannerTest.ScansUtf8Identifier',
      ],
      # TODO: Include dir_src_root in the APK assets for layout_tests
      'layout_tests': [test_filter.FILTER_ALL],
      'loader_test': [
          'FileFetcherTest.EmptyFile',
          'FileFetcherTest.ReadWithOffset',
          'FileFetcherTest.ReadWithOffsetAndSize',
          'FileFetcherTest.ValidFile',
          'ImageDecoderTest.DecodeAnimatedWEBPImage',
          'ImageDecoderTest.DecodeAnimatedWEBPImageWithMultipleChunks',
          'ImageDecoderTest.DecodeInterlacedPNGImage',
          'ImageDecoderTest.DecodeInterlacedPNGImageWithMultipleChunks',
          'ImageDecoderTest.DecodeJPEGImage',
          'ImageDecoderTest.DecodeJPEGImageWithMultipleChunks',
          'ImageDecoderTest.DecodePNGImage',
          'ImageDecoderTest.DecodePNGImageWithMultipleChunks',
          'ImageDecoderTest.DecodeProgressiveJPEGImage',
          'ImageDecoderTest.DecodeProgressiveJPEGImageWithMultipleChunks',
          'ImageDecoderTest.DecodeWEBPImage',
          'ImageDecoderTest.DecodeWEBPImageWithMultipleChunks',
          'LoaderTest.ValidFileEndToEndTest',
          'MeshDecoderTest.DecodeMesh',
          'MeshDecoderTest.DecodeMeshWithMultipleChunks',
          'TypefaceDecoderTest.DecodeTooLargeTypeface',
          'TypefaceDecoderTest.DecodeTooLargeTypefaceWithMultipleChunks',
          'TypefaceDecoderTest.DecodeTtfTypeface',
          'TypefaceDecoderTest.DecodeTtfTypefaceWithMultipleChunks',
          'TypefaceDecoderTest.DecodeWoffTypeface',
          'TypefaceDecoderTest.DecodeWoffTypefaceWithMultipleChunks',
      ],
      'net_unittests': [
          'DialHttpServerTest.AllOtherRequests',
          'DialHttpServerTest.CallbackExceptionInServiceHandler',
          'DialHttpServerTest.CallbackHandleRequestReturnsFalse',
          'DialHttpServerTest.CallbackNormalTest',
          'DialHttpServerTest.CurrentRunningAppRedirect',
          'DialHttpServerTest.SendManifest',
          'GZipUnitTest.DecodeCorruptedData',
          'GZipUnitTest.DecodeCorruptedHeader',
          'GZipUnitTest.DecodeDeflate',
          'GZipUnitTest.DecodeGZip',
          'GZipUnitTest.DecodeMissingData',
          'GZipUnitTest.DecodeWithOneByteBuffer',
          'GZipUnitTest.DecodeWithOneByteInputAndOutputBuffer',
          'GZipUnitTest.DecodeWithSmallBuffer',
          'GZipUnitTest.DecodeWithSmallOutputBuffer',
          'HostResolverImplDnsTest.DnsTaskUnspec',
          'MultiThreadedCertVerifierTest.CacheHit',
          'MultiThreadedCertVerifierTest.CancelRequest',
          'MultiThreadedCertVerifierTest.CancelRequestThenQuit',
          'MultiThreadedCertVerifierTest.DifferentCACerts',
          'MultiThreadedCertVerifierTest.InflightJoin',
          'TransportSecurityStateTest.BogusPinsHeadersSHA1',
          'TransportSecurityStateTest.BogusPinsHeadersSHA256',
          'TransportSecurityStateTest.ValidPinsHeadersSHA1',
          'TransportSecurityStateTest.ValidPinsHeadersSHA256',
          'X509CertificateParseTest.CanParseFormat/0',
          'X509CertificateParseTest.CanParseFormat/1',
          'X509CertificateParseTest.CanParseFormat/2',
          'X509CertificateParseTest.CanParseFormat/3',
          'X509CertificateParseTest.CanParseFormat/4',
          'X509CertificateParseTest.CanParseFormat/5',
          'X509CertificateParseTest.CanParseFormat/6',
          'X509CertificateParseTest.CanParseFormat/7',
          'X509CertificateParseTest.CanParseFormat/8',
          'X509CertificateParseTest.CanParseFormat/9',
          'X509CertificateParseTest.CanParseFormat/10',
          'X509CertificateParseTest.CanParseFormat/11',
          'X509CertificateParseTest.CanParseFormat/12',
          'X509CertificateTest.CAFingerprints',
          'X509CertificateTest.ExtractCRLURLsFromDERCert',
          'X509CertificateTest.ExtractSPKIFromDERCert',
          'X509CertificateTest.MultivalueRDN',
          'X509CertificateTest.ParseSubjectAltNames',
          'X509CertificateTest.UnescapedSpecialCharacters',
      ],
      'renderer_test': [
          # TODO: Include dir_src_root in the APK assets for renderer_test
          'PixelTest.*',
          'StressTest.TooManyTextures',
      ],
      'storage_test': [
          'StorageUpgradeTest.UpgradeMinimalCookie',
          'StorageUpgradeTest.UpgradeMinimalLocalStorageEntry',
          'StorageUpgradeTest.UpgradeFullData',
          'StorageUpgradeTest.UpgradeMissingFields',
          'StorageUpgradeTest.UpgradeMalformed',
          'StorageUpgradeTest.UpgradeExtraFields',
      ],
      # TODO: Include dir_src_root in the APK assets for web_platform_tests
      'web_platform_tests': [test_filter.FILTER_ALL],
      'webdriver_test': [
          'IsDisplayedTest.*',
      ],
  }
