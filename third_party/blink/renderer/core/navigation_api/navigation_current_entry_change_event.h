#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_CURRENT_ENTRY_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_CURRENT_ENTRY_CHANGE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class NavigationCurrentEntryChangeEventInit;
class NavigationHistoryEntry;

class NavigationCurrentEntryChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NavigationCurrentEntryChangeEvent* Create(
      const AtomicString& type,
      NavigationCurrentEntryChangeEventInit* init) {
    return MakeGarbageCollected<NavigationCurrentEntryChangeEvent>(type, init);
  }

  NavigationCurrentEntryChangeEvent(
      const AtomicString& type,
      NavigationCurrentEntryChangeEventInit* init);

  String navigationType() { return navigation_type_; }
  NavigationHistoryEntry* from() { return from_; }

  const AtomicString& InterfaceName() const final;
  void Trace(Visitor* visitor) const final;

 private:
  String navigation_type_;
  Member<NavigationHistoryEntry> from_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_CURRENT_ENTRY_CHANGE_EVENT_H_
