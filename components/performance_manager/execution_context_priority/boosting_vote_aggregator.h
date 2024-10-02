// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_BOOSTING_VOTE_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_BOOSTING_VOTE_AGGREGATOR_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {
namespace execution_context_priority {

class BoostingVoteAggregator;

// A BoostingVote is a special kind of relative vote that allows a voter to
// express that "execution context X should have the same or greater priority
// than execution context Y". It allows implementing priority boost semantics to
// avoid priority inversions for access to shared resources. BoostingVotes must
// be registered with a BoostingVoteAggregator. Similar to a VoteReceipt, they
// are a move-only type and their vote will be removed with their destruction.
//
// A BoostingVote is considered "active" if it is associated with an aggregator
// (the result of calling "aggregator()" is non-null).
//
// See comments in the implementation for details on how the algorithm works.
class BoostingVote {
 public:
  // Registers a relative vote with the provided |aggregator|, that ensures that
  // the priority of |output_execution_context| will be at least as high as that
  // of |input_execution_context|.
  BoostingVote(BoostingVoteAggregator* aggregator,
               const ExecutionContext* input_execution_context,
               const ExecutionContext* output_execution_context,
               const char* reason);
  BoostingVote(const BoostingVote& rhs) = delete;
  BoostingVote(BoostingVote&& rhs);
  BoostingVote& operator=(const BoostingVote& rhs) = delete;
  BoostingVote& operator=(BoostingVote&& rhs);
  ~BoostingVote();

  BoostingVoteAggregator* aggregator() const { return aggregator_; }
  const ExecutionContext* input_execution_context() const {
    return input_execution_context_;
  }
  const ExecutionContext* output_execution_context() const {
    return output_execution_context_;
  }
  const char* reason() const { return reason_; }

  // Detaches this BoostingVote from its aggregator. After calling this
  // |aggregator_| will be nullptr and the vote will no longer be active.
  void Reset();

 private:
  raw_ptr<BoostingVoteAggregator> aggregator_ = nullptr;
  raw_ptr<const ExecutionContext> input_execution_context_ = nullptr;
  raw_ptr<const ExecutionContext> output_execution_context_ = nullptr;
  const char* reason_ = nullptr;
};

// The BoostingVoteAggregator allows for incoming votes to be modified via a
// collection of registered "relative boosting votes" that express relationships
// such as "execution context X should have the same or greater priority than
// execution context Y". It is intended to serve as the root of a tree of voters
// and aggregators, allowing priority boost semantics to be implemented. This
// class must outlive all boosting votes registered with it.
class BoostingVoteAggregator : public VoteObserver {
 public:
  BoostingVoteAggregator();
  BoostingVoteAggregator(const BoostingVoteAggregator&) = delete;
  BoostingVoteAggregator& operator=(const BoostingVoteAggregator&) = delete;
  ~BoostingVoteAggregator() override;

  // Both of these must be called in order for the aggregator to be setup
  // ("IsSetup" will return true). Both of these should be called exactly once.
  VotingChannel GetVotingChannel();
  void SetUpstreamVotingChannel(VotingChannel channel);

  bool IsSetup() const;

 protected:
  friend class BoostingVote;

  // We currently require that base::TaskPriority be zero-based, and
  // consecutive. These static asserts ensure that we revisit this code if the
  // base::TaskPriority enum ever changes.
  static_assert(static_cast<int>(base::TaskPriority::LOWEST) == 0,
                "expect 0-based priorities");
  static_assert(static_cast<int>(base::TaskPriority::HIGHEST) == 2,
                "expect 3 priority levels");

  using NodePriorityMap = std::map<const ExecutionContext*, base::TaskPriority>;

  // Small helper class used to endow both edges and nodes with "active" bits
  // for each priority layer.
  class ActiveLayers {
   public:
    // Returns true if any layer is active.
    bool IsActiveInAnyLayer() const { return active_layers_ != 0; }

