// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_course_work_item.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
class Time;
}  // namespace base

namespace google_apis::classroom {
class Courses;
class CourseWork;
class StudentSubmissions;
}  // namespace google_apis::classroom

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

// Provides implementation for `GlanceablesClassroomClient`. Responsible for
// communication with Google Classroom API.
class GlanceablesClassroomClientImpl : public GlanceablesClassroomClient {
 public:
  // Provides an instance of `google_apis::RequestSender` for the client.
  using CreateRequestSenderCallback =
      base::RepeatingCallback<std::unique_ptr<google_apis::RequestSender>(
          const std::vector<std::string>& scopes,
          const net::NetworkTrafficAnnotationTag& traffic_annotation_tag)>;

  using SortComparator = base::RepeatingCallback<bool(
      const GlanceablesClassroomCourseWorkItem* lhs,
      const GlanceablesClassroomCourseWorkItem* rhs)>;

  GlanceablesClassroomClientImpl(
      base::Clock* clock,
      const CreateRequestSenderCallback& create_request_sender_callback,
      bool use_best_effort_prefetch_task_runner = true);
  GlanceablesClassroomClientImpl(const GlanceablesClassroomClientImpl&) =
      delete;
  GlanceablesClassroomClientImpl& operator=(
      const GlanceablesClassroomClientImpl&) = delete;
  ~GlanceablesClassroomClientImpl() override;

