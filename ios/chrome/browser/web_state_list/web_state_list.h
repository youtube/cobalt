// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_H_

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "url/gurl.h"

class WebStateListDelegate;
class WebStateListObserver;
class WebStateListOrderController;
struct WebStateOpener;

namespace web {
class WebState;
}

enum class ActiveWebStateChangeReason;

// Manages a list of WebStates.
//
// This class supports mutating the list and to observe the mutations via the
// WebStateListObserver interface. However, the class is not re-entrant, thus
// it is an error to mutate the list from an observer (either directly by the
// observer, or indirectly via code invoked from the observer).
//
// The WebStateList takes ownership of the WebStates that it manages.
class WebStateList {
 public:
  // Constants used when inserting WebStates.
  enum InsertionFlags {
    // Used to indicate that nothing special should happen to the newly
    // inserted WebState.
    INSERT_NO_FLAGS = 0,

    // Used to indicate that the WebState should be activated on insertion.
    INSERT_ACTIVATE = 1 << 0,

    // If not set, the insertion index of the WebState is left up to the
    // order controller associated with the WebStateList so the insertion
    // index may differ from the specified index. Otherwise the supplied
    // index is used.
    INSERT_FORCE_INDEX = 1 << 1,

    // If set, the WebState opener is set to the active WebState, otherwise
    // it must be explicitly passed.
    INSERT_INHERIT_OPENER = 1 << 2,

    // Used to indicate that the WebState should be pinned on insertion.
    INSERT_PINNED = 1 << 3,
  };

  // Constants used when closing WebStates.
  enum ClosingFlags {
    // Used to indicate that nothing special should happen to the closed
    // WebState.
    CLOSE_NO_FLAGS = 0,

    // Used to indicate that the WebState was closed due to user action.
    CLOSE_USER_ACTION = 1 << 0,
  };

  explicit WebStateList(WebStateListDelegate* delegate);

  WebStateList(const WebStateList&) = delete;
  WebStateList& operator=(const WebStateList&) = delete;

  ~WebStateList();

  // Returns a weak pointer to the WebStateList.
  base::WeakPtr<WebStateList> AsWeakPtr();

  // Returns whether the model is empty or not.
  bool empty() const { return web_state_wrappers_.empty(); }

  // Returns the number of WebStates in the model.
  int count() const { return static_cast<int>(web_state_wrappers_.size()); }

  // Returns the index of the currently active WebState, or kInvalidIndex if
  // there are no active WebState.
  int active_index() const { return active_index_; }

  // Returns true if the specified index is contained by the model.
  bool ContainsIndex(int index) const;

  // Returns true if the list is currently mutating.
  bool IsMutating() const;

  // Returns true if a batch operation is in progress.
  bool IsBatchInProgress() const;

  // Returns the currently active WebState or null if there is none.
  web::WebState* GetActiveWebState() const;

  // Returns the WebState at the specified index. It is invalid to call this
  // with an index such that `ContainsIndex(index)` returns false.
  web::WebState* GetWebStateAt(int index) const;

  // Returns the index of the specified WebState or kInvalidIndex if the
  // WebState is not in the model.
  int GetIndexOfWebState(const web::WebState* web_state) const;

  // Returns the index of the first WebState in the model whose visible URL is
  // `url` or kInvalidIndex if no WebState with that URL exists.
  int GetIndexOfWebStateWithURL(const GURL& url) const;

  // Returns the index of the first WebState, ignoring the currently active
  // WebState, in the model whose visible URL is `url` or kInvalidIndex if no
  // non-active WebState with that URL exists.
  int GetIndexOfInactiveWebStateWithURL(const GURL& url) const;

  // Returns the index of the first non-pinned WebState in the WebStateList.
  // Returns 0 in case no pinned WebStates are present.
  // Returns `count()` in case only pinned WebStates are present.
  int GetIndexOfFirstNonPinnedWebState() const;

  // Returns information about the opener of the WebState at the specified
  // index. The structure `opener` will be null if there is no opener.
  WebStateOpener GetOpenerOfWebStateAt(int index) const;

  // Stores information about the opener of the WebState at the specified
  // index. The WebStateOpener `opener` must be non-null and the WebState
  // must be in WebStateList.
  void SetOpenerOfWebStateAt(int index, WebStateOpener opener);

