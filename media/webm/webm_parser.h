// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_PARSER_H_
#define MEDIA_WEBM_WEBM_PARSER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// Interface for receiving WebM parser events.
//
// Each method is called when an element of the specified type is parsed.
// The ID of the element that was parsed is given along with the value
// stored in the element. List elements generate calls at the start and
// end of the list. Any pointers passed to these methods are only guaranteed
// to be valid for the life of that call. Each method returns a bool that
// indicates whether the parsed data is valid. If false is returned
// then the parse is immediately terminated and an error is reported by the
// parser.
class MEDIA_EXPORT WebMParserClient {
 public:
  virtual ~WebMParserClient();

  virtual bool OnListStart(int id) = 0;
  virtual bool OnListEnd(int id) = 0;
  virtual bool OnUInt(int id, int64 val) = 0;
  virtual bool OnFloat(int id, double val) = 0;
  virtual bool OnBinary(int id, const uint8* data, int size) = 0;
  virtual bool OnString(int id, const std::string& str) = 0;
  virtual bool OnSimpleBlock(int track_num, int timecode,
                             int flags,
                             const uint8* data, int size) = 0;
 protected:
  WebMParserClient();

  DISALLOW_COPY_AND_ASSIGN(WebMParserClient);
};

struct ListElementInfo;

// Parses a WebM list element and all of its children. This
// class supports incremental parsing of the list so Parse()
// can be called multiple times with pieces of the list.
// IsParsingComplete() will return true once the entire list has
// been parsed.
class MEDIA_EXPORT WebMListParser {
 public:
  // |id| - Element ID of the list we intend to parse.
  explicit WebMListParser(int id);
  ~WebMListParser();

  // Resets the state of the parser so it can start parsing a new list.
  void Reset();

  // Parses list data contained in |buf|.
  // |client| Called as different elements in the list are parsed.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int Parse(const uint8* buf, int size, WebMParserClient* client);

  // Returns true if the entire list has been parsed.
  bool IsParsingComplete() const;

 private:
  enum State {
    NEED_LIST_HEADER,
    INSIDE_LIST,
    DONE_PARSING_LIST,
    PARSE_ERROR,
  };

  struct ListState {
    int id_;
    int size_;
    int bytes_parsed_;
    const ListElementInfo* element_info_;
  };

  void ChangeState(State new_state);

  // Parses a single element in the current list.
  //
  // |header_size| - The size of the element header
  // |id| - The ID of the element being parsed.
  // |element_size| - The size of the element body.
  // |data| - Pointer to the element contents.
  // |size| - Number of bytes in |data|
  // |client| - Client to pass the parsed data to.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseListElement(int header_size,
                       int id, int64 element_size,
                       const uint8* data, int size,
                       WebMParserClient* client);

  // Called when starting to parse a new list.
  //
  // |id| - The ID of the new list.
  // |size| - The size of the new list.
  // |client| - The client object to notify that a new list is being parsed.
  //
  // Returns true if this list can be started in the current context. False
  // if starting this list causes some sort of parse error.
  bool OnListStart(int id, int64 size, WebMParserClient* client);

  // Called when the end of the current list has been reached. This may also
  // signal the end of the current list's ancestors if the current list happens
  // to be at the end of its parent.
  //
  // |client| - The client to notify about lists ending.
  //
  // Returns true if no errors occurred while ending this list(s).
  bool OnListEnd(WebMParserClient* client);

  State state_;

  // Element ID passed to the constructor.
  int root_id_;

  // Element level for |root_id_|. Used to verify that elements appear at
  // the correct level.
  int root_level_;

  // Stack of state for all the lists currently being parsed. Lists are
  // added and removed from this stack as they are parsed.
  std::vector<ListState> list_state_stack_;

  DISALLOW_COPY_AND_ASSIGN(WebMListParser);
};

// Parses an element header & returns the ID and element size.
//
// Returns < 0 if the parse fails.
// Returns 0 if more data is needed.
// Returning > 0 indicates success & the number of bytes parsed.
// |*id| contains the element ID on success and is undefined otherwise.
// |*element_size| contains the element size on success and is undefined
//                 otherwise.
int WebMParseElementHeader(const uint8* buf, int size,
                           int* id, int64* element_size);

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_PARSER_H_
