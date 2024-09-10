// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: User Interface Navigation module
//
// API to allow applications to take advantage of the platform's native UI
// engine. This is mainly to drive the animation of visual elements and to
// signal which of those elements have focus. The implementation should not
// render any visual elements; instead, it will be used to guide the app in
// where these elements should be drawn.
//
// When the application creates the user interface, it will create SbUiNavItems
// for interactable elements. Additionally, the app must specify the position
// and size of these navigation items. As the app's user interface changes, it
// will create and destroy navigation items as appropriate.
//
// For each render frame, the app will query the local transform for each
// SbUiNavItem in case the native UI engine moves individual items in response
// to user interaction. If the navigation item is a container, then the content
// offset will also be queried to determine the placement of its content items.

#ifndef STARBOARD_UI_NAVIGATION_H_
#define STARBOARD_UI_NAVIGATION_H_

#error This file is deprecated with SB_API_VERSION 16.

#endif  // STARBOARD_UI_NAVIGATION_H_