  // Returns the index of the next WebState in the sequence of WebStates opened
  // from the specified WebState after `start_index`, or kInvalidIndex if there
  // are no such WebState. If `use_group` is true, the opener's navigation index
  // is used to detect navigation changes within the same session.
  // If `exclude_pinned` is true, pinned WebStates are removed from search.
  int GetIndexOfNextWebStateOpenedBy(const web::WebState* opener,
                                     int start_index,
                                     bool use_group) const;

  // Returns the index of the last WebState in the sequence of WebStates opened
  // from the specified WebState after `start_index`, or kInvalidIndex if there
  // are no such WebState. If `use_group` is true, the opener's navigation index
  // is used to detect navigation changes within the same session.
  // If `exclude_pinned` is true, pinned WebStates are removed from search.
  int GetIndexOfLastWebStateOpenedBy(const web::WebState* opener,
                                     int start_index,
                                     bool use_group) const;

  // Changes the pinned state of the WebState at `index`. Returns the index the
  // WebState is now at (it may have been moved to maintain contiguity of pinned
  // WebStates at the beginning of the list).
  int SetWebStatePinnedAt(int index, bool pinned);

  // Returns true if the WebState at |index| is pinned.
  bool IsWebStatePinnedAt(int index) const;

  // Inserts the specified WebState at the best position in the WebStateList
  // given the specified opener, recommended index, insertion flags, ... The
  // `insertion_flags` is a bitwise combination of InsertionFlags values.
  // Returns the effective insertion index.
  int InsertWebState(int index,
                     std::unique_ptr<web::WebState> web_state,
                     int insertion_flags,
                     WebStateOpener opener);

  // Moves the WebState at the specified index to another index.
  void MoveWebStateAt(int from_index, int to_index);

  // Replaces the WebState at the specified index with new WebState. Returns
  // the old WebState at that index to the caller (abandon ownership of the
  // returned WebState).
  std::unique_ptr<web::WebState> ReplaceWebStateAt(
      int index,
      std::unique_ptr<web::WebState> web_state);

  // Detaches the WebState at the specified index. Returns the detached WebState
  // to the caller (abandon ownership of the returned WebState).
  std::unique_ptr<web::WebState> DetachWebStateAt(int index);

  // Closes and destroys the WebState at the specified index. The `close_flags`
  // is a bitwise combination of ClosingFlags values.
  void CloseWebStateAt(int index, int close_flags);

  // Closes and destroys all non-pinned WebStates. The `close_flags` is a
  // bitwise combination of ClosingFlags values.
  void CloseAllNonPinnedWebStates(int close_flags);

  // Closes and destroys all WebStates. The `close_flags` is a bitwise
  // combination of ClosingFlags values.
  void CloseAllWebStates(int close_flags);

  // Makes the WebState at the specified index the active WebState.
  void ActivateWebStateAt(int index);

  // Adds an observer to the model.
  void AddObserver(WebStateListObserver* observer);

  // Removes an observer from the model.
  void RemoveObserver(WebStateListObserver* observer);

  // Performs mutating operations on the WebStateList as batched operation.
  // The observers will be notified by WillBeginBatchOperation() before the
  // `operation` callback is executed and by BatchOperationEnded() after it
  // has completed.
  void PerformBatchOperation(base::OnceCallback<void(WebStateList*)> operation);

  // Invalid index.
  static const int kInvalidIndex = -1;

 private:
  class WebStateWrapper;

  // Locks the WebStateList for mutation. This methods checks that the list is
  // not currently mutated (as the class is not re-entrant it would lead to
  // corruption of the internal state and ultimately to indefined behaviour).
  base::AutoReset<bool> LockForMutation();

  // Inserts the specified WebState at the best position in the WebStateList
  // given the specified opener, recommended index, insertion flags, ... The
  // `insertion_flags` is a bitwise combination of InsertionFlags values.
  // Returns the effective insertion index.
  //
  // Assumes that the WebStateList is locked.
  int InsertWebStateImpl(int index,
                         std::unique_ptr<web::WebState> web_state,
                         int insertion_flags,
                         WebStateOpener opener);

  // Moves the WebState at the specified index to another index.
  //
  // Assumes that the WebStateList is locked.
  void MoveWebStateAtImpl(int from_index, int to_index);

