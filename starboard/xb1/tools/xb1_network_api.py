#!/usr/bin/python
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
""" This script provides an API for the Xbox REST API. """

from __future__ import print_function

import base64
import os
import pprint
import sys
import time
import traceback

from distutils.util import strtobool  # pylint: disable=deprecated-module
import requests
from requests.compat import urljoin
# This helps the script run in an consistent way across multiple different
# library versions(requests) and python runtime versions.
try:
  from requests.packages import urllib3
except ImportError:
  import urllib3
try:
  from urllib3.exceptions import InsecureRequestWarning
  urllib3.disable_warnings(InsecureRequestWarning)
except ImportError:
  pass

try:
  input_fn = raw_input
except NameError:
  input_fn = input

_DEFAULT_URL_FORMAT = 'https://{}:{}}/'

_APPX_RELATIVE_PATH = 'appx'
_DEVELOPMENT_FILES = 'DevelopmentFiles'
_LOCAL_APP_DATA = 'LocalAppData'
_LOCAL_CACHE_FOLDERNAME = r'\\LocalCache'
_TEMP_FILE_FOLDERNAME = r'\\WdpTempWebFolder'
_DEFAULT_STAGING_APP_NAME = 'appx'

_APP_INSTALLATION_STATUS_ENDPOINT = '/api/app/packagemanager/state'
_DELETE_APP_ENDPOINT = '/api/app/packagemanager/package'
_DELETE_FILE_ENDPOINT = '/api/filesystem/apps/file'
_DEVICE_INFO_ENDPOINT = '/ext/xbox/info'
_EXT_USER_ENDPOINT = '/ext/user'
_GET_FILE_ENDPOINT = '/api/filesystem/apps/file'
_GET_FILES_ENDPOINT = '/api/filesystem/apps/files'
_KILL_PROCESS_ENDPOINT = 'api/taskmanager/process'
_LIST_ALL_APPS_ENDPOINT = '/api/app/packagemanager/packages'
_MACHINENAME_ENDPOINT = '/api/os/machinename'
_NETWORK_CREDS_ENDPOINT = '/ext/networkcredential'
_REGISTER_ENDPOINT = 'api/app/packagemanager/register'
_RESOURCE_MANAGER_PROCESSES_ENDPOINT = 'api/resourcemanager/processes'
_SANDBOX_ENDPOINT = '/ext/xboxlive/sandbox'
_SETTINGS_ENDPOINT = '/ext/settings'
_OS_UPDATE_ENDPOINT = '/ext/settings/osupdatepolicy'
_TASKMANAGER_ENDPOINT = '/api/taskmanager/app'
_UPLOAD_FILE_ENDPOINT = '/api/filesystem/apps/file'
_UPLOAD_FOLDER_ENDPOINT = '/api/app/packagemanager/upload'
_DEPLOY_INFO_ENDPOINT = '/ext/app/deployinfo'
_DEVELOPER_FOLDER_ENDPOINT = '/ext/smb/developerfolder'

# This header is required for all non-GET requests for windows
# dev portal.  It must be populated with the value of a specific cookie.
_CSRF_TOKEN_HEADER = 'X-CSRF-Token'

_INSTALL_FINISH_TIMEOUT_SECONDS = 90.0
_PROCESS_RUN_TIMEOUT_SECONDS = 300.0
_PROCESS_KILL_TIMEOUT_SECONDS = 20.0

# Wait time to use when polling the same API repeatedly.
_API_RETRY_SECONDS = 2.0


# Returns a dictionary of "directory_name" -> [file_paths,...]
def GetFilesRecursively(path):
  path = os.path.abspath(path)

  all_file_paths = []
  for root, _, filenames in os.walk(path):
    for filename in filenames:
      all_file_paths.append(os.path.join(root, filename))

  out = {}
  for file_path in all_file_paths:
    if os.path.isdir(file_path):
      continue
    dir_name = os.path.dirname(file_path)
    if dir_name not in out:
      out[dir_name] = []
    out[dir_name].append(file_path)

  return out


