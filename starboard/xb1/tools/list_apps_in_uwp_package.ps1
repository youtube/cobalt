# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

$PackageName = $args[0]

Write-Verbose "Getting AppX package information for $PackageName"
$Apps = Get-AppxPackage -Name "*$PackageName*"
foreach ($Package in $Apps) {
  [xml]$AppXManifest = $Package | Get-AppxPackageManifest
  $Applications = ($AppXManifest.Package.Applications)

  foreach ($a in $Applications.Application) {
    foreach ($e in $a.Extensions.Extension) {
      if ($e.Category -eq 'windows.protocol') {
        Write-Output ("^:" + $Package.Name + ":" + $a.id + ":" + $a.Executable + ":" + $e.Protocol.Name)
      }
    }
  }
}
