# escape=`

# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
FROM build-base-windows

SHELL ["powershell", "-ExecutionPolicy", "Unrestricted", "-Command", `
       "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

WORKDIR "C:\code"

CMD Invoke-Command -ScriptBlock {`
      python cobalt/build/gyp_cobalt -vvv $env:PLATFORM -C $env:CONFIG;`
    } | Out-Default;`
    if ($?) {`
      Invoke-Command -ScriptBlock {`
        ninja $env:NINJA_FLAGS -C out/${env:PLATFORM}_$env:CONFIG $env:TARGET;`
      } | Out-Default`
    }
