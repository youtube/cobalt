// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_FEED_APP_AGENT_H_
#define IOS_CHROME_APP_FEED_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// The agent that manages the Feed service creation. This service allows the App
// and users to perform Feed related operations e.g. Creating a Feed, Following
// a Website, etc. This also manages feed background refresh.
// TODO(crbug.com/1344866): Coordinate background tasks when more are added.
@interface FeedAppAgent : SceneObservingAppAgent
@end

#endif  // IOS_CHROME_APP_FEED_APP_AGENT_H_