    // Returns the "active" state of this node for the given |layer_bit|.
    bool IsActive(uint32_t layer_bit) const;

    // Sets the active state for this node in the given |layer_bit|.
    void SetActive(uint32_t layer_bit);
    void SetInactive(uint32_t layer_bit);

   private:
    // A bit-set corresponding to the priority layers in which this object is
    // active.
    uint32_t active_layers_ = 0;
  };

  // This is move-only because all of its members are move-only.
  // An instance of this will exist for any node that is referenced, either by a
  // direct Vote for that node, or as an input or output of a BoostedVote.
  class NodeData : public ActiveLayers {
   public:
    NodeData();
    NodeData(const NodeData& rhs) = delete;
    NodeData(NodeData&& rhs);
    NodeData& operator=(const NodeData& rhs) = delete;
    NodeData& operator=(NodeData&& rhs) = default;
    ~NodeData();

    const Vote& incoming_vote() const { return incoming_vote_.value(); }

    // Sets/Cancels the incoming vote.
    void SetIncomingVote(const Vote& incoming_vote) {
      DCHECK(!incoming_vote_.has_value());
      incoming_vote_ = incoming_vote;
    }
    void RemoveIncomingVote() {
      DCHECK(incoming_vote_.has_value());
      incoming_vote_ = absl::nullopt;
    }
    // Updates the incoming vote.
    void UpdateIncomingVote(const Vote& incoming_vote) {
      DCHECK(incoming_vote_.has_value());
      incoming_vote_ = incoming_vote;
    }

    // Sets/Cancels the outgoing vote.
    void SetOutgoingVote(const Vote& outgoing_vote) {
      DCHECK(!outgoing_vote_.has_value());
      outgoing_vote_ = outgoing_vote;
    }
    void CancelOutgoingVote() {
      DCHECK(outgoing_vote_.has_value());
      outgoing_vote_ = absl::nullopt;
    }
    // Updates the outgoing vote. Returns true if it changed.
    bool UpdateOutgoingVote(const Vote& outgoing_vote) {
      DCHECK(outgoing_vote_.has_value());
      if (outgoing_vote == outgoing_vote_.value())
        return false;

      outgoing_vote_ = outgoing_vote;
      return true;
    }

    // Returns true if this node has an active |incoming| vote. If false that
    // means this node exists only because it is referenced by a BoostedVote.
    // Same as |incoming_vote_.has_value()|, but more readable.
    bool HasIncomingVote() const { return incoming_vote_.has_value(); }

    // Returns true if this node has an active outgoing vote.
    bool HasOutgoingVote() const { return outgoing_vote_.has_value(); }

    // Returns true if this node is involved in any edges.
    bool HasEdges() const { return edge_count_ > 0; }

    // Returns the effective priority of this node based on the highest of the
    // values in |supporting_node_count_|.
    base::TaskPriority GetEffectivePriorityLevel() const;

    // For keeping track of the number of edges in which this node is involved.
    void IncrementEdgeCount();
    void DecrementEdgeCount();

    size_t edge_count_for_testing() const { return edge_count_; }

   private:
    // Counts the number of edges involving this node, both input and output.
    // When this goes to zero the node no longer needs an explicit
    // representation.
    size_t edge_count_ = 0;

    // The input vote we've received, if any.
    absl::optional<Vote> incoming_vote_;

    // The output vote we're emitted, if any.
    absl::optional<Vote> outgoing_vote_;
  };

  // NOTE: It is important that NodeDataMap preserve pointers to NodeData
  // through insertions and deletions in the map, as we take raw pointers to
  // objects in the map.
  using NodeDataMap = std::map<const ExecutionContext*, NodeData>;
  using NodeDataPtrSet = std::set<NodeDataMap::value_type*>;

  // For any given edge, this maintains the metadata associated with that
  // particular edge.
  class EdgeData : public ActiveLayers {
   public:
    EdgeData();
    EdgeData(const EdgeData&) = delete;
    EdgeData(EdgeData&&);
    EdgeData& operator=(const EdgeData&) = delete;
    EdgeData& operator=(EdgeData&&);
    ~EdgeData();