def _PrintAndRaiseIfError(response):
  """If http response is not OK (200), then print information and raise error

  If the response is not http "OK", then an exception is raised by
  |raise_for_status|, and this function will print additional information
  such as the request url, the response headers, and the response content.
  After that the exception is re-raised so that it can be handled at a higher
  level.  This function is mainly useful for debugging failed requests.

  Args:
    response: Server's response to our http request.

  Raises:
    requests.exceptions.HTTPError
  Returns:
    None
  """
  try:
    #The following line only raises on a response that is not status OK(200).
    response.raise_for_status()
  except Exception as e:
    print('URL:', response.url)
    print('status_code: ' + str(response.status_code))
    print('Headers:')
    pprint.pprint(dict(response.headers))
    print('Response:')
    try:
      pprint.pprint(response.json())
    except Exception:  #pylint: disable=broad-except
      print(response.text)
    raise e


class MissingDependencyError(Exception):
  pass


# Xb1NetworkApi allows an interface for communicating with a remote xbox
# device.
class Xb1NetworkApi:
  """ This header is required for all non-GET requests for windows
  dev portal.  It must be populated with the value of a specific cookie."""
  _CSRF_TOKEN_HEADER = 'X-CSRF-Token'

  def __init__(self, device_id, port):
    #? self.out_directory = os.path.split(self.GetTargetPath())[0]
    self.web_portal_url = f'https://{device_id}:{port}/'
    self._logfile_name = None
    self._csrf_token = None
    self._cookie_jar = None
    self.logging_function = None
    self.cached_package_name_map = {}
    self._AuthenticateOnce()

  def SetLoggingFunction(self, logging_function):
    self.logging_function = logging_function

  def Log(self, s):
    if self.logging_function:
      self.logging_function(s)
    else:
      print(s, end='')

  def LogLn(self, s):
    self.Log(s + '\n')

  def ComputerName(self):
    return self._DoJsonRequest('GET', _MACHINENAME_ENDPOINT)['ComputerName']

  def GetInstalledPackages(self):
    installed_apps_response = self._DoJsonRequest('GET',
                                                  _LIST_ALL_APPS_ENDPOINT)
    return installed_apps_response['InstalledPackages']

  # Returns all running processes in the app.
  def GetRunningProcesses(self):
    response_decoded = self._DoJsonRequest(
        'GET', _RESOURCE_MANAGER_PROCESSES_ENDPOINT)
    if 'Processes' in response_decoded:
      return response_decoded['Processes']
    return {}

  def GetSettings(self):
    val = self._DoJsonRequest('GET', _SETTINGS_ENDPOINT)
    return val.get('Settings', [])

  def IsPendingOSUpdate(self):
    """Detects whether there is a pending OS update."""
    try:
      self._DoRequest('GET', _OS_UPDATE_ENDPOINT)
      # Value returned is: <Response [200]>
      # So no pending update.
      return False
    except ValueError:
      # If there is a pending OS update then the following exception is
      # generated:
      # URL: https://172.31.165.44:11443/ext/settings/osupdatepolicy
      # status_code: 500
      # Headers:
      # {'Content-Length': '0',
      #  'Content-Type': 'text/plain',
      #  'Date': 'Thu, 09 May 2019 18:10:59 GMT',
      #  'Server': 'Microsoft-HTTPAPI/2.0',
      #  'Set-Cookie': 'CSRF-Token=YNsSUe/7hKTHsW/o/goM1opGEnYb6gi/'}
      # In this case there is a response code with error 500.
      return True
    except Exception as err:  #pylint: disable=broad-except
      traceback.print_exc()
      print(f'Unexpected {err}')
      return True

  def GetSetting(self, name):
    settings = self.GetSettings()
    for setting in settings:
      if setting.get('Name', '') == name:
        return setting
    return {}

  def IsInDevMode(self):
    setting = self.GetSetting('DefaultApp')
    return setting.get('Value', '') == 'Dev Home'

  def GetDeviceInfo(self):
    return self._DoJsonRequest('GET', _DEVICE_INFO_ENDPOINT)

  def GetLiveSandboxInfo(self):
    return self._DoRequest('GET', _SANDBOX_ENDPOINT).text

  def GetNetworkCredentials(self):
    val = self._DoJsonRequest('GET', _NETWORK_CREDS_ENDPOINT)
    if val.get('Credentials'):
      return (val['Username'], val['Password'], val['Path'])
    return ()

  def AddNetworkCredentials(self, username, password, path):
    p = {'Username': username, 'Password': password, 'NetworkPath': path}
    return self._DoJsonRequest('POST', _NETWORK_CREDS_ENDPOINT, params=p)

  def GetSandbox(self):
    return self._DoRequest('GET', '/ext/xboxlive/sandbox')

  def GetXboxLiveUserInfos(self):
    return self._DoJsonRequest('GET', _EXT_USER_ENDPOINT)

  def GetUserId(self, email_address):
    email_address = email_address.lower()
    users = self.GetXboxLiveUserInfos().get('Users', [])

    if not users:
      self.LogLn('Error - no users logged in')
      return

    for user in users:
      if user.get('EmailAddress', '').lower() == email_address:
        return user.get('UserId', None)

  def Restart(self):
    return self._DoRequest('POST', '/api/control/restart')

  def SetXboxLiveSignedInUserState(self, user_email, is_signed_in):
    user_id = self.GetUserId(user_email)
    if not user_id:
      self.LogLn('Could not find any users registered on this device')
      return

    creds = {
        'Users': [{
            'UserId': int(user_id),
            'SignedIn': bool(strtobool(str(is_signed_in))),
        },]
    }
    rtn = self._DoRequest('PUT', _EXT_USER_ENDPOINT, json=creds)
    return rtn

  def GetXboxLiveUserIsLoggedIn(self, user_email):
    user_email = user_email.lower()
    infos = self.GetXboxLiveUserInfos().get('Users', [])

    for info in infos:
      if info.get('EmailAddress', '').lower() == user_email:
        return info.get('SignedIn', False)
    return False

  def GetDeveloperFolderCredentials(self):
    d = self._DoJsonRequest('GET', _DEVELOPER_FOLDER_ENDPOINT)
    return (d['Username'], d['Password'], d['Path'])

  def GetDeploymentInfo(self, partial_package_name):
    full_package_name = self.ResolvePackageFullName(partial_package_name)

    if not full_package_name:
      self.LogLn('Could not get deployment info for ' + partial_package_name)
      full_package_name = partial_package_name
      return None

    body = {
        'DeployInfo': [{
            'PackageFullName': full_package_name
        },],
    }

    val = self._DoJsonRequest('POST', _DEPLOY_INFO_ENDPOINT, json=body)
    return val

  def IsBinaryRunning(self, target_name):
    response_decoded = self._DoJsonRequest(
        'GET', _RESOURCE_MANAGER_PROCESSES_ENDPOINT)
    running_processes = response_decoded['Processes']
    exe_name = target_name + '.exe'
    filtered_processes = \
        list(filter(lambda p: p['ImageName'] == exe_name, running_processes))
    if len(filtered_processes) == 0:
      return False
    process_under_test = filtered_processes[0]
    return process_under_test.get('IsRunning', False)

  def KillProcess(self, pid):
    self._DoJsonRequest('DELETE', _KILL_PROCESS_ENDPOINT, params={'pid': pid})

  def WaitForBinaryToFinishRunning(self, target_name, timeout_seconds):
    """Waits up to |timeout_seconds| for |self.target_name| to finish running.

    Args:
      timeout_seconds: Timeout in seconds.
    """
    assert isinstance(timeout_seconds, float)
    current_time = time.time()
    max_time = current_time + timeout_seconds
    while current_time <= max_time:
      if not self.IsBinaryRunning(target_name):
        return
      time.sleep(_API_RETRY_SECONDS)
    if self.IsBinaryRunning(target_name):
      raise RuntimeError('Waiting for process to end timed out.')

  def FetchFileContent(self, filename_to_grab):
    """Fetch a file from device and returns the content

    This function fetches a file named |filename_to_grab|,
    within the |_LOCAL_APP_DATA| folder.

    Args:
      filename_to_grab: name of the file to fetch.
    """
    self.LogLn('Fetching log..')
    package_full_name = self._GetPackageFullName()
    if not package_full_name:
      return

    fetch_log_request = requests.get(  # pylint: disable=missing-timeout
        urljoin(self.web_portal_url, self._GET_FILE_ENDPOINT),
        verify=False,
        params={
            'knownfolderid': self._LOCAL_APP_DATA,
            'packagefullname': package_full_name,
            'filename': filename_to_grab,
            'path': self._LOCAL_CACHE_FOLDERNAME
        })
    _PrintAndRaiseIfError(fetch_log_request)
    return fetch_log_request.text

  # Fetch a file given the package and file path.
  def FetchPackageFile(self,
                       package_name,
                       path_to_file,
                       raise_on_failure=True,
                       is_text=True):
    """Fetch a file from device and returns the content

    Note that files associated with a running process will
    fail to be fetched.

    Args:
      path_to_file: name of the file to fetch.
      raise_on_failure: when True, failure to fetch the file will result in an
        raised exception.
    """
    filename = os.path.basename(path_to_file)
    path = os.path.dirname(path_to_file)
    package_full_name = self.ResolvePackageFullName(package_name)

    if path:
      path = '\\\\' + path
    filename = '' + filename

    url = urljoin(self.web_portal_url, _GET_FILE_ENDPOINT)

    f = requests.get(  # pylint: disable=missing-timeout
        url,
        verify=False,
        params={
            'knownfolderid': _LOCAL_APP_DATA,
            'packagefullname': package_full_name,
            'filename': filename,
            'path': path
        })

    if raise_on_failure:
      _PrintAndRaiseIfError(f)
      if is_text:
        return f.text
      else:
        return f.content
    else:
      try:
        if is_text:
          return f.text
        else:
          return f.content
      except Exception:  #pylint: disable=broad-except
        return None
    return None

  def ExecuteBinary(self, partial_package_name, app_alias_name):
    default_relative_name = self._GetDefaultRelativeId(partial_package_name)
    if not default_relative_name or not '!' in default_relative_name:
      raise IOError('Could not resolve package name "' + partial_package_name +
                    '"')

    relative_package_name = default_relative_name.split('!')[0]
    package_relative_id = relative_package_name + '!' + app_alias_name
    appid_64 = base64.b64encode(package_relative_id.encode('UTF-8'))
    package_64 = base64.b64encode(default_relative_name.encode('UTF-8'))

    retry_count = 4
    # Time to wait between tries.
    retry_wait_s = 8
    try:
      while retry_count > 0:
        self.LogLn('Executing: ' + package_relative_id)
        response = self._DoJsonRequest(
            'POST',
            _TASKMANAGER_ENDPOINT,
            params={
                'appid': appid_64,
                'package': package_64
            },
            raise_on_failure=False)
        if not response or response == requests.codes.OK:
          self.LogLn('Execution successful')
          break
        self.LogLn('Execution not successful: ' + str(response))
        self.LogLn('Retrying with ' + str(retry_count) + ' attempts remaining.')
        time.sleep(retry_wait_s)
        retry_count -= 1
        # Double the wait time until the next attempt.
        retry_wait_s *= 2
    except Exception as err:
      err_msg = '\n  Failed to run:\n  ' + package_relative_id + \
                '\n  because of:\n' + str(err)
      raise IOError(err_msg) from err

  # Given a package name, return all files + directories.
  # Throws IOError if the app is locked.
  def ListFilesAndDirs(self, package_name, knownfolderid, sub_path):
    args = {}
    if sub_path:
      args['path'] = '\\\\' + sub_path
    if package_name:
      args['packagefullname'] = self.ResolvePackageFullName(package_name)
      # Per api, knownfolder id must 'LocalAppData' whenever package
      # name is used.
      assert knownfolderid == 'LocalAppData'
    # 'knownfolderid' is required
    args['knownfolderid'] = '' + knownfolderid

    response = self._DoRequest(
        'GET',
        '/api/filesystem/apps/files',
        params=args,
        raise_on_failure=False)

    if response.status_code == 403:
      raise IOError('Could not list files for ' + package_name +
                    ': permission denied.\n' + str(response))

    response_json = response.json()

    sub_files = []
    sub_dirs = []
    if 'Items' in response_json:
      for item in response_json['Items']:
        name = item['Name']
        sub_path = item['SubPath']
        if sub_path.endswith(name):
          sub_dirs.append(name)
        else:
          sub_files.append(name)
    return sub_files, sub_dirs

  def ListPackageFilesAndDirs(self, package_name, sub_path=None):
    return self.ListFilesAndDirs(package_name, 'LocalAppData', sub_path)

  # Returns files, dirs
  def ListPackageFilesAndDirsRecursive(self,
                                       package_name,
                                       list_empty_dirs=True):
    package_name = self.ResolvePackageFullName(package_name)

    def RecursiveStep(curr_dir_stack, found_files, found_dirs):
      curr_dir = '/'.join(curr_dir_stack)
      files, dirs = self.ListPackageFilesAndDirs(
          package_name, sub_path=curr_dir)

      add_curr_dir = curr_dir and (files or list_empty_dirs)
      if add_curr_dir:
        found_dirs.append(curr_dir)

      for f in files:
        tmp_path = curr_dir_stack[:]
        tmp_path.append(f)
        tmp_path = '/'.join(tmp_path)
        found_files.append(tmp_path)
      for d in dirs:
        next_dir_stack = curr_dir_stack[:]  # Copy by value.
        next_dir_stack.append(d)
        RecursiveStep(next_dir_stack, found_files, found_dirs)

    curr_dir_stack = []
    found_files = []
    found_dirs = []

    RecursiveStep(curr_dir_stack, found_files, found_dirs)
    return found_files, found_dirs

  def DeleteFile(self, known_folder_id, path, filename_to_delete):
    """Deletes a file named |file|.

    This function deletes a file.

    Args:
      known_folder_id: name of the top level known folder
      path: path to the directory containing filename_to_delete
      filename_to_delete: name of the file to delete
    """
    self.LogLn(f'Deleting [{filename_to_delete}]')
    file_listing = self._DoJsonRequest(
        'GET',
        _GET_FILES_ENDPOINT,
        params={
            'knownfolderid': known_folder_id,
            'path': path
        })
    for item in file_listing['Items']:
      if item['Name'] == filename_to_delete:
        break
    else:
      self.LogLn(filename_to_delete + ' was not found.  Skipping delete step.')
      return

    self.LogLn('File found. Deleting.')
    self._DoJsonRequest(
        'DELETE',
        _DELETE_FILE_ENDPOINT,
        params={
            'knownfolderid': known_folder_id,
            'filename': filename_to_delete,
            'path': path
        })

  def ClearTempFiles(self):
    file_listing = self._DoJsonRequest(
        'GET',
        _GET_FILES_ENDPOINT,
        params={
            'knownfolderid': _DEVELOPMENT_FILES,
            'path': _TEMP_FILE_FOLDERNAME
        })
    for file in file_listing['Items']:
      self.DeleteFile(_DEVELOPMENT_FILES, _TEMP_FILE_FOLDERNAME, file['Name'])

  def FindPackage(self, package_name):
    all_packages = self.GetInstalledPackages()

    for package in all_packages:
      if package_name in package['PackageRelativeId']:
        return package

    for package in all_packages:
      if package_name in package['PackageFullName']:
        return package

    for package in all_packages:
      if package_name in package['PackageFamilyName']:
        return package
    return None

  def ResolvePackageFullName(self, package_name):
    if package_name not in self.cached_package_name_map:
      package = self.FindPackage(package_name)
      full_package_name = None
      if package:
        full_package_name = package['PackageFullName']
      self.cached_package_name_map[package_name] = full_package_name
    return self.cached_package_name_map[package_name]

  def _GetDefaultRelativeId(self, partial_package_name):
    package = self.FindPackage(partial_package_name)
    if package is None:
      return None
    return package.get('PackageRelativeId', None)

  def _DoRequest(self, method, endpoint, **kwargs):
    url_to_request = urljoin(self.web_portal_url, endpoint)
    headers = {}
    raise_on_failure = True

    if 'headers' in kwargs:
      headers = kwargs['headers']
      del kwargs['headers']

    if 'raise_on_failure' in kwargs:
      raise_on_failure = kwargs['raise_on_failure']
      del kwargs['raise_on_failure']

    cookie_jar = None
    if method != 'GET':
      cookie_jar = self._cookie_jar
      if not self._csrf_token:
        raise RuntimeError('CSRF token is required for all non-GET requests.')
      headers.update({_CSRF_TOKEN_HEADER: self._csrf_token})

    request_info = requests.request(
        method,
        url_to_request,
        verify=False,
        timeout=(60, 60),
        cookies=cookie_jar,
        headers=headers,
        **kwargs)
    if raise_on_failure:
      _PrintAndRaiseIfError(request_info)

    if request_info.cookies:
      self._cookie_jar = request_info.cookies
      self._csrf_token = self._cookie_jar['CSRF-Token']
    return request_info

  def _DoJsonRequest(self, method, endpoint, **kwargs):
    response = self._DoRequest(method, endpoint, **kwargs)
    if response is None:
      return {}
    try:
      return response.json()
    except ValueError:
      return {}

  def _AuthenticateOnce(self):
    if self._csrf_token:
      return
    # Cookies / Authentication is a side effect of calling _DoRequest()
    self._DoRequest('GET', _MACHINENAME_ENDPOINT)
    if not self._csrf_token:
      raise RuntimeError('Authentication failed.')


