# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Finds android browsers that can be started and controlled by telemetry."""

from __future__ import absolute_import
import contextlib
import logging
import os
import platform
import posixpath
import shutil
import subprocess

from devil import base_error
from devil.android import apk_helper
from devil.android import flag_changer
from devil.android.sdk import version_codes
from py_utils import dependency_util
from py_utils import file_util
from py_utils import tempfile_ext
from telemetry import compat_mode_options
from telemetry import decorators
from telemetry.core import exceptions
from telemetry.core import platform as telemetry_platform
from telemetry.core import util
from telemetry.internal.backends import android_browser_backend_settings
from telemetry.internal.backends.chrome import android_browser_backend
from telemetry.internal.backends.chrome import chrome_startup_args
from telemetry.internal.browser import browser
from telemetry.internal.browser import possible_browser
from telemetry.internal.platform import android_device
from telemetry.internal.util import binary_manager
from telemetry.internal.util import format_for_logging
from telemetry.internal.util import local_first_binary_manager


ANDROID_BACKEND_SETTINGS = (
    android_browser_backend_settings.ANDROID_BACKEND_SETTINGS)


@contextlib.contextmanager
def _ProfileWithExtraFiles(profile_dir, profile_files_to_copy):
  """Yields a temporary directory populated with input files.

  Args:
    profile_dir: A directory whose contents will be copied to the output
      directory.
    profile_files_to_copy: A list of (source, dest) tuples to be copied to
      the output directory.

  Yields: A path to a temporary directory, named "_default_profile". This
    directory will be cleaned up when this context exits.
  """
  with tempfile_ext.NamedTemporaryDirectory() as tempdir:
    # TODO(csharrison): "_default_profile" was chosen because this directory
    # will be pushed to the device's sdcard. We don't want to choose a
    # random name due to the extra failure mode of filling up the sdcard
    # in the case of unclean test teardown. We should consider changing
    # PushProfile to avoid writing to this intermediate location.
    host_profile = os.path.join(tempdir, '_default_profile')
    if profile_dir:
      shutil.copytree(profile_dir, host_profile)
    else:
      os.mkdir(host_profile)

    # Add files from |profile_files_to_copy| into the host profile
    # directory. Don't copy files if they already exist.
    for source, dest in profile_files_to_copy:
      host_path = os.path.join(host_profile, dest)
      if not os.path.exists(host_path):
        file_util.CopyFileWithIntermediateDirectories(source, host_path)
    yield host_profile


