// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Document;
class Element;
class IdTargetObserver;
class LayoutSVGResourceContainer;
class QualifiedName;
class SVGFilterPrimitiveStandardAttributes;
class SVGResourceClient;
class SVGResourceDocumentContent;
class TreeScope;

// A class tracking a reference to an SVG resource (an element that constitutes
// a paint server, mask, clip-path, filter et.c.)
//
// Elements can be referenced using CSS, for example like:
//
//   filter: url(#baz);                             ("local")
//
//    or
//
//   filter: url(foo.com/bar.svg#baz);              ("external")
//
// SVGResource provide a mechanism to persistently reference an element in such
// cases - regardless of if the element reside in the same document (read: tree
// scope) or in an external (resource) document. Loading events related to the
// external documents case are handled by the SVGResource.
//
// For same document references, changes that could affect the 'id' lookup will
// be tracked, to handle elements being added, removed or having their 'id'
// mutated. (This does not apply for the external document case because it's
// assumed they will not mutate after load, due to scripts not being run etc.)
//
// SVGResources are created, and managed, either by SVGTreeScopeResources
// (local) or CSSURIValue (external), and have SVGResourceClients as a means to
// deliver change notifications. Clients that are interested in change
// notifications hence need to register a SVGResourceClient with the
// SVGResource. Most commonly this registration would take place when the
// computed style changes.
//
// The element is bound either when the SVGResource is created (for local
// resources) or after the referenced resource has completed loading (for
// external resources.)
//
// As content is mutated, clients will get notified via the SVGResource.
//
// <event> -> SVG...Element -> SVGResource -> SVGResourceClient(0..N)
//
class SVGResource : public GarbageCollected<SVGResource> {
 public:
  SVGResource(const SVGResource&) = delete;
  SVGResource& operator=(const SVGResource&) = delete;
  virtual ~SVGResource();

  virtual void Load(Document&) {}
  virtual void LoadWithoutCSP(Document&) {}

  Element* Target() const { return target_; }
  // Returns the target's LayoutObject (if target exists and is attached to the
  // layout tree). Also perform cycle-checking, and may thus return nullptr if
  // this SVGResourceClient -> SVGResource reference would start a cycle.
  LayoutSVGResourceContainer* ResourceContainer(SVGResourceClient&) const;
  // Same as the above, minus the cycle-checking.
  LayoutSVGResourceContainer* ResourceContainerNoCycleCheck() const;
  // Run cycle-checking for this SVGResourceClient -> SVGResource
  // reference. Used internally by the cycle-checking, and shouldn't be called
  // directly in general.
  bool FindCycle(SVGResourceClient&) const;

  void AddClient(SVGResourceClient&);
  void RemoveClient(SVGResourceClient&);

  virtual void Trace(Visitor*) const;

 protected:
  SVGResource();

  void InvalidateCycleCache();
  void NotifyContentChanged();

  Member<Element> target_;

  enum CycleState {
    kNeedCheck,
    kPerformingCheck,
    kHasCycle,
    kNoCycle,
  };
  struct ClientEntry {
    int count = 0;
    CycleState cached_cycle_check = kNeedCheck;
  };
  mutable HeapHashMap<Member<SVGResourceClient>, ClientEntry> clients_;
};

// Local resource reference (see SVGResource.)
class LocalSVGResource final : public SVGResource {
 public:
  LocalSVGResource(TreeScope&, const AtomicString& id);

  void Unregister();

  using SVGResource::NotifyContentChanged;

  void NotifyFilterPrimitiveChanged(
      SVGFilterPrimitiveStandardAttributes& primitive,
      const QualifiedName& attribute);

  void Trace(Visitor*) const override;

 private:
  void TargetChanged(const AtomicString& id);

  Member<TreeScope> tree_scope_;
  Member<IdTargetObserver> id_observer_;
};

// External resource reference (see SVGResource.)
class ExternalSVGResource final : public SVGResource, public ResourceClient {
 public:
  explicit ExternalSVGResource(const KURL&);

  void Load(Document&) override;
  void LoadWithoutCSP(Document&) override;

  void Trace(Visitor*) const override;

 private:
  Element* ResolveTarget();

  // ResourceClient implementation
  void NotifyFinished(Resource*) override;
  String DebugName() const override;

  Member<SVGResourceDocumentContent> document_content_;
  KURL url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_H_
