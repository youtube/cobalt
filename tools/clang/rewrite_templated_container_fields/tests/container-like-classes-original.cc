// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <vector>

class List {
 public:
  // Expected rewrite: List(const std::vector<raw_ptr<int>>& arg):
  // member_(arg){}
  List(const std::vector<int*>& arg) : member_(arg) {}

  // Expected rewrite: std::vector<raw_ptr<int>>::iterator begin()
  std::vector<int*>::iterator begin() { return member_.begin(); }

  // Expected rewrite: std::vector<raw_ptr<int>>::const_iterator end() const
  std::vector<int*>::iterator end() { return member_.end(); }

  // Expected rewrite: std::vector<raw_ptr<int>>::const_iterator begin() const
  std::vector<int*>::const_iterator begin() const { return member_.begin(); }

  // Expected rewrite: std::vector<raw_ptr<int>>::const_iterator end() const
  std::vector<int*>::const_iterator end() const { return member_.end(); }

 private:
  // Expected rewrite: std::vector<raw_ptr<int>> member_;
  std::vector<int*> member_;
};

List* get_ptr() {
  return nullptr;
}

void fct() {
  // Expected rewrite: std::vector<raw_ptr<int>> temp;
  std::vector<int*> temp;
  temp.push_back(nullptr);

  List l(temp);

  // Expected rewrite: for (int* i : l)
  for (auto* i : l) {
    (void)i;
  }

  List* ptr = &l;
  // Expected rewrite: for (int* i : *ptr)
  for (auto* i : *ptr) {
    (void)i;
  }

  // Expected rewrite: for (int* i : *get_ptr())
  for (auto* i : *get_ptr()) {
    (void)i;
  }
}
