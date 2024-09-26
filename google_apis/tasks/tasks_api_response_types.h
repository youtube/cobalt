// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_
#define GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::tasks {

// https://developers.google.com/tasks/reference/rest/v1/tasklists
class TaskList {
 public:
  TaskList();
  TaskList(const TaskList&) = delete;
  TaskList& operator=(const TaskList&) = delete;
  ~TaskList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TaskList>* converter);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  const base::Time& updated() const { return updated_; }

 private:
  // Task list identifier.
  std::string id_;

  // Title of the task list.
  std::string title_;

  // Last modification time of the task list.
  base::Time updated_;
};

// Container for multiple `TaskList`s.
class TaskLists {
 public:
  TaskLists();
  TaskLists(const TaskLists&) = delete;
  TaskLists& operator=(const TaskLists&) = delete;
  ~TaskLists();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TaskLists>* converter);

  // Creates a `TaskLists` from parsed JSON.
  static std::unique_ptr<TaskLists> CreateFrom(const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<TaskList>>& items() const { return items_; }
  std::vector<std::unique_ptr<TaskList>>* mutable_items() { return &items_; }

 private:
  // Token that can be used to request the next page of this result.
  std::string next_page_token_;

  // `TaskList` items stored in this container.
  std::vector<std::unique_ptr<TaskList>> items_;
};

// https://developers.google.com/tasks/reference/rest/v1/tasks
class Task {
 public:
  // Status of the task.
  enum class Status {
    kUnknown,
    kNeedsAction,
    kCompleted,
  };

  Task();
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  ~Task();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(base::JSONValueConverter<Task>* converter);

  // Stringifies `Status` enum value.
  static std::string StatusToString(Status);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  Status status() const { return status_; }
  const std::string& parent_id() const { return parent_id_; }
  absl::optional<base::Time> due() { return due_; }

 private:
  // Task identifier.
  std::string id_;

  // Title of the task.
  std::string title_;

  // Status of the task.
  Status status_ = Status::kUnknown;

  // Parent task identifier.
  std::string parent_id_;

  // Due date of the task (comes as a RFC 3339 timestamp and converted to
  // `base::Time`). The due date only records date information. Not all tasks
  // have a due date.
  absl::optional<base::Time> due_ = absl::nullopt;
};

// Container for multiple `Task`s.
class Tasks {
 public:
  Tasks();
  Tasks(const Tasks&) = delete;
  Tasks& operator=(const Tasks&) = delete;
  ~Tasks();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(base::JSONValueConverter<Tasks>* converter);

  // Creates a `Tasks` from parsed JSON.
  static std::unique_ptr<Tasks> CreateFrom(const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<Task>>& items() const { return items_; }
  std::vector<std::unique_ptr<Task>>* mutable_items() { return &items_; }

 private:
  // Token that can be used to request the next page of this result.
  std::string next_page_token_;

  // `Task` items stored in this container.
  std::vector<std::unique_ptr<Task>> items_;
};

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_RESPONSE_TYPES_H_