class PossibleAndroidBrowser(possible_browser.PossibleBrowser):
  """A launchable android browser instance."""

  def __init__(self, browser_type, finder_options, android_platform,
               backend_settings, local_apk=None, target_os='android'):
    super().__init__(
        browser_type, target_os, backend_settings.supports_tab_control)
    assert browser_type in FindAllBrowserTypes(), (
        'Please add %s to android_browser_finder.FindAllBrowserTypes' %
        browser_type)
    self._platform = android_platform
    self._platform_backend = (
        android_platform._platform_backend)  # pylint: disable=protected-access
    self._backend_settings = backend_settings
    self._local_apk = local_apk
    self._flag_changer = None
    self._modules_to_install = None
    self._compile_apk = finder_options.compile_apk
    self._finder_options = finder_options
    self._browser_package = None

    if self._local_apk is None and finder_options.chrome_root is not None:
      self._local_apk = self._backend_settings.FindLocalApk(
          self._platform_backend.device, finder_options.chrome_root)

    # At this point the local_apk, if any, must exist.
    assert self._local_apk is None or os.path.exists(self._local_apk)
    self._build_dir = util.GetBuildDirFromHostApkPath(self._local_apk)

    if finder_options.modules_to_install:
      self._modules_to_install = set(['base'] +
                                     finder_options.modules_to_install)

    self._support_apk_list = []
    if (self._backend_settings.requires_embedder or
        self._backend_settings.has_additional_apk):
      if finder_options.webview_embedder_apk:
        self._support_apk_list = finder_options.webview_embedder_apk
      else:
        self._support_apk_list = self._backend_settings.FindSupportApks(
            self._local_apk, finder_options.chrome_root)
    elif finder_options.webview_embedder_apk:
      logging.warning(
          'No embedder needed for %s, ignoring --webview-embedder-apk option',
          self._backend_settings.browser_type)

    # At this point the apks in _support_apk_list, if any, must exist.
    for apk in self._support_apk_list:
      assert os.path.exists(apk)

  def __repr__(self):
    return 'PossibleAndroidBrowser(browser_type=%s)' % self.browser_type

  @property
  def settings(self):
    """Get the backend_settings for this possible browser."""
    return self._backend_settings

  @property
  def browser_package(self):
    if not self._browser_package:
      self._browser_package = self._backend_settings.package
      if self._backend_settings.IsWebView():
        self._browser_package = (
            self._backend_settings.
            GetEmbedderPackageName(self._finder_options))
    return self._browser_package

  @property
  def browser_directory(self):
    # On Android L+ the directory where base APK resides is also used for
    # keeping extracted native libraries and .odex. Here is an example layout:
    # /data/app/$package.apps.chrome-1/
    #                                  base.apk
    #                                  lib/arm/libchrome.so
    #                                  oat/arm/base.odex
    # Declaring this toplevel directory as 'browser_directory' allows the cold
    # startup benchmarks to flush OS pagecache for the native library, .odex and
    # the APK.
    apks = self._platform_backend.device.GetApplicationPaths(
        self.browser_package)
    # A package can map to multiple APKs if the package overrides the app on
    # the system image. Such overrides should not happen on perf bots. The
    # package can also map to multiple apks if splits are used. In all cases, we
    # want the directory that contains base.apk.
    for apk in apks:
      if apk.endswith('/base.apk'):
        return apk[:-9]
    return None

  @property
  def profile_directory(self):
    return self._platform_backend.GetProfileDir(self.browser_package)

  @property
  def last_modification_time(self):
    if self._local_apk:
      return os.path.getmtime(self._local_apk)
    return -1

  def _GetPathsForOsPageCacheFlushing(self):
    return [self.profile_directory, self.browser_directory]

  def _InitPlatformIfNeeded(self):
    pass

  def _SetupProfile(self):
    if self._browser_options.dont_override_profile:
      return

    # Just remove the existing profile if we don't have any files to copy over.
    # This is because PushProfile does not support pushing completely empty
    # directories.
    profile_files_to_copy = self._browser_options.profile_files_to_copy
    if not self._browser_options.profile_dir and not profile_files_to_copy:
      self._platform_backend.RemoveProfile(
          self.browser_package,
          self._backend_settings.profile_ignore_list)
      return

    with _ProfileWithExtraFiles(self._browser_options.profile_dir,
                                profile_files_to_copy) as profile_dir:
      self._platform_backend.PushProfile(self.browser_package,
                                         profile_dir)

  def SetUpEnvironment(self, browser_options):
    super().SetUpEnvironment(browser_options)
    self._platform_backend.DismissCrashDialogIfNeeded()
    device = self._platform_backend.device
    startup_args = self.GetBrowserStartupArgs(self._browser_options)
    device.adb.Logcat(clear=True)

    # use legacy commandline path if in compatibility mode
    self._flag_changer = flag_changer.FlagChanger(
        device, self._backend_settings.command_line_name, use_legacy_path=
        compat_mode_options.LEGACY_COMMAND_LINE_PATH in
        browser_options.compatibility_mode)
    self._flag_changer.ReplaceFlags(startup_args, log_flags=False)
    formatted_args = format_for_logging.ShellFormat(
        startup_args, trim=browser_options.trim_logs)
    logging.info('Flags set on device were %s', formatted_args)
    # Stop any existing browser found already running on the device. This is
    # done *after* setting the command line flags, in case some other Android
    # process manages to trigger Chrome's startup before we do.
    self._platform_backend.StopApplication(self.browser_package)
    self._SetupProfile()

    # Remove any old crash dumps
    self._platform_backend.device.RemovePath(
        self._platform_backend.GetDumpLocation(self.browser_package),
        recursive=True, force=True)

  def _TearDownEnvironment(self):
    self._RestoreCommandLineFlags()

  def _RestoreCommandLineFlags(self):
    if self._flag_changer is not None:
      try:
        self._flag_changer.Restore()
      finally:
        self._flag_changer = None

  def Create(self):
    """Launch the browser on the device and return a Browser object."""
    return self._GetBrowserInstance(existing=False)

  def FindExistingBrowser(self):
    """Find a browser running on the device and bind a Browser object to it.

    The returned Browser object will only be bound to a running browser
    instance whose package name matches the one specified by the backend
    settings of this possible browser.

    A BrowserGoneException is raised if the browser cannot be found.
    """
    return self._GetBrowserInstance(existing=True)

  def _GetBrowserInstance(self, existing):
    # Init the LocalFirstBinaryManager if this is the first time we're creating
    # a browser. Note that we use the host's OS and architecture since the
    # retrieved dependencies are used on the host, not the device.
    if local_first_binary_manager.LocalFirstBinaryManager.NeedsInit():
      local_first_binary_manager.LocalFirstBinaryManager.Init(
          self._build_dir, self._local_apk, platform.system().lower(),
          platform.machine())

    browser_backend = android_browser_backend.AndroidBrowserBackend(
        self._platform_backend, self._finder_options,
        self.browser_directory, self.profile_directory,
        self._backend_settings, build_dir=self._build_dir,
        local_apk_path=self._local_apk)
    try:
      return browser.Browser(
          browser_backend, self._platform_backend, startup_args=(),
          find_existing=existing)
    except Exception:
      browser_backend.Close()
      raise

  def GetBrowserStartupArgs(self, browser_options):
    startup_args = chrome_startup_args.GetFromBrowserOptions(browser_options)
    # use the flag `--ignore-certificate-errors` if in compatibility mode
    supports_spki_list = (
        self._backend_settings.supports_spki_list and
        compat_mode_options.IGNORE_CERTIFICATE_ERROR
        not in browser_options.compatibility_mode)
    startup_args.extend(chrome_startup_args.GetReplayArgs(
        self._platform_backend.network_controller_backend,
        supports_spki_list=supports_spki_list))
    startup_args.append('--enable-remote-debugging')
    startup_args.append('--disable-fre')
    startup_args.append('--disable-external-intent-requests')

    # Need to specify the user profile directory for
    # --ignore-certificate-errors-spki-list to work.
    startup_args.append('--user-data-dir=' + self.profile_directory)

    # Needed so that non-browser-process crashes avoid automatic dump upload
    # and subsequent deletion. The extra "Crashpad" is necessary because
    # crashpad_stackwalker.py is hard-coded to look for a "Crashpad" directory
    # in the dump directory that it is provided.
    startup_args.append('--breakpad-dump-location=' + posixpath.join(
        self._platform_backend.GetDumpLocation(self.browser_package),
        'Crashpad'))

    return startup_args

  def SupportsOptions(self, browser_options):
    if len(browser_options.extensions_to_load) != 0:
      return False
    return True

  def IsAvailable(self):
    """Returns True if the browser is or can be installed on the platform."""
    has_local_apks = self._local_apk and (
        not self._backend_settings.requires_embedder or self._support_apk_list)
    return has_local_apks or self.platform.CanLaunchApplication(
        self.settings.package)

  @decorators.Cache
  def UpdateExecutableIfNeeded(self):
    # TODO(crbug.com/815133): This logic should belong to backend_settings.
    for apk in self._support_apk_list:
      logging.warning('Installing %s on device if needed.', apk)
      self.platform.InstallApplication(apk)

    apk_name = self._backend_settings.GetApkName(
        self._platform_backend.device)
    is_webview_apk = apk_name is not None and ('SystemWebView' in apk_name or
                                               'system_webview' in apk_name or
                                               'TrichromeWebView' in apk_name or
                                               'trichrome_webview' in apk_name)
    # The WebView fallback logic prevents sideloaded WebView APKs from being
    # installed and set as the WebView implementation correctly. Disable the
    # fallback logic before installing the WebView APK to make sure the fallback
    # logic doesn't interfere.
    if is_webview_apk:
      self._platform_backend.device.SetWebViewFallbackLogic(False)

    if self._local_apk:
      logging.warning('Installing %s on device if needed.', self._local_apk)
      self.platform.InstallApplication(
          self._local_apk, modules=self._modules_to_install)
      if self._compile_apk:
        package_name = apk_helper.GetPackageName(self._local_apk)
        logging.warning('Compiling %s.', package_name)
        self._platform_backend.device.RunShellCommand(
            ['cmd', 'package', 'compile', '-m', self._compile_apk, '-f',
             package_name],
            check_return=True,
            timeout=120)

    sdk_version = self._platform_backend.device.build_version_sdk
    # Bundles are in the ../bin directory, so it's safer to just check the
    # correct name is part of the path.
    is_monochrome = apk_name is not None and (apk_name == 'Monochrome.apk' or
                                              'monochrome_bundle' in apk_name)
    if ((is_webview_apk or
         (is_monochrome and sdk_version < version_codes.Q)) and
        sdk_version >= version_codes.NOUGAT):
      package_name = apk_helper.GetPackageName(self._local_apk)
      logging.warning('Setting %s as WebView implementation.', package_name)
      self._platform_backend.device.SetWebViewImplementation(package_name)

  def GetTypExpectationsTags(self):
    tags = super().GetTypExpectationsTags()
    if 'webview' in self.browser_type:
      tags.append('android-webview')
    else:
      tags.append('android-not-webview')
    return tags