    // Adds a reason to the set of reasons associated with this edge.
    void AddReason(const char* reason);

    // Removes a reason from this edge. Returns true if this was the active
    // selected reason that had been forwarded, indicating that a new reason
    // needs to be chosen.
    bool RemoveReason(const char* reason);

    // Returns the active reason for this edge.
    const char* GetActiveReason() const;

    // Returns the total number of reasons associated with this edge. This is
    // effectively the multiplicity of the edge in the dependency graph.
    size_t GetReasonCount() const { return reasons_.size(); }

   private:
    // The reasons associated with this particular edge (one contribution per
    // BoostingVote). We really don't expect many multiple edges so a vector is
    // used to reduce allocations. This is semantically a multi-set.
    std::vector<const char*> reasons_;
  };

  // A helper for storing edges with different sort orders. Templated so that it
  // is strongly typed.
  template <bool kForward>
  class Edge {
   public:
    Edge(const ExecutionContext* src, const ExecutionContext* dst)
        : src_(src), dst_(dst) {}
    explicit Edge(const BoostingVote* boosting_vote)
        : src_(boosting_vote->input_execution_context()),
          dst_(boosting_vote->output_execution_context()) {}
    Edge(const Edge&) = default;
    ~Edge() {}

    Edge& operator=(const Edge&) = default;
    Edge& operator=(Edge&&) = delete;

    bool operator==(const Edge& rhs) const {
      return std::tie(src_, dst_) == std::tie(rhs.src_, rhs.dst_);
    }

    bool operator!=(const Edge& rhs) const { return !(*this == rhs); }

    // Forward edges sort by (src, dst), while reverse edges sort by (dst, src).
    bool operator<(const Edge& rhs) const {
      if (kForward)
        return std::tie(src_, dst_) < std::tie(rhs.src_, rhs.dst_);
      return std::tie(dst_, src_) < std::tie(rhs.dst_, rhs.src_);
    }

    const ExecutionContext* src() const { return src_; }
    const ExecutionContext* dst() const { return dst_; }

   private:
    raw_ptr<const ExecutionContext> src_ = nullptr;
    raw_ptr<const ExecutionContext> dst_ = nullptr;
  };
  using ForwardEdge = Edge<true>;
  using ReverseEdge = Edge<false>;

  // EdgeData is stored in the forward map, and a pointer to that data is
  // included in the reverse edge map.
  using ForwardEdges = std::map<ForwardEdge, EdgeData>;
  using ReverseEdges = std::map<ReverseEdge, EdgeData*>;

  // To be called by BoostingVote.
  void SubmitBoostingVote(const BoostingVote* boosting_vote);
  void CancelBoostingVote(const BoostingVote* boosting_vote);

  // VoteObserver implementation:
  void OnVoteSubmitted(VoterId voter_id,
                       const ExecutionContext* execution_context,
                       const Vote& vote) override;
  void OnVoteChanged(VoterId voter_id,
                     const ExecutionContext* execution_context,
                     const Vote& new_vote) override;
  void OnVoteInvalidated(VoterId voter_id,
                         const ExecutionContext* execution_context) override;

  // Helper functions for enumerating over incoming and outgoing edges.
  // |function| should accept a single input parameter that is a
  // ForwardEdge::iterator or ReverseEdge::iterator, as appropriate, and which
  // returns a bool. Returning true indicates the iteration should continue,
  // returning false indicates it should terminate.
  template <typename Function>
  void ForEachIncomingEdge(const ExecutionContext* node, Function&& function);
  template <typename Function>
  void ForEachOutgoingEdge(const ExecutionContext* node, Function&& function);

  // Finds or creates the node data associated with the given node.
  NodeDataMap::iterator FindOrCreateNodeData(const ExecutionContext* node);
  NodeDataMap::iterator FindNodeData(const ExecutionContext* node);

