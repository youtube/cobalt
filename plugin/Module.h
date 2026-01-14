/**
* Copyright 2020 Comcast Cable Communications Management, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* SPDX-License-Identifier: Apache-2.0
*/
#pragma once

#ifndef MODULE_NAME
#define MODULE_NAME Plugin_Cobalt
#endif

#include <core/core.h>
#include <plugins/plugins.h>

#if !defined(THUNDER_VERSION) && defined(THUNDER_VERSION_MAJOR)
#define THUNDER_VERSION THUNDER_VERSION_MAJOR
#endif

#undef EXTERNAL
#define EXTERNAL
