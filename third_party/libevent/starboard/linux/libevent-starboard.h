/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBEVENT_STARBOARD_H_
#define _LIBEVENT_STARBOARD_H_

#include "starboard/types.h"

#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define HAVE_EPOLL 1
#define HAVE_EPOLL_CTL 1

#endif  // _LIBEVENT_STARBOARD_H_