  // Returns the vote reason that should be associated with the given
  // node. This will preferentially select the reason that comes with a direct
  // vote if any is present; otherwise, it will select the active reason of the
  // active edge that causes the node itself to be active. Complexity is
  // O(|inbound edge count| + lg |total edge count|). This can return nullptr if
  // no non-null reasons have been provided.
  const char* GetVoteReason(const NodeDataMap::value_type* node_data_value);

  // Upstreams the vote for this |node| via its associated NodeData.
  void UpstreamVoteIfNeeded(NodeDataMap::value_type* node_data_value);

  // Upstreams changes that have been made to the provided set of nodes. This
  // takes care of deleted nodes if they no longer need to be represented in
  // the priority flow graph.
  void UpstreamChanges(const NodeDataPtrSet& changes);

  // Helper for removing a node from the NodeDataMap.
  void MaybeRemoveNode(NodeDataMap::iterator node_data_it);

  // Marks sub-tree rooted at |node| as inactive, and returns the nodes that
  // were deactivated in the provided output set.
  void MarkSubtreeInactive(uint32_t layer_bit,
                           NodeDataMap::value_type* node,
                           NodeDataPtrSet* deactivated);

  // Determines if the given node has an inbound active edge, returning an
  // iterator to it if there is one.
  ReverseEdges::iterator GetActiveInboundEdge(
      uint32_t layer_bit,
      const NodeDataMap::value_type* node);

  // Gets the nearest active ancestor of a given deactivated node. Returns
  // nullptr if there is none.
  NodeDataMap::value_type* GetNearestActiveAncestor(
      uint32_t layer_bit,
      const NodeDataMap::value_type* deactivated_node);

  // Given a set of inactive nodes, returns a search front corresponding to
  // all of their nearest active ancestors.
  void GetNearestActiveAncestors(uint32_t layer_bit,
                                 const NodeDataPtrSet& deactivated,
                                 NodeDataPtrSet* active_ancestors);

  // Given a search front of active nodes, explores outwards from those nodes
  // in order to generate a reachability spanning tree. Empties the
  // |active_search_front| as the search progresses, and populates |changes|
  // with the set of nodes that were made active as a result of the search.
  void MarkNodesActiveFromSearchFront(uint32_t layer_bit,
                                      NodeDataPtrSet* active_search_front,
                                      NodeDataPtrSet* activated);

  // Reprocesses the subtree rooted at the provided |node|. This is used to
  // repair the reachability spanning tree when the active edge inbound to
  // |node| is deleted. The set of nodes that have seen an active state toggle
  // or a change in vote reason are stored in |changes|, for use with
  // "UpstreamChanges".
  void ReprocessSubtree(uint32_t layer_bit,
                        NodeDataMap::value_type* node,
                        NodeDataPtrSet* changes);

  // Used by SubmitVote/ChangeVote and VoteInvalidated.
  void OnVoteAdded(uint32_t layer_bit,
                   NodeDataMap::value_type* node,
                   NodeDataPtrSet* changes);
  void OnVoteRemoved(uint32_t layer_bit,
                     NodeDataMap::value_type* node,
                     NodeDataPtrSet* changes);

  // Our input voter. We'll only accept votes from this voter otherwise we'll
  // DCHECK.
  voting::VoterId<Vote> input_voter_id_;

  // Our channel for upstreaming our votes.
  VotingChannel channel_;

  // Provides a VotingChannel to our input voter.
  VotingChannelFactory voting_channel_factory_{this};

  // Nodes and associated metadata in the "priority flow graph". An entry exists
  // in this map for any node that has an active non-default vote, or for any
  // node that is referenced by the "priority flow graph".
  NodeDataMap nodes_;

  // The collection of know BoostingVotes, describing the edges in the
  // "priority flow graph" as adjacency lists. Nodes are stored as instances of
  // NodeData.
  ForwardEdges forward_edges_;
  ReverseEdges reverse_edges_;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_BOOSTING_VOTE_AGGREGATOR_H_
