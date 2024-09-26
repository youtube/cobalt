// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_component.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/liburlpattern/parse.h"

namespace blink {

class ExceptionState;
class URLPatternInit;
class URLPatternOptions;
class URLPatternResult;

namespace url_pattern {
class Component;
}  // namespace url_pattern

class CORE_EXPORT URLPattern : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  using Component = url_pattern::Component;

 public:
  static URLPattern* Create(const V8URLPatternInput* input,
                            const String& base_url,
                            const URLPatternOptions* options,
                            ExceptionState& exception_state);

  static URLPattern* Create(const V8URLPatternInput* input,
                            const String& base_url,
                            ExceptionState& exception_state);

  static URLPattern* Create(const V8URLPatternInput* input,
                            const URLPatternOptions* options,
                            ExceptionState& exception_state);

  static URLPattern* Create(const V8URLPatternInput* input,
                            ExceptionState& exception_state);

  static URLPattern* Create(const URLPatternInit* init,
                            Component* precomputed_protocol_component,
                            const URLPatternOptions* options,
                            ExceptionState& exception_state);

  URLPattern(Component* protocol,
             Component* username,
             Component* password,
             Component* hostname,
             Component* port,
             Component* pathname,
             Component* search,
             Component* hash,
             base::PassKey<URLPattern> key);

  bool test(ScriptState* script_state,
            const V8URLPatternInput* input,
            const String& base_url,
            ExceptionState& exception_state) const;
  bool test(ScriptState* script_state,
            const V8URLPatternInput* input,
            ExceptionState& exception_state) const;

  URLPatternResult* exec(ScriptState* script_state,
                         const V8URLPatternInput* input,
                         const String& base_url,
                         ExceptionState& exception_state) const;
  URLPatternResult* exec(ScriptState* script_state,
                         const V8URLPatternInput* input,
                         ExceptionState& exception_state) const;

  String protocol() const;
  String username() const;
  String password() const;
  String hostname() const;
  String port() const;
  String pathname() const;
  String search() const;
  String hash() const;

  static int compareComponent(const V8URLPatternComponent& component,
                              const URLPattern* left,
                              const URLPattern* right);

  // Used for testing and debugging.
  String ToString() const;

  void Trace(Visitor* visitor) const override;

 private:
  // A utility function to determine if a given `input` matches the pattern or
  // not.  Returns `true` if there is a match and `false` otherwise.  If
  // `result` is not nullptr then the URLPatternResult contents will be filled.
  bool Match(ScriptState* script_state,
             const V8URLPatternInput* input,
             const String& base_url,
             URLPatternResult* result,
             ExceptionState& exception_state) const;

  // The compiled patterns for each URL component.
  Member<Component> protocol_;
  Member<Component> username_;
  Member<Component> password_;
  Member<Component> hostname_;
  Member<Component> port_;
  Member<Component> pathname_;
  Member<Component> search_;
  Member<Component> hash_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_H_
