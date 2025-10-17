// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_CONSUMER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_CONSUMER_H_

enum class AIPrototypingFeature : NSInteger;

// Consumer protocol for the mediator to interact with the view controller.
@protocol AIPrototypingConsumer

// Updates the result of a query that was previously executed on a model.
- (void)updateQueryResult:(NSString*)result
               forFeature:(AIPrototypingFeature)feature;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_CONSUMER_H_