  // Replaces the WebState at the specified index with new WebState. Returns
  // the old WebState at that index to the caller (abandon ownership of the
  // returned WebState).
  //
  // Assumes that the WebStateList is locked.
  std::unique_ptr<web::WebState> ReplaceWebStateAtImpl(
      int index,
      std::unique_ptr<web::WebState> web_state);

  // Detaches the WebState at the specified index. Returns the detached WebState
  // to the caller (abandon ownership of the returned WebState).
  //
  // Assumes that the WebStateList is locked.
  std::unique_ptr<web::WebState> DetachWebStateAtImpl(int index);

  // Closes and destroys the WebState at the specified index. The `close_flags`
  // is a bitwise combination of ClosingFlags values.
  //
  // Assumes that the WebStateList is locked.
  void CloseWebStateAtImpl(int index, int close_flags);

  // Closes and destroys all WebStates after `start_index`. The `close_flags`
  // is a bitwise combination of ClosingFlags values.
  //
  // Assumes that the WebStateList is locked.
  void CloseAllWebStatesAfterIndexImpl(int start_index, int close_flags);

  // Makes the WebState at the specified index the active WebState.
  //
  // Assumes that the WebStateList is locked.
  void ActivateWebStateAtImpl(int index, ActiveWebStateChangeReason reason);

  // Sets the opener of any WebState that reference the WebState at the
  // specified index to null.
  void ClearOpenersReferencing(int index);

  // Notify the observers if the active WebState change. `reason` is the value
  // passed to the WebStateListObservers.
  void NotifyIfActiveWebStateChanged(web::WebState* old_web_state,
                                     ActiveWebStateChangeReason reason);

  // Returns the index of the `n`-th WebState (with n > 0) in the sequence of
  // WebStates opened from the specified WebState starting the search from
  // `start_index` (the returned index may be smaller than `start_index` if
  // the element have been rearranged), or kInvalidIndex if there are no such
  // WebState. If `use_group` is true, the opener's navigation index is used
  // to detect navigation changes within the same session.
  // If `exclude_pinned` is true, pinned WebStates are removed from search.
  int GetIndexOfNthWebStateOpenedBy(const web::WebState* opener,
                                    int start_index,
                                    bool use_group,
                                    int n) const;

  // Changes the pinned state of the WebState at `index`, moving the tab to the
  // end of the pinned/unpinned section in the process if necessary. Returns
  // the new index of the WebState.
  int SetWebStatePinnedImpl(int index, bool pinned);

  // Verifies that WebState's insertion `index` is within the proper index
  // range. `pinned` WebStates `index` should be within the pinned WebStates
  // range. Regular WebState `index` should be outside of the pinned WebStates
  // range. Returns an updated insertion `index` of the WebState.
  int ConstrainInsertionIndex(int index, bool pinned) const;

  // Verifies that WebState's move `index` is within the proper index range.
  // `pinned` WebStates `index` should be within the pinned WebStates range.
  // Regular WebState `index` should be outside of the pinned WebStates range.
  // Returns an updated move `index` of the WebState.
  int ConstrainMoveIndex(int index, bool pinned) const;

  // Returns the wrapper of the currently active WebState or null if there
  // is none.
  WebStateWrapper* GetActiveWebStateWrapper() const;

  // Returns the wrapper of the WebState at the specified index. It is invalid
  // to call this with an index such that `ContainsIndex(index)` returns false.
  WebStateWrapper* GetWebStateWrapperAt(int index) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // The WebStateList delegate.
  WebStateListDelegate* delegate_ = nullptr;

  // Wrappers to the WebStates hosted by the WebStateList.
  std::vector<std::unique_ptr<WebStateWrapper>> web_state_wrappers_;

  // An object that determines where new WebState should be inserted and where
  // selection should move when a WebState is detached.
  std::unique_ptr<WebStateListOrderController> order_controller_;

  // List of observers notified of changes to the model.
  base::ObserverList<WebStateListObserver, true> observers_;

  // Index of the currently active WebState, kInvalidIndex if no such WebState.
  int active_index_ = kInvalidIndex;

  // Lock to prevent observers from mutating or deleting the list while it is
  // mutating. The lock is managed by LockForMutation() method (and released
  // by the returned base::AutoReset<bool>).
  bool locked_ = false;

  // Lock to prevent nesting batched operations.
  bool batch_operation_in_progress_ = false;

  // Weak pointer factory.
  base::WeakPtrFactory<WebStateList> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_H_
