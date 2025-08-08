// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_CONSTANTS_H_

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"

// Name of the directory containing the legacy sessions.
extern const base::FilePath::CharType kLegacySessionsDirname[];

// Name of the directory containing the legacy web sessions.
extern const base::FilePath::CharType kLegacyWebSessionsDirname[];

// Name of the legacy session file.
extern const base::FilePath::CharType kLegacySessionFilename[];

// Name of the directory containing the sessions' storage.
extern const base::FilePath::CharType kSessionRestorationDirname[];

// Name of the session metadata file.
extern const base::FilePath::CharType kSessionMetadataFilename[];

// Name of the file storing the data for a single WebState.
extern const base::FilePath::CharType kWebStateStorageFilename[];

// Name of the file storing the session data for a single WebState.
extern const base::FilePath::CharType kWebStateSessionFilename[];

// Name of the file storing the metadata for a single WebState.
extern const base::FilePath::CharType kWebStateMetadataStorageFilename[];

// Keys used to store information metadata about a WebState in a WebStateList.
extern NSString* const kLegacyWebStateListPinnedStateKey;
extern NSString* const kLegacyWebStateListOpenerIndexKey;
extern NSString* const kLegacyWebStateListOpenerNavigationIndexKey;

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_CONSTANTS_H_
