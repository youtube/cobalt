// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_IOS_H_

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/ios/block_types.h"
#include "base/task/sequenced_task_runner.h"

@class SessionIOS;
@class SessionIOSFactory;

// A singleton service for saving the sessions (list of tabs). Can either save
// on a delay or immediately. Saving is always performed on a separate thread.
@interface SessionServiceIOS : NSObject

// Lazily creates a singleton instance with a default task runner.
+ (SessionServiceIOS*)sharedService;

// Initializes a SessionServiceIOS with a given task runner. Prefer to use the
// `sharedService` method.
- (instancetype)initWithTaskRunner:
    (const scoped_refptr<base::SequencedTaskRunner>&)taskRunner
    NS_DESIGNATED_INITIALIZER;

// Block and call `completion` when task runner is complete. Should only be used
// by fast terminate experiment.
- (void)shutdownWithCompletion:(ProceduralBlock)completion;

// Saves the session (list of tabs) returned by `factory`. The save location
// is derived from the scene identifier `sessionID` and the ChromeBrowserState
// `directory`. If `immediately` is NO, the save is done after a fixed delay,
// or ignored if another delayed save for the same location is still pending.
// If `immediately` is YES, then the save is done immediately and any pending
// save is cancelled. Either way, the save is done on a separate thread to
// avoid blocking the UI thread.
- (void)saveSession:(__weak SessionIOSFactory*)factory
          sessionID:(NSString*)sessionID
          directory:(const base::FilePath&)directory
        immediately:(BOOL)immediately;

// Loads a session (list of tabs) from the save location derived from the scene
// identifier `sessionID` and the ChromeBrowserState `directory`.
- (SessionIOS*)loadSessionWithSessionID:(NSString*)sessionID
                              directory:(const base::FilePath&)directory;

// Loads the session from `sessionPath` on the main thread. Returns nil in case
// of errors.
- (SessionIOS*)loadSessionFromPath:(NSString*)sessionPath;

// Schedules deletion of the all session files from a specific `directory`.
- (void)deleteAllSessionFilesInDirectory:(const base::FilePath&)directory
                              completion:(base::OnceClosure)callback;

// Schedule deletion of session directories with `sessionIDs` which resides in
// a specific browser state `directory`.
- (void)deleteSessions:(NSArray<NSString*>*)sessionIDs
             directory:(const base::FilePath&)directory
            completion:(base::OnceClosure)callback;

// Returns the path of the session with `sessionID` within a `directory`.
+ (NSString*)sessionPathForSessionID:(NSString*)sessionID
                           directory:(const base::FilePath&)directory;

// Returns the path of the tab file with id `tabID` for session with `sessionID`
// within a `directory`.
+ (NSString*)filePathForTabID:(NSString*)tabID
                    sessionID:(NSString*)sessionID
                    directory:(const base::FilePath&)directory;

// Returns the path of the tab file with id `tabID` for session at
// `sessionPath`.
+ (NSString*)filePathForTabID:(NSString*)tabID
                  sessionPath:(NSString*)sessionPath;

@end

@interface SessionServiceIOS (SubClassing)

// For some of the unit tests, we need to make sure the session is saved
// immediately so we can read it back in to verify various attributes. This
// is not a situation we normally expect to be in because we never
// want the session being saved on the main thread in the production app.
- (void)performSaveSessionData:(NSData*)sessionData
                   sessionPath:(NSString*)sessionPath;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_SERVICE_IOS_H_