  // GlanceablesClassroomClient:
  void IsStudentRoleActive(IsRoleEnabledCallback callback) override;
  void GetCompletedStudentAssignments(GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithMissedDueDate(
      GetAssignmentsCallback callback) override;
  void GetStudentAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void IsTeacherRoleActive(IsRoleEnabledCallback callback) override;
  void GetTeacherAssignmentsWithApproachingDueDate(
      GetAssignmentsCallback callback) override;
  void GetTeacherAssignmentsRecentlyDue(
      GetAssignmentsCallback callback) override;
  void GetTeacherAssignmentsWithoutDueDate(
      GetAssignmentsCallback callback) override;
  void GetGradedTeacherAssignments(GetAssignmentsCallback callback) override;
  void OnGlanceablesBubbleClosed() override;

  void set_number_of_assignments_prioritized_for_display_for_testing(
      size_t value) {
    number_of_assignments_prioritized_for_display_ = value;
  }

  // If `teacher_data_prefetch_timer_` is running, fires it.
  // `prefetch_callback`, if set, will run when the data requested by the
  // prefetch gets loaded.
  // No-op if the timer is not running. `prefetch_callback` gets ignored in this
  // case.
  // Returns whether the timer was fired.
  bool FireTeacherDataPrefetchTimerIfRunningForTesting(
      base::OnceClosure prefetch_callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest, FetchCourses);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCoursesOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCoursesMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest, FetchCourseWork);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkAndSubmissions);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkAndSubmissionsMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissions);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissionsOnHttpError);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchStudentSubmissionsMultiplePages);
  FRIEND_TEST_ALL_PREFIXES(GlanceablesClassroomClientImplTest,
                           FetchCourseWorkAfterStudentSubmissions);

  // Done callback for fetching all courses for student or teacher roles.
  using CourseList = std::vector<std::unique_ptr<GlanceablesClassroomCourse>>;
  using FetchCoursesCallback =
      base::OnceCallback<void(bool success, const CourseList& courses)>;

  using CourseWorkInfo =
      base::flat_map<std::string, GlanceablesClassroomCourseWorkItem>;
  using CourseWorkPerCourse = base::flat_map<std::string, CourseWorkInfo>;

  enum class FetchStatus {
    // The data needs to be fetched - either because it was never fetched, or
    // glanceables bubble was closed since the data was last fetched.
    kNotFetched,

    // The data fetch is in progress.
    kFetching,

    // The data fetch is in progress, but the glanceables bubble was closed
    // before the fetch finished.
    kFetchingInvalidated,

    // The data has been fetched.
    kFetched
  };

  // Flavours of course work data handled by the client.
  enum class CourseWorkType { kStudent, kTeacher };

  // Tracks a course list state - the latest fetched list, the fetch status, and
  // the list of callbacks waiting for the list to be fetched.
  class CourseListState {
   public:
    CourseListState();
    CourseListState(const CourseListState&) = delete;
    CourseListState& operator=(const CourseListState&) = delete;
    ~CourseListState();

    // If the course list is fetched, it runs the `callback` immediately.
    // Otherwise, it enqueues the callback to be run when the list gets fetched.
    // It updates the fetch status to indicate that the list is to be fetched.
    // Returns whether the client should initiate course list request.
    bool RunOrEnqueueCallbackAndUpdateFetchStatus(
        FetchCoursesCallback callback);

    // Called by the `GlanceablesClassroomClientImpl` to update the course list
    // state when a course list fetch request completes.
    // It updates the cached course list, and updates the fetch state, and runs
    // any pending course list callbacks.
    // `fetched_courses` - the list of fetched courses, nullptr if the course
    // list fetch request failed.
    void FinalizeFetch(std::unique_ptr<CourseList> fetched_courses);

    // Updates fetch status to indicate it can be refetched when course list is
    // requested next time. Called when the glanceables bubble gets closed.
    void InvalidateFetchStatus();

    const CourseList& courses() const { return courses_; }

   private:
    // Runs pending callbacks in `callbacks_` and passes them the latest cached
    // course list.
    void RunCallbacks(bool success);

    CourseList courses_;
    FetchStatus fetch_status_ = FetchStatus::kNotFetched;
    std::vector<FetchCoursesCallback> callbacks_;
  };

  // Wrapper around course work fetch callback that tracks the number of pending
  // course work page requests.
  // While individual pages need to be fetched serially, course work fetch may
  // require fetching student submissions for course work in each of the course
  // work pages. In that case, a page request is deemed complete when all
  // required student submissions are fetched. Fetching student submissions for
  // a course work page does not block fetch of the next course work page, which
  // means that handling of different course work pages may overlap.
  class CourseWorkRequest {
   public:
    explicit CourseWorkRequest(base::OnceClosure callback);
    CourseWorkRequest(const CourseWorkRequest&) = delete;
    CourseWorkRequest& operator=(const CourseWorkRequest&) = delete;
    ~CourseWorkRequest();

    // Increases the count of pending course work page requests - should be
    // called when a fetch for a course work page is initiated.
    void IncrementPendingPageCount();

    // Decrease the count of pending course work page requests - should be
    // called when a fetch for a course work page, including student submissions
    // data (when required) completes.
    void DecrementPendingPageCount();

    // If no more page tokens are pending, runs the `callback_`.
    // Returns whether the callback was run. If the callback is run, the object
    // can be discarded. and `RespondIfComplete()` should not be called any
    // longer.
    bool RespondIfComplete();

   private:
    base::OnceClosure callback_;
    int pending_page_requests_ = 0;
  };

  // Updates the `*fetch_status` in response to the glanceables bubble closing -
  // it updates the fetch status to indicate that the data can be refetched when
  // requested again.
  static void InvalidateFetchStatus(FetchStatus* fetch_status);

  // Whether student submissions should be fetched per course work item, or per
  // course.
  bool ShouldFetchSubmissionsPerCourseWork(
      CourseWorkType course_work_type) const;

  // Gets a reference to course work data for the provided course work type.
  CourseWorkPerCourse& GetCourseWork(CourseWorkType type);

  // Called when an API call to get part of course work data for the course work
  // type fails.
  void SetCourseWorkFetchHadFailure(CourseWorkType type);

  // Fetches all courses for student and teacher roles and invokes `callback`
  // when done.
  void FetchStudentCourses(FetchCoursesCallback callback);
  void FetchTeacherCourses(FetchCoursesCallback callback);

  // Fetches all course work items for the specified `course_id` and invokes
  // `callback` when done. The course work information is saved in the course
  // work map for `course_work_type` (either `student_course_work_` or
  // `teacher_course_work_`).
  void FetchCourseWork(const std::string& course_id,
                       CourseWorkType course_work_type,
                       base::OnceClosure callback);

  // Fetches all student submissions for the specified `course_id` and
  // `course_work_id` and invokes `callback` when done.
  // To requests student submissions for all course work item in the course,
  // pass in `course_work_id` value "-".
  // The fetched student submissions get added to the course work map for
  // `course_work_type` (either `student_course_work_` or
  // `teacher_course_work_`).
  void FetchStudentSubmissions(const std::string& course_id,
                               const std::string& course_work_id,
                               CourseWorkType course_work_type,
                               base::OnceClosure callback);

  // Callback called when either student or teacher data fetch completes. The
  // callback is expected to process the student/teacher data. Returns true if
  // the data has been processed successfully, and false if additional data
  // was requested.
  using DataFetchCallback = base::OnceCallback<bool()>;

  // Delays executing `callback` until all student data are fetched.
  void InvokeOnceStudentDataFetched(DataFetchCallback callback);

  // Delays executing `callback` until all teacher data are fetched.
  void InvokeOnceTeacherDataFetched(DataFetchCallback callback);

  // Fetches one page of courses.
  // `student_id`         - restricts returned courses to those having a student
  //                        with the specified identifier. Use an empty string
  //                        to avoid filtering by student id.
  // `teacher_id`         - restricts returned courses to those having a teacher
  //                        with the specified identifier. Use an empty string
  //                        to avoid filtering by teacher id.
  // `page_token`         - token specifying the result page to return, comes
  //                        from the previous fetch request. Use an empty string
  //                        to fetch the first page.
  // `target_course_list` - reference to the course list that is being fetched,
  //                        and that needs to be updated once the full course
  //                        list gets fetched.
  // `fetched_courses`    - the container to which course items returned during
  //                        course list fetch are saved. This container will be
  //                        passed to `callback` once all items have been
  //                        fetched.
  void FetchCoursesPage(const std::string& student_id,
                        const std::string& teacher_id,
                        const std::string& page_token,
                        CourseListState& target_course_list,
                        std::unique_ptr<CourseList> fetched_courses);

  // Callback for `FetchCoursesPage()`. If `next_page_token()` in the `result`
  // is not empty - calls another `FetchCoursesPage()`, otherwise runs done
  // `callback`.
  void OnCoursesPageFetched(
      const std::string& student_id,
      const std::string& teacher_id,
      CourseListState& target_course_list,
      std::unique_ptr<CourseList> fetched_courses,
      const base::Time& request_start_time,
      base::expected<std::unique_ptr<google_apis::classroom::Courses>,
                     google_apis::ApiErrorCode> result);

  // Callback for `FetchStudentCourses()` or `FetchTeacherCourses()`. Triggers
  // fetching course work and student submissions for fetched `courses` and
  // invokes `on_course_work_and_student_submissions_fetched` when done.
  // `course_work_type` indicates the flavour of course work information that's
  // being fetched, and is used to determine the course work map where the
  // course work and student submissions whose fetch gets requested should be
  // saved.
  void OnCoursesFetched(
      CourseWorkType course_work_type,
      base::OnceClosure on_course_work_and_student_submissions_fetched,
      bool success,
      const CourseList& target_course_list);

  // Fetches one page of course work items.
  // `request_id`       - the ID for the course work request that's being
  //                      handled. It can be used to get the associated
  //                      `CourseWorkRequest` from `course_work_requests_`.
  // `course_id`        - identifier of the course.
  // `page_token`       - token specifying the result page to return, comes from
  //                      the previous fetch request. Use an empty string to
  //                      fetch the first page.
  // `page_number`      - 1-based page number of this fetch request. Used for
  //                      UMA to track the total number of pages needed to
  //                      fetch.
  // `course_work_type` - The flavour of course work information being fetched.
  //                      Determines the course work map where course work
  //                      information gets saved, and whether student
  //                      submissions need to be fetched per course work item.
  void FetchCourseWorkPage(int request_id,
                           const std::string& course_id,
                           const std::string& page_token,
                           int page_number,
                           CourseWorkType course_work_type);

  // Callback for `FetchCourseWorkPage()`. If `next_page_token()` in the
  // `result` is not empty - calls another `FetchCourseWorkPage()`, otherwise
  // runs done `callback`.
  void OnCourseWorkPageFetched(
      int request_id,
      const std::string& course_id,
      CourseWorkType course_work_type,
      const base::Time& request_start_time,
      int page_number,
      base::expected<std::unique_ptr<google_apis::classroom::CourseWork>,
                     google_apis::ApiErrorCode> result);

  // Fetches one page of student submissions.
  // `course_id`        - identifier of the course.
  // `course_work_id`   - identifier of the course work item. May be "-" to
  //                      request student submissions for all course work in the
  //                      course.
  // `page_token`       - token specifying the result page to return, comes from
  //                      the previous fetch request. Use an empty string to
  //                      fetch the first page.
  // `page_number`      - 1-based page number of this fetch request. Used for
  //                      UMA to track the total number of pages needed to
  //                      fetch.
  // `course_work_type` - The flavour of course work information being fetched.
  //                      Determines the course work map where student
  //                      submissions information gets saved.
  // `callback`         - a callback that runs when all student submissions in a
  //                      course have been fetched. This may require multiple
  //                      fetch requests, in this case `callback` gets called
  //                      when the final request completes.
  void FetchStudentSubmissionsPage(const std::string& course_id,
                                   const std::string& course_work_id,
                                   const std::string& page_token,
                                   int page_number,
                                   CourseWorkType course_work_type,
                                   base::OnceClosure callback);

  // Callback for `FetchStudentSubmissionsPage()`. If `next_page_token()` in the
  // `result` is not empty - calls another `FetchStudentSubmissionsPage()`,
  // otherwise runs done `callback`.
  void OnStudentSubmissionsPageFetched(
      const std::string& course_id,
      const std::string& course_work_id,
      CourseWorkType course_work_type,
      const base::Time& request_start_time,
      int page_number,
      base::OnceClosure callback,
      base::expected<
          std::unique_ptr<google_apis::classroom::StudentSubmissions>,
          google_apis::ApiErrorCode> result);

  // Callback for requests to fetch student submissions for all course work
  // items within a course work list page. The student submissions fetch is a
  // subtask of a course work request, which is identified by `request_id`.
  // When processing a page in course work list response, student submissions
  // may get requested for each course work item - this callback is called
  // when all requested student submission lists have been fetched.
  void OnCourseWorkSubmissionsFetched(int request_id,
                                      const std::string& course_id);

  // Invokes all pending callbacks from `callbacks_waiting_for_student_data_`
  // once all student data are fetched (courses + course work + student
  // submissions).
  void OnStudentDataFetched(const base::Time& sequence_start_time);

  // Invokes all pending callbacks from `callbacks_waiting_for_teacher_data_`
  // once all teacher data are fetched (courses + course work + student
  // submissions).
  void OnTeacherDataFetched(const base::Time& sequence_start_time);

  // Selects student assignments that satisfy both filtering predicates below.
  // `due_predicate`              - returns `true` if passed due date/time
  //                                satisfies filtering requirements.
  // `submission_state_predicate` - returns `true` if passed submission state
  //                                satisfies filtering requirements.
  // `sort_comparator`            - the function used when comparing two
  //                                assignments for sorting.
  // `callback`                   - invoked with filtered results.
  //
  // Returns whether the `callback` has been run. The current implementation
  // always returns true - the return value is bool for consistency with
  // `GetFilteredTeacherAssignments()`.
  bool GetFilteredStudentAssignments(
      base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
          due_predicate,
      base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmissionState)>
          submission_state_predicate,
      SortComparator sort_comparator,
      GetAssignmentsCallback callback);

  // Selects teacher assignments that satisfy the filtering below.
  // `due_predicate`              - returns `true` if passed due date/time
  //                                satisfies filtering requirements.
  // `graded`                     - whether or not we only want to include
  //                                course work which has a grade for every
  //                                submission.
  // `sort_comparator`            - the function used when comparing two
  //                                assignments for sorting.
  // `allow_submissions_refresh`  - whether student submissions for course work
  //                                items whose submission state is not fresh
  //                                can be refetched, and callback delayed until
  //                                the submissions have been refreshed.
  // `callback`                   - invoked with filtered results.
  //
  // Returns whether the `callback` has been run. It returns false if the
  // callbask was delayed to refresh submissions state of a subset of filtered
  // assignments.
  bool GetFilteredTeacherAssignments(
      base::RepeatingCallback<bool(const absl::optional<base::Time>&)>
          due_predicate,
      base::RepeatingCallback<bool(GlanceablesClassroomStudentSubmissionState)>
          submission_state_predicate,
      SortComparator sort_comparator,
      bool allow_submissions_refresh,
      GetAssignmentsCallback callback);

  // Removes all invalid course work items from `course_work` for courses in
  // the course list.
  void PruneInvalidCourseWork(const CourseList& courses,
                              CourseWorkPerCourse& course_work);

  // Triggers teacher data fetch. Runs with a delay after login. The goal is to
  // fetch course work student submissions state early, so older submissions
  // don't have to be refetched when the UI is first shown.
  void PrefetchTeacherData();

  // Returns lazily initialized `request_sender_`.
  google_apis::RequestSender* GetRequestSender();

  // Clock to be used to retrieve current time - expected to be default clock in
  // production.
  const raw_ptr<base::Clock, ExperimentalAsh> clock_;

  // Callback passed from `GlanceablesKeyedService` that creates
  // `request_sender_`.
  const CreateRequestSenderCallback create_request_sender_callback_;

  // Helper class that sends requests, handles retries and authentication.
  std::unique_ptr<google_apis::RequestSender> request_sender_;

  // Timers used to prefetch teacher data.
  base::OneShotTimer teacher_data_prefetch_timer_;

  // Callback which, if set, gets called when a fetch triggered by
  // `teacher_data_prefetch_timer_` completes. The callback can be set using
  // `FireTeacherDataPrefetchTimerIfRunningForTesting()`.
  base::OnceClosure prefetch_callback_;

  // The number of top teacher assignments that are expected to be displayed in
  // the glanceables bubble, and should thus be fresh when returned using the
  // `GlanceablesClassroomClient` interface.
  size_t number_of_assignments_prioritized_for_display_;

  // Available courses for student and teacher roles.
  CourseListState student_courses_;
  CourseListState teacher_courses_;

  // All course work information grouped by course id.
  CourseWorkPerCourse student_course_work_;
  CourseWorkPerCourse teacher_course_work_;

  // Fetch status of all student data.
  FetchStatus student_data_fetch_status_ = FetchStatus::kNotFetched;

  // Whether any of API requests made to fetch student data failed, indicating
  // that student data may not be fully fresh.
  bool student_data_fetch_had_failure_ = false;

  // Pending callbacks awaiting all student data.
  std::list<DataFetchCallback> callbacks_waiting_for_student_data_;

  // Fetch status of all teacher data.
  FetchStatus teacher_data_fetch_status_ = FetchStatus::kNotFetched;

  // Whether any of API requests made to fetch teacher data failed, indicating
  // that teacher data may not be fully fresh.
  bool teacher_data_fetch_had_failure_ = false;

  // Pending callbacks awaiting all teacher data.
  std::list<DataFetchCallback> callbacks_waiting_for_teacher_data_;

  // The next available course work fetch request ID. The IDs will increase
  // monotonically with each new request.
  int next_course_work_request_id_ = 0;

  // In progress course work requests, mapped by the course work request ID.
  base::flat_map<int, std::unique_ptr<CourseWorkRequest>> course_work_requests_;

  base::WeakPtrFactory<GlanceablesClassroomClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_CLASSROOM_CLIENT_IMPL_H_
