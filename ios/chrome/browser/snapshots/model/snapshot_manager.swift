// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// A class that takes care of creating, storing and returning snapshots of a tab's web page. This
// lives on the UI thread.
@objcMembers public class SnapshotManager: NSObject {
  // Strong reference to the snapshot generator which is used to generate snapshots.
  private let snapshotGenerator: SnapshotGenerator

  // Weak reference to the snapshot storage which is used to store and retrieve snapshots.
  // SnapshotStorage is owned by SnapshotBrowserAgent.
  weak var snapshotStorage: SnapshotStorage?

  // The unique ID for WebState's snapshot.
  let snapshotID: SnapshotIDWrapper

  // The timestamp associated to the latest snapshot stored.
  private var latestCommitedTimestamp: Date = .distantPast

  // Designated initializer.
  init(generator: SnapshotGenerator, snapshotID: SnapshotIDWrapper) {
    assert(snapshotID.valid(), "snapshot ID should be valid")
    self.snapshotGenerator = generator
    self.snapshotID = snapshotID
    super.init()
  }

  // Gets a color snapshot for the current page, calling `completion` once it has been retrieved.
  // Invokes `completion` with nil if a snapshot does not exist.
  func retrieveSnapshot(completion: @escaping (UIImage?) -> Void) {
    if let storage = snapshotStorage {
      storage.retrieveImage(snapshotID: snapshotID, completion: completion)
    } else {
      completion(nil)
    }
  }

  // Gets a grey snapshot for the current page, calling `completion` once it has been retrieved or
  // regenerated. If the snapshot cannot be generated, the `completion` will be called with nil.
  func retrieveGreySnapshot(completion: @escaping (UIImage?) -> Void) {
    weak var weakSelf = self
    let wrappedCompletion: (UIImage?) -> Void = { image in
      weakSelf?.greySnapshotRetrieved(image: image, completion: completion)
    }

    if let storage = snapshotStorage {
      storage.retrieveGreyImage(snapshotID: snapshotID, completion: wrappedCompletion)
    } else {
      wrappedCompletion(nil)
    }
  }

  // Generates a new snapshot, updates the snapshot storage, and runs a callback with the new
  // snapshot image.
  func updateSnapshot(completion: ((UIImage?) -> Void)?) {
    weak var weakSelf = self

    // Since the snapshotting strategy may change, the order of snapshot updates
    // cannot be guaranteed. To prevent older snapshots from overwriting newer
    // ones, the timestamp of each snapshot request is recorded.
    let timestamp: Date = .now
    let wrappedCompletion: (UIImage?) -> Void = { image in
      // Update the snapshot storage with the latest snapshot. The old image is deleted if `image`
      // is nil.
      weakSelf?.updateSnapshotStorage(image: image, timestamp: timestamp)

      // Callback is called if it exists.
      completion?(image)
    }

    snapshotGenerator.generateSnapshot(completion: wrappedCompletion)
  }

  // Generates and returns a new snapshot image with UIKit-based snapshot API. This does not update
  // the snapshot storage.
  func generateUIViewSnapshot() -> UIImage? {
    return snapshotGenerator.generateUIViewSnapshot()
  }

  // Requests deletion of the current page snapshot from disk and memory.
  func removeSnapshot() {
    snapshotStorage?.removeImage(snapshotID: snapshotID)
  }

  // Sets the delegate to SnapshotGenerator. Generating snapshots before setting a delegate will
  // fail. The delegate is not owned by the tab helper.
  func setDelegate(_ delegate: SnapshotGeneratorDelegate) {
    snapshotGenerator.delegate = delegate
  }

  // Updates the snapshot storage with `image`.
  func updateSnapshotStorage(image: UIImage?) {
    // This method is bridging into Objective-C and cannot have a default value
    // for timestamp. For this reason, fallback to clasic method overloading
    // approach.
    updateSnapshotStorage(image: image, timestamp: .now)
  }

  /// Updates the snapshot storage with the provided image.
  ///
  /// - Parameters:
  ///   - image: The image to store as a snapshot. If `nil`, any existing
  ///   snapshot is removed.
  ///   - timestamp: The timestamp of when the snapshot was taken. If the
  ///    timestamp is earlier than the last commited one this method is NO-OP.
  private func updateSnapshotStorage(image: UIImage?, timestamp: Date) {
    guard let image = image else {
      latestCommitedTimestamp = .distantPast
      return
    }

    guard timestamp > latestCommitedTimestamp else { return }

    latestCommitedTimestamp = timestamp
    snapshotStorage?.setImage(image, snapshotID: snapshotID)
  }

  // Helper method used to retrieve a grey snapshot.
  private func greySnapshotRetrieved(
    image: UIImage?, completion: @escaping (UIImage?) -> Void
  ) {
    // Found an image in SnapshotStorage.
    if image != nil {
      completion(image)
      return
    }

    // Generate an image because it doesn't exist in SnapshotStorage.
    let generatedImage = snapshotGenerator.generateUIViewSnapshotWithOverlays()
    updateSnapshotStorage(image: generatedImage)
    if generatedImage != nil {
      let greyImage = UiKitUtils.greyImage(generatedImage)
      completion(greyImage)
    } else {
      completion(nil)
    }
  }
}
