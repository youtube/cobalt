// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

import AppIntents
import Foundation

// Opens the given Task in the app when run.
struct YtSearchVideoIntent: ShowInAppSearchResultsIntent {
  // NOTE:
  // The app is eligible for press-to-dictate, specially when the app is in the foreground.
  // The app will also appear in the Search app’s row of apps. This functionality was added
  // in tvOS 17.2.
  static var title: LocalizedStringResource = "Search Videos"

  static var searchScopes: [StringSearchScope] = [.freeformVideo, .general, .movies, .tv]

  @Parameter(title: "Search criteria")
  var criteria: StringSearchCriteria

  // Called by the OS when the user performs the given intent.
  @MainActor
  func perform() async throws -> some IntentResult {
    DeepLinkSupportTvos.handleSiriIntents(criteria.term, isSearch: true)
    return .result()
  }
}

// Opens the given Task in the app when run.
struct YtPlayVideoIntent: PlayVideoIntent {
  // NOTE:
  // Siri play requests. Using Siri to explicitly play video content on an app intent enabled app.
  // Support for this is coming in a future tvOS release.
  static var title: LocalizedStringResource = "Play Video"

  static var supportedCategories: [VideoCategory] = [.freeform, .movies, .tv]

  @Parameter(title: "Term")
  var term: String

  @MainActor
  func perform() async throws -> some IntentResult {
    DeepLinkSupportTvos.handleSiriIntents(term, isSearch: false)
    return .result()
  }
}
