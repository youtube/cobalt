// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANNOTATION_ANNOTATION_AGENT_CONTAINER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANNOTATION_ANNOTATION_AGENT_CONTAINER_IMPL_H_

#include "base/types/pass_key.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AnnotationAgentContainerImplTest;
class AnnotationAgentImpl;
class AnnotationSelector;
class LocalFrame;
class AnnotationAgentGenerator;
class TextFragmentSelectorGenerator;

// This class provides a per-Document container for AnnotationAgents. It is
// used primarily as an entrypoint to allow clients to create an
// AnnotationAgent in a particular document.
//
// Agents can either be created via Mojo, in which case they are bound to a
// corresponding host object, or they can be created unbound from within Blink.
//
// The container is created lazily on demand.
class CORE_EXPORT AnnotationAgentContainerImpl final
    : public GarbageCollected<AnnotationAgentContainerImpl>,
      public mojom::blink::AnnotationAgentContainer,
      public Supplement<Document> {
 public:
  using PassKey = base::PassKey<AnnotationAgentContainerImpl>;

  static const char kSupplementName[];

  // Static getter for the container for the given document. Will instantiate a
  // container if the document doesn't yet have one. This can return nullptr if
  // requested from an inactive or detached document.
  static AnnotationAgentContainerImpl* From(Document&);

  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver);

  // Only instantiated using static From method.
  explicit AnnotationAgentContainerImpl(
      Document&,
      base::PassKey<AnnotationAgentContainerImpl>);
  ~AnnotationAgentContainerImpl() override = default;

  // Not copyable or movable
  AnnotationAgentContainerImpl(const AnnotationAgentContainerImpl&) = delete;
  AnnotationAgentContainerImpl& operator=(const AnnotationAgentContainerImpl&) =
      delete;

  void Bind(
      mojo::PendingReceiver<mojom::blink::AnnotationAgentContainer> receiver);

  void Trace(Visitor* visitor) const override;

  // Notifies the container when parsing in its document has finished.
  void FinishedParsing();

  // Removes the given agent from this container. It is an error to try and
  // remove an agent from a container that doesn't hold it. Once removed, the
  // agent is no longer usable and cannot be added back. Agents can only be
  // removed via AnnotationAgentImpl::Remove.
  void RemoveAgent(AnnotationAgentImpl& agent,
                   base::PassKey<AnnotationAgentImpl>);

  // Returns all annotation agents in this container of the given type.
  HeapHashSet<Member<AnnotationAgentImpl>> GetAgentsOfType(
      mojom::blink::AnnotationType type);

  // Use from within Blink to create an agent in this container.
  AnnotationAgentImpl* CreateUnboundAgent(mojom::blink::AnnotationType type,
                                          AnnotationSelector& selector);

  // mojom::blink::AnnotationAgentContainer
  void CreateAgent(
      mojo::PendingRemote<mojom::blink::AnnotationAgentHost> host_remote,
      mojo::PendingReceiver<mojom::blink::AnnotationAgent> agent_receiver,
      mojom::blink::AnnotationType type,
      const String& serialized_selector) override;
  void CreateAgentFromSelection(
      mojom::blink::AnnotationType type,
      CreateAgentFromSelectionCallback callback) override;

  void OpenedContextMenuOverSelection();

 private:
  friend AnnotationAgentContainerImplTest;

  bool ShouldPreemptivelyGenerate();

  void DidFinishSelectorGeneration(
      CreateAgentFromSelectionCallback callback,
      mojom::blink::AnnotationType type,
      shared_highlighting::LinkGenerationReadyStatus ready_status,
      const String& selected_text,
      const TextFragmentSelector& selector,
      shared_highlighting::LinkGenerationError error);

  Member<AnnotationAgentGenerator> annotation_agent_generator_;

  HeapMojoReceiverSet<mojom::blink::AnnotationAgentContainer,
                      AnnotationAgentContainerImpl>
      receivers_;

  HeapHashSet<Member<AnnotationAgentImpl>> agents_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANNOTATION_ANNOTATION_AGENT_CONTAINER_IMPL_H_