def SelectDefaultBrowser(possible_browsers):
  """Return the newest possible browser."""
  if not possible_browsers:
    return None
  return max(possible_browsers, key=lambda b: b.last_modification_time)


def CanFindAvailableBrowsers():
  return android_device.CanDiscoverDevices()


def _CanPossiblyHandlePath(apk_path):
  if not apk_path:
    return False
  try:
    apk_helper.ToHelper(apk_path)
    return True
  except apk_helper.ApkHelperError:
    return False


def FindAllBrowserTypes():
  browser_types = [b.browser_type for b in ANDROID_BACKEND_SETTINGS]
  return browser_types + ['exact', 'reference']


def _FetchReferenceApk(android_platform, is_bundle=False):
  """Fetch the apk for reference browser type from gcloud.

  Local path to the apk will be returned upon success.
  Otherwise, None will be returned.
  """
  os_version = dependency_util.GetChromeApkOsVersion(
      android_platform.GetOSVersionName())
  if is_bundle:
    os_version += '_bundle'
  arch = android_platform.GetArchName()
  try:
    reference_build = binary_manager.FetchPath(
        'chrome_stable', 'android', arch, os_version)
    if reference_build and os.path.exists(reference_build):
      return reference_build
  except binary_manager.NoPathFoundError:
    logging.warning('Cannot find path for reference apk for device %s',
                    android_platform.GetDeviceId())
  except binary_manager.CloudStorageError:
    logging.warning('Failed to download reference apk for device %s',
                    android_platform.GetDeviceId())
  return None


