// Copyright 2022 The Cobalt Authors.All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

const data = "worker data";
this.postMessage(data);
this.onmessage = function (event) {
    let message = 'worker received wrong message';
    if (event.data === 'window data') {
        message = 'worker received correct message';
    }
    console.log(message);
    this.postMessage(message);
    const uppercase_data = event.data.toUpperCase();
    this.postMessage(uppercase_data);
};
this
