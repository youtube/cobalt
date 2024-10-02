// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_H_

#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/accessibility.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

class AXObject;
class AXObjectCacheImpl;
class InspectorDOMAgent;
class InspectedFrames;
class LocalFrame;

using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;

typedef blink::protocol::Accessibility::Backend::QueryAXTreeCallback
    QueryAXTreeCallback;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorBaseAgent<protocol::Accessibility::Metainfo> {
 public:
  InspectorAccessibilityAgent(InspectedFrames*, InspectorDOMAgent*);

  InspectorAccessibilityAgent(const InspectorAccessibilityAgent&) = delete;
  InspectorAccessibilityAgent& operator=(const InspectorAccessibilityAgent&) =
      delete;

  static void ProvideTo(LocalFrame* frame);

  // Base agent methods.
  void Trace(Visitor*) const override;
  void Restore() override;

  // Protocol methods.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getPartialAXTree(
      protocol::Maybe<int> dom_node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      protocol::Maybe<bool> fetch_relatives,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;
  protocol::Response getFullAXTree(
      protocol::Maybe<int> depth,
      protocol::Maybe<String> frame_id,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;
  protocol::Response getRootAXNode(
      protocol::Maybe<String> frame_id,
      std::unique_ptr<protocol::Accessibility::AXNode>* node) override;
  protocol::Response getAXNodeAndAncestors(
      protocol::Maybe<int> dom_node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
          out_nodes) override;
  protocol::Response getChildAXNodes(
      const String& in_id,
      protocol::Maybe<String> frame_id,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
          out_nodes) override;
  // Invoked by CDP clients such as Puppeteer. See
  // third_party/blink/public/devtools_protocol/browser_protocol.pdl for me
  // details.
  void queryAXTree(protocol::Maybe<int> dom_node_id,
                   protocol::Maybe<int> backend_node_id,
                   protocol::Maybe<String> object_id,
                   protocol::Maybe<String> accessibleName,
                   protocol::Maybe<String> role,
                   std::unique_ptr<QueryAXTreeCallback>) override;
  // An event was fired on the given AXObject, which should now also be
  // considered modified (as if AXObjectModified was called on it).
  void AXEventFired(AXObject* object, ax::mojom::blink::Event event);

  // The given AXObject (and possibly entire |subtree|) has changed.
  void AXObjectModified(AXObject* object, bool subtree);

  // Called by the AXObjectCache when a11y is clean and it is safe to traverse
  // the a11y tree and fetch object properties.
  void AXReadyCallback(Document& document);

  void ScheduleAXUpdateIfNeeded(TimerBase*, Document*);

 private:
  // Used to store the queries received by queryAXTree. The queries are
  // processed once the a11y tree is clean and ready for traversing
  // (AXReadyCallback).
  struct AXQuery {
   public:
    protocol::Maybe<int> dom_node_id;
    protocol::Maybe<int> backend_node_id;
    protocol::Maybe<String> object_id;
    protocol::Maybe<String> accessible_name;
    protocol::Maybe<String> role;
    std::unique_ptr<QueryAXTreeCallback> callback;
  };

  // Timer bound to a Document and an InspectorAccessibilityAgent instance,
  // similar to HeapTaskRunnerTimer
  // (third_party/blink/renderer/platform/timer.h).
  class DocumentTimer final : public TimerBase {
    DISALLOW_NEW();

   public:
    using TimerFiredFunction = void (InspectorAccessibilityAgent::*)(TimerBase*,
                                                                     Document*);

    explicit DocumentTimer(Document* document,
                           InspectorAccessibilityAgent* agent,
                           TimerFiredFunction function)
        : TimerBase(document->GetTaskRunner(TaskType::kInternalInspector)),
          agent_(agent),
          document_(document),
          function_(function) {}

    void Trace(Visitor* visitor) const {
      visitor->Trace(document_);
      visitor->Trace(agent_);
    }

    ~DocumentTimer() final = default;

   protected:
    void Fired() override { (agent_->*function_)(this, document_); }

    base::OnceClosure BindTimerClosure() final {
      return WTF::BindOnce(&DocumentTimer::RunInternalTrampoline,
                           WTF::Unretained(this),
                           WrapWeakPersistent(agent_.Get()),
                           WrapWeakPersistent(document_.Get()));
    }

   private:
    static void RunInternalTrampoline(DocumentTimer* timer,
                                      InspectorAccessibilityAgent* agent,
                                      Document* document) {
      // If both the document and the agent have been garbage collected,
      // the timer does not fire.
      if (agent && document)
        timer->RunInternal();
    }

    WeakMember<InspectorAccessibilityAgent> agent_;
    WeakMember<Document> document_;
    TimerFiredFunction function_;
  };

  void CompleteQuery(AXQuery&);
  bool MarkAXObjectDirty(AXObject* ax_object);
  // Unconditionally enables the agent, even if |enabled_.Get()==true|.
  // For idempotence, call enable().
  void EnableAndReset();
  std::unique_ptr<protocol::Array<AXNode>> WalkAXNodesToDepth(
      Document* document,
      int max_depth);
  std::unique_ptr<AXNode> BuildProtocolAXNodeForDOMNodeWithNoAXNode(
      int backend_node_id) const;
  std::unique_ptr<AXNode> BuildProtocolAXNodeForAXObject(
      AXObject&,
      bool force_name_and_role = false) const;
  std::unique_ptr<AXNode> BuildProtocolAXNodeForIgnoredAXObject(
      AXObject&,
      bool force_name_and_role) const;
  std::unique_ptr<AXNode> BuildProtocolAXNodeForUnignoredAXObject(
      AXObject&) const;
  void FillCoreProperties(AXObject&, AXNode*) const;
  void AddAncestors(AXObject& first_ancestor,
                    AXObject* inspected_ax_object,
                    std::unique_ptr<protocol::Array<AXNode>>& nodes,
                    AXObjectCacheImpl&) const;
  void AddChildren(AXObject& ax_object,
                   bool follow_ignored,
                   std::unique_ptr<protocol::Array<AXNode>>& nodes,
                   AXObjectCacheImpl&) const;
  LocalFrame* FrameFromIdOrRoot(const protocol::Maybe<String>& frame_id);
  void ScheduleAXChangeNotification(Document* document);
  AXObjectCacheImpl& AttachToAXObjectCache(Document*);
  void ProcessPendingQueries(Document&);
  void ProcessPendingDirtyNodes(Document&);

  Member<InspectedFrames> inspected_frames_;
  Member<InspectorDOMAgent> dom_agent_;
  InspectorAgentState::Boolean enabled_;
  HashSet<AXID> nodes_requested_;
  // The collected dirty AXObjects that should be refreshed in the AX tree view
  // in DevTools.
  HeapHashMap<WeakMember<Document>, Member<HeapHashSet<WeakMember<AXObject>>>>
      dirty_nodes_;
  // Time when every document was synced.
  HeapHashMap<WeakMember<Document>, base::Time> last_sync_times_;
  // The agent needs to keep AXContext because it enables caching of a11y nodes.
  HeapHashMap<WeakMember<Document>, std::unique_ptr<AXContext>>
      document_to_context_map_;
  // The agent keeps timers per document to make sure throttling of updates
  // happens per document.
  HeapHashMap<WeakMember<Document>, Member<DisallowNewWrapper<DocumentTimer>>>
      timers_;

  HeapHashMap<WeakMember<Document>, Vector<AXQuery>> queries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_ACCESSIBILITY_AGENT_H_
