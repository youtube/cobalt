/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InputStreamPreprocessor_h
#define InputStreamPreprocessor_h

#include "html_character_provider.h"

namespace WebCore {

// http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
template <typename Tokenizer>
class InputStreamPreprocessor {
public:
    InputStreamPreprocessor(Tokenizer* tokenizer)
        : m_tokenizer(tokenizer)
    {
        reset();
    }

    InputStreamPreprocessor(const InputStreamPreprocessor&) = delete;
    InputStreamPreprocessor& operator=(const InputStreamPreprocessor&) = delete;

    ALWAYS_INLINE UChar nextInputCharacter() const { return m_nextInputCharacter; }

    // Returns whether we succeeded in peeking at the next character.
    // The only way we can fail to peek is if there are no more
    // characters in |source| (after collapsing \r\n, etc).
    ALWAYS_INLINE bool peek(CharacterProvider& source)
    {
        m_nextInputCharacter = source.currentCharacter();

        // Every branch in this function is expensive, so we have a
        // fast-reject branch for characters that don't require special
        // handling. Please run the parser benchmark whenever you touch
        // this function. It's very hot.
        static const UChar specialCharacterMask = '\n' | '\r' | '\0';
        if (m_nextInputCharacter & ~specialCharacterMask) {
            m_skipNextNewLine = false;
            return true;
        }
        return processNextInputCharacter(source);
    }

    // Returns whether there are more characters in |source| after advancing.
    ALWAYS_INLINE bool advance(CharacterProvider& source)
    {
        source.next();
        if (source.isEmpty())
            return false;
        return peek(source);
    }

    void reset(bool skipNextNewLine = false)
    {
        m_nextInputCharacter = '\0';
        m_skipNextNewLine = skipNextNewLine;
    }

private:
    bool processNextInputCharacter(CharacterProvider& source)
    {
    ProcessAgain:
        ASSERT(m_nextInputCharacter == source.currentCharacter());

        if (m_nextInputCharacter == '\n' && m_skipNextNewLine) {
            m_skipNextNewLine = false;
            source.next();
            if (source.isEmpty())
                return false;
            m_nextInputCharacter = source.currentCharacter();
        }
        if (m_nextInputCharacter == '\r') {
            m_nextInputCharacter = '\n';
            m_skipNextNewLine = true;
        } else {
            m_skipNextNewLine = false;
            // FIXME: The spec indicates that the surrogate pair range as well as
            // a number of specific character values are parse errors and should be replaced
            // by the replacement character. We suspect this is a problem with the spec as doing
            // that filtering breaks surrogate pair handling and causes us not to match Minefield.
            if (m_nextInputCharacter == '\0' && !shouldTreatNullAsEndOfFileMarker(source)) {
                if (m_tokenizer->shouldSkipNullCharacters()) {
                    source.next();
                    if (source.isEmpty())
                        return false;
                    m_nextInputCharacter = source.currentCharacter();
                    goto ProcessAgain;
                }
                m_nextInputCharacter = 0xFFFD;
            }
        }
        return true;
    }

    bool shouldTreatNullAsEndOfFileMarker(CharacterProvider& source) const
    {
        return source.remainingBytes() == 1;
    }

    Tokenizer* m_tokenizer;

    // http://www.whatwg.org/specs/web-apps/current-work/#next-input-character
    UChar m_nextInputCharacter;
    bool m_skipNextNewLine;
};

}

#endif // InputStreamPreprocessor_h