def _GetReferenceAndroidBrowser(android_platform, finder_options):
  reference_build = _FetchReferenceApk(android_platform)
  if reference_build:
    return PossibleAndroidBrowser(
        'reference',
        finder_options,
        android_platform,
        android_browser_backend_settings.ANDROID_CHROME,
        reference_build)
  return None


def _FindAllPossibleBrowsers(finder_options, android_platform):
  """Testable version of FindAllAvailableBrowsers."""
  if not android_platform:
    return []
  possible_browsers = []

  for apk in finder_options.webview_embedder_apk:
    if not os.path.exists(apk):
      raise exceptions.PathMissingError(
          'Unable to find apk specified by --webview-embedder-apk=%s' % apk)

  # Add the exact APK if given.
  if _CanPossiblyHandlePath(finder_options.browser_executable):
    if not os.path.exists(finder_options.browser_executable):
      raise exceptions.PathMissingError(
          'Unable to find exact apk specified by --browser-executable=%s' %
          finder_options.browser_executable)

    package_name = apk_helper.GetPackageName(finder_options.browser_executable)
    try:
      backend_settings = next(
          b for b in ANDROID_BACKEND_SETTINGS if b.package == package_name)
    except StopIteration as e:
      raise exceptions.UnknownPackageError(
          '%s specified by --browser-executable has an unknown package: %s' %
          (finder_options.browser_executable, package_name)) from e

    possible_browsers.append(PossibleAndroidBrowser(
        'exact',
        finder_options,
        android_platform,
        backend_settings,
        finder_options.browser_executable))

  if finder_options.IsBrowserTypeRelevant('reference'):
    reference_browser = _GetReferenceAndroidBrowser(
        android_platform, finder_options)
    if reference_browser:
      possible_browsers.append(reference_browser)

  # Add any other known available browsers.
  for settings in ANDROID_BACKEND_SETTINGS:
    if finder_options.IsBrowserTypeRelevant(settings.browser_type):
      local_apk = None
      if finder_options.IsBrowserTypeReference():
        local_apk = _FetchReferenceApk(
            android_platform, finder_options.IsBrowserTypeBundle())

      if settings.IsWebView():
        p_browser = PossibleAndroidBrowser(
            settings.browser_type, finder_options, android_platform, settings,
            local_apk=local_apk, target_os='android_webview')
      else:
        p_browser = PossibleAndroidBrowser(
            settings.browser_type, finder_options, android_platform, settings,
            local_apk=local_apk)
      if p_browser.IsAvailable():
        possible_browsers.append(p_browser)
  return possible_browsers


def FindAllAvailableBrowsers(finder_options, device):
  """Finds all the possible browsers on one device.

  The device is either the only device on the host platform,
  or |finder_options| specifies a particular device.
  """
  if not isinstance(device, android_device.AndroidDevice):
    return []

  try:
    android_platform = telemetry_platform.GetPlatformForDevice(
        device, finder_options)
    return _FindAllPossibleBrowsers(finder_options, android_platform)
  except base_error.BaseError as e:
    logging.error('Unable to find browsers on %s: %s', device.device_id, str(e))
    ps_output = subprocess.check_output(['ps', '-ef'])
    logging.error('Ongoing processes:\n%s', ps_output)
  return []