############################################
# A little tester to test + inspect packages
def InteractiveCommandLineMode():

  # Local functions used in this test.
  def PrintCurrentlyRunningProcesses(network_api):
    running_processes = network_api.GetRunningProcesses()
    print('Found ' + str(len(running_processes)) + ' processes:')

    columns = ['ImageName', 'UserName', 'PageFileUsage', 'ProcessId']
    size_dict = {}

    for p in running_processes:
      for key in columns:
        val = p[key]
        prev_val_size = size_dict.get(key, 0)
        val_size = len(str(val))
        if prev_val_size <= val_size:
          size_dict[key] = val_size

    header = ''
    for key in columns:
      n_size = max(len(key), size_dict.get(key, 0)) + 2
      size_dict[key] = n_size
      header += key.upper().rjust(n_size)
    print(header)
    for _ in range(0, len(header)):
      print('-', end='')
    print()
    for p in running_processes:
      msg = ''
      for key in columns:
        val = p[key]
        msg += str(val).rjust(size_dict[key])
      print(msg)

  def PrintInstalledPackage(network_api):
    packages = network_api.GetInstalledPackages()
    print('Apps currently installed:')
    for p in packages:
      print('  ' + p['PackageFamilyName'])

  def FindThenPrintPackage(network_api):
    package_name = input_fn('Package name to inspect: ')
    package = network_api.FindPackage(package_name)
    if package is None:
      print('Could not find package ' + package_name)
    else:
      pprint.pprint(package)

  def ListFilesAndDirsInPackage(network_api):
    package_name = input_fn('Package name: ')
    package = network_api.FindPackage(package_name)
    if package is None:
      print('Could not find package ' + package_name)
    else:
      try:
        files, dirs = network_api.ListPackageFilesAndDirs(package_name)
        print(f'Files in {package_name}:\n')
        pprint.pprint({'files': files, 'dirs': dirs})
      except IOError as ioe:
        print(ioe)

  def PromptAndRunExecutable(network_api):
    package_name = input_fn('Package name: ')
    app_name = input_fn('App name: ')
    try:
      print('Attempting to run: ' + app_name + ' in ' + package_name)
      network_api.ExecuteBinary(package_name, app_name)
    except Exception as e:  #pylint: disable=broad-except
      print('Could not run binary because: ' + str(e))

  def PromptAndSignInUserToXboxLive(network_api):
    network_api.GetXboxLiveUserInfos()
    email_addr = input_fn('User EmailAddress: ')
    signed_in_state = input_fn('SignInState: ')
    network_api.SetXboxLiveSignedInUserState(email_addr, signed_in_state)

  def PromptPackageAndListFilesAndDirsRecursively(network_api):
    package_name = input_fn('Package name: ')
    files, dirs = network_api.ListPackageFilesAndDirsRecursive(package_name)
    if files:
      print('Files:')
      for f in files:
        print('  ' + f)
    if dirs:
      print('Directories:')
      for d in dirs:
        print('  ' + d)

  # Begin Test.
  ip = input_fn('IP Address of XBOX: ')
  if len(ip.split('.')) != 4:
    print('Expected ip to have a 4 numbers')
    return

  port = input_fn('port(default is 11443): ')
  if len(port) == 0:
    port = '11443'

  network_api = Xb1NetworkApi(ip, port)

  while True:
    print('What would you like to do?')
    print('  [0] See all currently running processes.')
    print('  [1] Run executable.')
    print('  [2] Find all installed packages.')
    print('  [3] Inspect a package by name.')
    print('  [4] List root directories and files in a package.')
    print('  [5] Recursive list all files in package.')
    print('  [6] Print System Settings.')
    print('  [7] Print Device Info.')
    print('  [8] Print Xbox Live Sandbox Mode.')
    print('  [9] Print Network Credentials.')
    print('  [10] XboxLive: Print signed in users.')
    print('  [11] XboxLive: Sign in user.')
    print('  [12] Print Network Credentials.')
    print('  [13] Update Network Credentials.')
    print('  [14] Print Deployment Info.')
    print('  [15] Print whether OS Update is pending.')
    print('  [q] Quit.')
    val = input_fn('> ')

    if val in ('', 'q'):
      print('quitting.')
      sys.exit(0)
    elif '0' == val:
      PrintCurrentlyRunningProcesses(network_api)
    elif '1' == val:
      PromptAndRunExecutable(network_api)
    elif '2' == val:
      PrintInstalledPackage(network_api)
    elif '3' == val:
      FindThenPrintPackage(network_api)
    elif '4' == val:
      ListFilesAndDirsInPackage(network_api)
    elif '5' == val:
      PromptPackageAndListFilesAndDirsRecursively(network_api)
    elif '6' == val:
      pprint.pprint(network_api.GetSettings())
    elif '7' == val:
      pprint.pprint(network_api.GetDeviceInfo())
    elif '8' == val:
      pprint.pprint(network_api.GetLiveSandboxInfo())
    elif '9' == val:
      pprint.pprint(network_api.GetNetworkCredentials())
    elif '10' == val:
      pprint.pprint(network_api.GetXboxLiveUserInfos())
    elif '11' == val:
      PromptAndSignInUserToXboxLive(network_api)
    elif '12' == val:
      pprint.pprint(network_api.GetNetworkCredentials())
    elif '13' == val:
      user_name = input_fn('username: ')
      password = input_fn('password: ')
      path = input_fn(
          'path (blank = "D:\\DevelopmentFiles\\LooseApps\\appx"?): ')
      if not path:
        path = 'D:\\DevelopmentFiles\\LooseApps\\appx'
      pprint.pprint(
          network_api.AddNetworkCredentials(user_name, password, path))
    elif '14' == val:
      package_name = input_fn('Package Name: ')
      pprint.pprint(network_api.GetDeploymentInfo(package_name))
    elif '15' == val:
      val = network_api.IsPendingOSUpdate()
      if val:
        print('----> OS has a pending update.\n')
      else:
        print('----> OS does NOT have a pending update.\n')
    else:
      print('Unexpected answer: ' + val)
    input_fn('press the return key to continue....')


if __name__ == '__main__':
  InteractiveCommandLineMode()
